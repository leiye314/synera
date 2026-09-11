#include "systems/savemanager.h"
#include "core/gamestate.h"
#include "entity/unit.h"
#include "entity/herodata.h"
#include "entity/equipment.h"
#include "systems/traitsystem.h"
#include <QFile>
#include <QSaveFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <cmath>
#include <limits>

namespace SaveManager {
namespace {
constexpr qint64 MaxBytes = 1024 * 1024;

bool integer(const QJsonObject& object, const char* key, int& out, int low, int high)
{
    const auto value = object.value(QLatin1String(key));
    if (!value.isDouble()) return false;
    const double number = value.toDouble();
    if (!std::isfinite(number) || std::floor(number) != number || number < low || number > high)
        return false;
    out = static_cast<int>(number);
    return true;
}
bool fail(QString& error, const QString& message)
{
    error = message;
    return false;
}
QJsonObject encode(const GameState& state)
{
    const Player& p = state.player();
    QJsonObject root{
        {"version", kVersion}, {"round", state.round()}, {"phase", "Prep"},
        {"player", QJsonObject{{"hp", p.hp()}, {"gold", p.gold()}, {"level", p.level()},
                              {"winStreak", p.winStreak()}, {"lossStreak", p.lossStreak()}}},
        {"random", QJsonObject{{"seed", double(state.random().seed())},
                               {"engine", state.random().encode()}}}
    };
    QJsonArray shop, pool, units;
    for (const auto& slot : state.shop()) shop.append(slot.filled ? heroTypeKey(slot.type) : QString());
    for (auto equipment : state.equipmentPool()) pool.append(equipmentTypeKey(equipment));
    for (const auto& owned : state.units()) {
        const Unit& u = *owned;
        QJsonObject record{
            {"hero", heroTypeKey(u.heroType())}, {"owner", u.owner() == Owner::Player ? "Player" : "Enemy"},
            {"star", u.star()}, {"hp", u.hp()}, {"mana", u.mana()},
            {"equip", u.hasEquipment() ? equipmentTypeKey(u.equipment()) : QString()}
        };
        const int slot = state.bench().slotOf(&u);
        if (slot >= 0) {
            record["place"] = "bench";
            record["slot"] = slot;
        } else {
            record["place"] = "board";
            record["x"] = u.position().x();
            record["y"] = u.position().y();
        }
        units.append(record);
    }
    root["shop"] = shop;
    root["equipmentPool"] = pool;
    root["units"] = units;
    return root;
}

// Decode into an isolated state. The caller commits only after every field passes.
bool decode(const QJsonObject& root, GameState& candidate, QString& error)
{
    int version = 0, round = 0;
    if (!integer(root, "version", version, 1, kVersion))
        return fail(error, QStringLiteral("存档版本不兼容。"));
    if (root.value("phase") != QJsonValue("Prep"))
        return fail(error, QStringLiteral("仅支持准备阶段存档。"));
    if (!integer(root, "round", round, 1, candidate.maxRounds()) || !root.value("player").isObject())
        return fail(error, QStringLiteral("关卡或玩家数据非法。"));

    const QJsonObject player = root.value("player").toObject();
    int hp, gold, level, wins, losses;
    constexpr int MaxInt = std::numeric_limits<int>::max();
    if (!integer(player, "hp", hp, 1, 100) || !integer(player, "gold", gold, 0, MaxInt)
        || !integer(player, "level", level, 1, candidate.player().maxLevel())
        || !integer(player, "winStreak", wins, 0, MaxInt)
        || !integer(player, "lossStreak", losses, 0, MaxInt) || (wins && losses))
        return fail(error, QStringLiteral("玩家字段必须是范围内的整数，连胜与连败不能同时存在。"));
    candidate.player().setHp(hp);
    candidate.player().setGold(gold);
    candidate.player().setLevel(level);
    candidate.player().setWinStreak(wins);
    candidate.player().setLossStreak(losses);
    candidate.setRound(round);
    if (version >= 2) {
        if (!root.value("random").isObject())
            return fail(error, QStringLiteral("缺少随机状态。"));
        const auto random = root.value("random").toObject();
        const auto seedValue = random.value("seed");
        const double seed = seedValue.toDouble(-1);
        if (!seedValue.isDouble() || seed < 0 || seed > std::numeric_limits<quint32>::max()
            || !std::isfinite(seed) || std::floor(seed) != seed
            || !random.value("engine").isString()
            || !candidate.random().restore(static_cast<quint32>(seed), random.value("engine").toString()))
            return fail(error, QStringLiteral("随机状态非法。"));
    } // v1 has no RNG state: migrate with the documented seed 0.

    if (!root.value("shop").isArray() || root.value("shop").toArray().size() != 5
        || !root.value("equipmentPool").isArray() || !root.value("units").isArray())
        return fail(error, QStringLiteral("商店、装备池或单位列表格式非法。"));
    const auto shop = root.value("shop").toArray();
    for (int i = 0; i < shop.size(); ++i) {
        if (!shop[i].isString()) return fail(error, QStringLiteral("商店项必须为字符串。"));
        const QString key = shop[i].toString();
        if (!key.isEmpty()) {
            HeroType hero;
            if (!heroTypeFromKey(key, hero)) return fail(error, QStringLiteral("商店包含未知英雄。"));
            candidate.shop()[i] = {true, hero};
        }
    }
    const auto pool = root.value("equipmentPool").toArray();
    if (pool.size() > 1024) return fail(error, QStringLiteral("装备池过大。"));
    for (const auto& value : pool) {
        EquipmentType type;
        if (!value.isString() || !equipmentTypeFromKey(value.toString(), type))
            return fail(error, QStringLiteral("装备池包含未知装备。"));
        candidate.equipmentPool().append(type);
    }
    const auto units = root.value("units").toArray();
    if (units.size() > level + Bench::SLOTS)
        return fail(error, QStringLiteral("单位数量超过人口与备战槽容量。"));
    for (const auto& value : units) {
        if (!value.isObject()) return fail(error, QStringLiteral("单位必须为对象。"));
        const auto record = value.toObject();
        HeroType hero;
        int star, mana;
        const double hpValue = record.value("hp").toDouble(-1);
        if (!heroTypeFromKey(record.value("hero").toString(), hero)
            || record.value("owner") != QJsonValue("Player")
            || !integer(record, "star", star, 1, 3) || !integer(record, "mana", mana, 0, 1000)
            || !record.value("hp").isDouble() || !std::isfinite(hpValue) || hpValue <= 0 || hpValue > 1000000
            || !record.value("equip").isString())
            return fail(error, QStringLiteral("单位属性或归属非法。"));
        auto owned = HeroData::create(hero, Owner::Player);
        owned->setStar(star);
        const QString equip = record.value("equip").toString();
        if (!equip.isEmpty()) {
            EquipmentType type;
            if (!equipmentTypeFromKey(equip, type)) return fail(error, QStringLiteral("单位装备未知。"));
            owned->setEquipment(type);
        }
        Unit* unit = candidate.addUnit(std::move(owned));
        const auto place = record.value("place").toString();
        int slot, x, y;
        if (place == "bench") {
            if (!integer(record, "slot", slot, 0, Bench::SLOTS - 1) || !candidate.bench().placeAt(slot, unit))
                return fail(error, QStringLiteral("备战槽非法或重复。"));
        } else if (place == "board") {
            if (!integer(record, "x", x, 0, Board::COLS - 1)
                || !integer(record, "y", y, Board::ROWS / 2, Board::ROWS - 1)
                || !candidate.board().placeUnit(unit, {x, y}))
                return fail(error, QStringLiteral("棋盘坐标非法、重复或位于敌方半场。"));
        } else return fail(error, QStringLiteral("单位位置类型非法。"));
    }
    if (candidate.populationUsed() > level) return fail(error, QStringLiteral("上阵人数超过人口。"));
    // Prep is a formation snapshot; transient combat HP, mana and targets are never resumed.
    TraitSystem::applyModifiers(candidate.playerBoardUnits());
    for (Unit* unit : candidate.bench().allUnits()) {
        StatModifiers modifiers;
        if (unit->hasEquipment()) EquipmentData::apply(unit->equipment(), modifiers);
        unit->setModifiers(modifiers);
    }
    for (Unit* unit : candidate.playerRoster()) unit->normalizeForPrep();
    return true;
}
} // namespace

bool save(const GameState& state, const QString& path, QString& errorOut)
{
    errorOut.clear();
    if (state.phase() != Phase::Prep) return fail(errorOut, QStringLiteral("只能在准备阶段存档。"));
    const auto root = encode(state);
    GameState validation;
    if (!decode(root, validation, errorOut)) return false;
    const auto bytes = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (bytes.size() > MaxBytes) return fail(errorOut, QStringLiteral("存档过大。"));
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) return fail(errorOut, file.errorString());
    if (file.write(bytes) != bytes.size()) {
        const QString error = file.errorString();
        file.cancelWriting();
        return fail(errorOut, error);
    }
    if (!file.commit()) return fail(errorOut, file.errorString());
    return true;
}
bool load(GameState& state, const QString& path, QString& errorOut)
{
    errorOut.clear();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return fail(errorOut, file.errorString());
    if (file.size() > MaxBytes) return fail(errorOut, QStringLiteral("存档超过 1 MiB 限制。"));
    const auto bytes = file.read(MaxBytes + 1);
    if (file.error() != QFileDevice::NoError) return fail(errorOut, file.errorString());
    if (bytes.size() > MaxBytes) return fail(errorOut, QStringLiteral("存档过大。"));
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return fail(errorOut, QStringLiteral("JSON 存档损坏。"));
    GameState candidate;
    if (!decode(document.object(), candidate, errorOut)) return false;
    state = std::move(candidate);
    return true;
}
} // namespace SaveManager
