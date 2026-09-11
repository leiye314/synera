#include "core/selftest.h"
#include "core/gamecontroller.h"
#include "core/gamestate.h"
#include "core/board.h"
#include "core/bench.h"
#include "core/levelconfig.h"
#include "entity/unit.h"
#include "entity/herodata.h"
#include "entity/heroes.h"
#include "systems/combatsystem.h"
#include "systems/combatcontext.h"
#include "systems/traitsystem.h"
#include "systems/pathfinder.h"
#include "systems/savemanager.h"
#include "systems/equipmentsystem.h"
#include "systems/shopsystem.h"
#include <limits>

#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTemporaryDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QStringList>
#include <QTextStream>
#include <QTimer>
#include <cmath>
#include <functional>
#include <vector>

namespace {

struct Report {
    QStringList lines;
    int failures = 0;
    void check(bool ok, const QString& name) {
        lines << QStringLiteral("[%1] %2").arg(ok ? QStringLiteral("PASS") : QStringLiteral("FAIL"), name);
        if (!ok) ++failures;
    }
};

bool nearlyEqual(double a, double b, double eps = 0.5) { return std::abs(a - b) <= eps; }

// 棋盘上所有存活单位位置是否互不相同（无重叠）。
bool noOverlap(const GameState& st)
{
    QSet<int> seen;
    for (Unit* u : st.board().allUnits()) {
        if (!u->isAlive()) continue;
        const int key = u->position().y() * Board::COLS + u->position().x();
        if (seen.contains(key)) return false;
        seen.insert(key);
    }
    return true;
}

void testBoardAndBench(Report& r)
{
    GameState st;
    Unit* a = st.addUnit(HeroData::create(HeroType::Warrior, Owner::Player));
    Unit* b = st.addUnit(HeroData::create(HeroType::Mage, Owner::Player));

    r.check(st.board().placeUnit(a, QPoint(3, 6)), QStringLiteral("Board: place into empty cell"));
    r.check(!st.board().placeUnit(b, QPoint(3, 6)), QStringLiteral("Board: reject placing into occupied cell"));
    r.check(st.board().placeUnit(b, QPoint(4, 6)), QStringLiteral("Board: place second unit"));
    r.check(st.board().swapUnits(QPoint(3, 6), QPoint(4, 6)), QStringLiteral("Board: swap two occupied cells"));
    r.check(a->position() == QPoint(4, 6) && b->position() == QPoint(3, 6), QStringLiteral("Board: swap updates positions"));
    r.check(!st.board().moveUnit(QPoint(4, 6), QPoint(3, 6)), QStringLiteral("Board: reject move into occupied"));
    st.board().removeUnit(a);
    r.check(!st.board().hasUnitAt(QPoint(4, 6)), QStringLiteral("Board: remove frees cell"));

    Bench bench;
    st.board().removeUnit(b);
    r.check(bench.addUnit(b) && bench.slotOf(b) == 0, QStringLiteral("Bench: add to first slot"));
    for (int i = 0; i < Bench::SLOTS; ++i) {
        bench.placeAt(i, st.addUnit(HeroData::create(HeroType::Ranger, Owner::Player)));
    }
    // 上一行第 0 槽已被 b 占用，placeAt 会失败；备战区应满。
    r.check(bench.isFull(), QStringLiteral("Bench: becomes full at 8"));
}

void testPathfinder(Report& r)
{
    GameState st;
    Unit* mover = st.addUnit(HeroData::create(HeroType::Warrior, Owner::Player));
    st.board().placeUnit(mover, QPoint(3, 7));

    QPoint step;
    const bool moved = Pathfinder::nextStep(st.board(), QPoint(3, 7), QPoint(3, 4), 1.0, step);
    r.check(moved && step == QPoint(3, 6), QStringLiteral("Pathfinder: steps straight toward target"));

    r.check(!Pathfinder::inAttackRange(QPoint(3, 7), QPoint(3, 4), 1.0)
            && Pathfinder::inAttackRange(QPoint(3, 5), QPoint(3, 4), 1.0),
            QStringLiteral("Pathfinder: range check correct"));

    // 阻挡：在正前方放一个单位，下一步必须绕开。
    Unit* blocker = st.addUnit(HeroData::create(HeroType::Knight, Owner::Player));
    st.board().placeUnit(blocker, QPoint(3, 6));
    QPoint step2;
    const bool moved2 = Pathfinder::nextStep(st.board(), QPoint(3, 7), QPoint(3, 0), 1.0, step2);
    r.check(moved2 && step2 != QPoint(3, 6) && std::abs(step2.x() - 3) + std::abs(step2.y() - 7) == 1,
            QStringLiteral("Pathfinder: routes around blocker"));
}

void testTraitsAndEquipment(Report& r)
{
    // 装备：锁子甲 +150 最大生命，重复重算不叠加。
    auto k = HeroData::create(HeroType::Knight, Owner::Player);
    const double base = k->effectiveMaxHp();
    k->setEquipment(EquipmentType::Armor);
    QVector<Unit*> one{ k.get() };
    TraitSystem::applyModifiers(one);
    const double withArmor = k->effectiveMaxHp();
    TraitSystem::applyModifiers(one); // 再算一次
    const double withArmor2 = k->effectiveMaxHp();
    r.check(nearlyEqual(withArmor, base + 150.0), QStringLiteral("Equipment: armor adds +150 maxHp"));
    r.check(nearlyEqual(withArmor, withArmor2), QStringLiteral("Equipment/Trait: recompute never double-stacks"));

    // 羁绊：两个不同英雄都带战士标签 -> 战士羁绊达到 2 档，+180 HP。
    auto w1 = HeroData::create(HeroType::Warrior, Owner::Player); // Warrior+Guardian
    auto w2 = HeroData::create(HeroType::Assassin, Owner::Player); // Warrior+Ranger
    const double w1base = w1->effectiveMaxHp();
    QVector<Unit*> team{ w1.get(), w2.get() };
    TraitSystem::applyModifiers(team);
    r.check(nearlyEqual(w1->effectiveMaxHp(), w1base + 180.0),
            QStringLiteral("Trait: Warrior 2 grants +180 maxHp (distinct heroes)"));

    // 同名重复不应累计：两个 Warrior（同类型）战士羁绊仍按 1 计，不应触发 2 档。
    auto wA = HeroData::create(HeroType::Warrior, Owner::Player);
    auto wB = HeroData::create(HeroType::Warrior, Owner::Player);
    const double wAbase = wA->effectiveMaxHp();
    QVector<Unit*> dup{ wA.get(), wB.get() };
    TraitSystem::applyModifiers(dup);
    r.check(nearlyEqual(wA->effectiveMaxHp(), wAbase),
            QStringLiteral("Trait: duplicate same-hero does NOT double-count"));
}

void testCombat(Report& r)
{
    GameState st;
    Unit* player = st.addUnit(HeroData::create(HeroType::Warrior, Owner::Player));
    player->setStar(2); // 明显更强
    st.board().placeUnit(player, QPoint(3, 7));
    Unit* enemy = st.addUnit(HeroData::create(HeroType::Warrior, Owner::Enemy));
    st.board().placeUnit(enemy, QPoint(3, 0));

    CombatSystem cs([](const QString&) {});
    cs.begin(st);

    CombatResult result = CombatResult::Ongoing;
    int guard = 0;
    bool overlapOk = true;
    while (result == CombatResult::Ongoing && guard < 2000) {
        result = cs.tick(st);
        if (!noOverlap(st)) overlapOk = false;
        ++guard;
    }
    r.check(result != CombatResult::Ongoing, QStringLiteral("Combat: terminates with a result"));
    r.check(result == CombatResult::PlayerWin, QStringLiteral("Combat: stronger player wins 1v1"));
    r.check(overlapOk, QStringLiteral("Combat: units never overlap on the same cell"));
    r.check(!enemy->isAlive(), QStringLiteral("Combat: losing unit ends up Dead"));
}

void testStarUpViaController(Report& r)
{
    GameController gc;
    gc.newGame();
    for (int i = 0; i < 3; ++i) {
        gc.state().shop()[0] = ShopSlot{ true, HeroType::Warrior };
        gc.buyUnit(0);
    }
    const QVector<Unit*> roster = gc.state().playerRoster();
    r.check(roster.size() == 1, QStringLiteral("StarUp: three 1-stars collapse to a single unit"));
    r.check(!roster.isEmpty() && roster.first()->star() == 2, QStringLiteral("StarUp: merged unit becomes 2-star"));
}

void testSaveLoad(Report& r)
{
    const QString path = QStringLiteral("selftest_save.json");
    QFile::remove(path);

    GameState a;
    a.player().setGold(42);
    a.player().setLevel(5);
    a.player().setHp(77);
    a.setRound(3);
    Unit* u1 = a.addUnit(HeroData::create(HeroType::Mage, Owner::Player));
    u1->setStar(2);
    u1->setEquipment(EquipmentType::Sword);
    a.board().placeUnit(u1, QPoint(2, 6));
    Unit* u2 = a.addUnit(HeroData::create(HeroType::Ranger, Owner::Player));
    a.bench().placeAt(1, u2);

    QString err;
    r.check(SaveManager::save(a, path, err), QStringLiteral("Save: writes file"));

    GameState b;
    r.check(SaveManager::load(b, path, err), QStringLiteral("Load: reads file"));
    r.check(b.player().gold() == 42 && b.player().level() == 5 && b.player().hp() == 77 && b.round() == 3,
            QStringLiteral("Load: player/round restored"));
    r.check(b.units().size() == 2, QStringLiteral("Load: unit count restored"));
    r.check(b.board().hasUnitAt(QPoint(2, 6)), QStringLiteral("Load: board placement restored"));

    // 损坏存档不得破坏当前状态。
    QFile bad(path);
    if (bad.open(QIODevice::WriteOnly | QIODevice::Truncate)) { bad.write("{ this is : not valid json"); bad.close(); }
    GameState c;
    c.player().setGold(999);
    QString err2;
    const bool loadedBad = SaveManager::load(c, path, err2);
    r.check(!loadedBad && c.player().gold() == 999,
            QStringLiteral("Load: corrupted file fails safely without altering state"));

    GameState d;
    d.player().setGold(123);
    const bool loadedMissing = SaveManager::load(d, QStringLiteral("definitely_missing_file.json"), err2);
    r.check(!loadedMissing && d.player().gold() == 123,
            QStringLiteral("Load: missing file fails safely"));

    QFile::remove(path);
}

// 写入一份合法基准存档，供结构篡改测试复用。
bool writeBaselineSave(const QString& path)
{
    GameState a;
    a.player().setGold(55);
    a.player().setLevel(4);
    a.player().setHp(88);
    a.setRound(2);
    a.shop()[0] = ShopSlot{ true, HeroType::Warrior };
    a.equipmentPool().append(EquipmentType::Sword);
    Unit* u = a.addUnit(HeroData::create(HeroType::Mage, Owner::Player));
    a.board().placeUnit(u, QPoint(1, 6));
    QString err;
    return SaveManager::save(a, path, err);
}

// 读取 JSON 存档、应用修改函数后写回。
bool mutateSaveJson(const QString& path, const std::function<void(QJsonObject&)>& mutate)
{
    QFile in(path);
    if (!in.open(QIODevice::ReadOnly)) {
        return false;
    }
    QJsonParseError perr;
    const QJsonDocument doc = QJsonDocument::fromJson(in.readAll(), &perr);
    in.close();
    if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }
    QJsonObject root = doc.object();
    mutate(root);
    QFile out(path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    out.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    out.close();
    return true;
}

void testSaveValidation(Report& r)
{
    const QString path = QStringLiteral("selftest_save_validate.json");
    QFile::remove(path);
    r.check(writeBaselineSave(path), QStringLiteral("SaveValidate: baseline save written"));

    auto rejectWithoutMutatingState = [&](const std::function<void(QJsonObject&)>& mutate,
                                          const QString& name) {
        r.check(writeBaselineSave(path) && mutateSaveJson(path, mutate), name + QStringLiteral(" (mutate)"));
        GameState st;
        st.player().setGold(777);
        st.player().setHp(66);
        QString err;
        const bool ok = SaveManager::load(st, path, err);
        r.check(!ok && st.player().gold() == 777 && st.player().hp() == 66,
                name);
    };

    rejectWithoutMutatingState(
        [](QJsonObject& root) { root["phase"] = QStringLiteral("Battle"); },
        QStringLiteral("SaveValidate: invalid phase rejected, state unchanged"));

    rejectWithoutMutatingState(
        [](QJsonObject& root) {
            QJsonArray shop = root.value(QStringLiteral("shop")).toArray();
            shop.removeLast();
            root["shop"] = shop;
        },
        QStringLiteral("SaveValidate: shop size != 5 rejected, state unchanged"));

    rejectWithoutMutatingState(
        [](QJsonObject& root) { root.remove(QStringLiteral("equipmentPool")); },
        QStringLiteral("SaveValidate: missing equipmentPool rejected, state unchanged"));

    rejectWithoutMutatingState(
        [](QJsonObject& root) { root["equipmentPool"] = QJsonObject(); },
        QStringLiteral("SaveValidate: non-array equipmentPool rejected, state unchanged"));

    rejectWithoutMutatingState(
        [](QJsonObject& root) {
            QJsonArray units = root.value(QStringLiteral("units")).toArray();
            QJsonObject u = units.first().toObject();
            u["owner"] = QStringLiteral("Neutral");
            units[0] = u;
            root["units"] = units;
        },
        QStringLiteral("SaveValidate: invalid owner rejected, state unchanged"));

    // 篡改测试会修改同一文件，读回前恢复合法基准存档。
    r.check(writeBaselineSave(path),
            QStringLiteral("SaveValidate: baseline restored before valid load"));
    GameState loaded;
    QString err;
    r.check(SaveManager::load(loaded, path, err),
            QStringLiteral("SaveValidate: valid baseline still loads"));
    r.check(loaded.player().gold() == 55 && loaded.player().hp() == 88 && loaded.round() == 2,
            QStringLiteral("SaveValidate: baseline player/round restored"));

    QFile::remove(path);
}

// 通过真实 QTimer 事件循环驱动一轮战斗到结算返回准备阶段。
// 返回 true 表示正常结算回到准备阶段；false 表示游戏结束或超时。
bool runOneRoundLoop(GameController& gc)
{
    QEventLoop loop;
    bool resolved = false;
    bool over = false;
    auto c1 = QObject::connect(&gc, &GameController::phaseChanged, [&](Phase p) {
        if (p == Phase::Prep) { resolved = true; loop.quit(); }
    });
    auto c2 = QObject::connect(&gc, &GameController::gameOver, [&](bool) {
        over = true; loop.quit();
    });
    QTimer::singleShot(30000, &loop, &QEventLoop::quit); // 安全超时
    gc.startCombat();
    loop.exec();
    QObject::disconnect(c1);
    QObject::disconnect(c2);
    return resolved && !over;
}

void testMultiRoundViaController(Report& r)
{
    GameController gc;
    gc.newGame();

    // 放两个压倒性的三星影刺，确保每关都稳定取胜（修复后的语义里只有胜利才推进关卡），
    // 便于自动验证“真实 QTimer 战斗 + 多关推进”的完整流程。
    Unit* hero = gc.state().addUnit(HeroData::create(HeroType::Assassin, Owner::Player));
    hero->setStar(3);
    const int hid = hero->id();
    gc.state().board().placeUnit(hero, QPoint(3, 7));
    Unit* hero2 = gc.state().addUnit(HeroData::create(HeroType::Assassin, Owner::Player));
    hero2->setStar(3);
    gc.state().board().placeUnit(hero2, QPoint(4, 7));

    const bool ok1 = runOneRoundLoop(gc);
    r.check(ok1, QStringLiteral("Round1: resolves and returns to Prep"));
    Unit* h1 = gc.state().findUnit(hid);
    r.check(h1 && h1->isAlive() && h1->position() == QPoint(3, 7),
            QStringLiteral("Round1: player unit revived at its snapshot position"));
    r.check(gc.state().enemyBoardUnits().isEmpty(),
            QStringLiteral("Round1: enemies cleared after resolve"));

    const bool ok2 = runOneRoundLoop(gc);
    r.check(ok2, QStringLiteral("Round2: resolves and returns to Prep"));
    Unit* h2 = gc.state().findUnit(hid);
    r.check(h2 && h2->isAlive(),
            QStringLiteral("Round2: roster stable across rounds (no dangling/double-free)"));
    r.check(gc.state().round() >= 3, QStringLiteral("Round counter advances across rounds"));
}

// ---- 关卡推进与整局胜负的确定性回归 ----

// 纯结算决策函数：穷举验证四类状态转移，与随机战斗解耦。
void testRoundDecisionPure(Report& r)
{
    using RO = GameController::RoundOutcome;
    constexpr int kMax = 10;

    // 普通关胜利 -> 进入下一关。
    r.check(GameController::decideRoundOutcome(true, false, 3, kMax) == RO::Advance,
            QStringLiteral("Decision: win on a normal stage advances"));
    // 最后一关胜利 -> 整局胜利。
    r.check(GameController::decideRoundOutcome(true, false, kMax, kMax) == RO::Victory,
            QStringLiteral("Decision: win on the final stage wins the run"));
    // 普通关失败但仍有生命 -> 重试本关。
    r.check(GameController::decideRoundOutcome(false, false, 3, kMax) == RO::Retry,
            QStringLiteral("Decision: loss with hp remaining retries the same stage"));
    // 最后一关失败但仍有生命 -> 重试本关（绝不触发胜利）。
    r.check(GameController::decideRoundOutcome(false, false, kMax, kMax) == RO::Retry,
            QStringLiteral("Decision: loss on the final stage never triggers victory"));
    // 生命归零 -> 整局失败（即使发生在最后一关或同时判胜）。
    r.check(GameController::decideRoundOutcome(false, true, 5, kMax) == RO::Defeat,
            QStringLiteral("Decision: zero hp ends the run in defeat"));
    r.check(GameController::decideRoundOutcome(true, true, kMax, kMax) == RO::Defeat,
            QStringLiteral("Decision: defeat takes precedence over final-stage win"));
}

// 借助 applyRoundResult 在不跑随机战斗的前提下，确定性验证失败/胜利的真实状态转移。
void testRoundProgressionViaController(Report& r)
{
    // --- 普通关失败：生命下降、关卡不变、返回准备、敌人与临时状态清理 ---
    {
        GameController gc;
        gc.newGame();
        gc.state().setRound(3);
        Unit* hero = gc.state().addUnit(HeroData::create(HeroType::Warrior, Owner::Player));
        const int hid = hero->id();
        gc.state().board().placeUnit(hero, QPoint(2, 6));
        gc.capturePlayerFormation();

        // 制造一场“战斗残留”：敌人在场、我方单位带战斗期临时状态。
        Unit* enemy = gc.state().addUnit(HeroData::create(HeroType::Mage, Owner::Enemy));
        gc.state().board().placeUnit(enemy, QPoint(2, 1));
        hero->setShield(120.0);
        hero->setTargetId(enemy->id());
        hero->setHp(hero->effectiveMaxHp() / 2.0);

        const int hpBefore = gc.state().player().hp();
        gc.applyRoundResult(false);

        r.check(gc.state().player().hp() < hpBefore,
                QStringLiteral("LossRetry: player hp drops after a stage loss"));
        r.check(gc.state().round() == 3,
                QStringLiteral("LossRetry: stage number unchanged after loss"));
        r.check(gc.state().phase() == Phase::Prep,
                QStringLiteral("LossRetry: returns to Prep so the same stage can be retried"));
        r.check(gc.state().enemyBoardUnits().isEmpty(),
                QStringLiteral("LossRetry: enemies cleared on retry"));
        Unit* h = gc.state().findUnit(hid);
        r.check(h && h->isAlive() && h->position() == QPoint(2, 6),
                QStringLiteral("LossRetry: player unit revived at its formation position"));
        r.check(h && h->shield() == 0.0 && h->targetId() == -1
                    && h->state() == CombatState::Idle && h->mana() == 0
                    && nearlyEqual(h->hp(), h->effectiveMaxHp(), 0.001),
                QStringLiteral("LossRetry: no leftover combat state (shield/target/stun/hp)"));

        // 重试后可以再次开始同一关（仍为第 3 关）。
        gc.startCombat();
        r.check(gc.state().phase() == Phase::Combat && gc.state().round() == 3,
                QStringLiteral("LossRetry: same stage can be started again"));
    }

    // --- 普通关胜利：关卡编号加一 ---
    {
        GameController gc;
        gc.newGame();
        gc.state().setRound(4);
        Unit* hero = gc.state().addUnit(HeroData::create(HeroType::Warrior, Owner::Player));
        gc.state().board().placeUnit(hero, QPoint(3, 6));
        gc.capturePlayerFormation();

        gc.applyRoundResult(true);
        r.check(gc.state().round() == 5,
                QStringLiteral("WinAdvance: winning a normal stage increments the stage"));
        r.check(gc.state().phase() == Phase::Prep,
                QStringLiteral("WinAdvance: returns to Prep for the next stage"));
    }

    // --- 最后一关失败且仍有生命：不触发胜利 ---
    {
        GameController gc;
        gc.newGame();
        const int last = gc.state().maxRounds();
        gc.state().setRound(last);
        gc.state().player().setHp(80);
        Unit* hero = gc.state().addUnit(HeroData::create(HeroType::Warrior, Owner::Player));
        gc.state().board().placeUnit(hero, QPoint(3, 6));
        gc.capturePlayerFormation();
        Unit* enemy = gc.state().addUnit(HeroData::create(HeroType::Mage, Owner::Enemy));
        gc.state().board().placeUnit(enemy, QPoint(3, 1));

        bool overEmitted = false;
        bool overWon = false;
        QObject::connect(&gc, &GameController::gameOver, [&](bool won) { overEmitted = true; overWon = won; });

        gc.applyRoundResult(false);
        r.check(!overEmitted,
                QStringLiteral("FinalLoss: final-stage loss never triggers gameOver(true)"));
        r.check(gc.state().round() == last && gc.state().phase() == Phase::Prep,
                QStringLiteral("FinalLoss: stays on final stage and returns to Prep"));
        Q_UNUSED(overWon);
    }

    // --- 最后一关胜利：触发 gameOver(true) ---
    {
        GameController gc;
        gc.newGame();
        const int last = gc.state().maxRounds();
        gc.state().setRound(last);
        Unit* hero = gc.state().addUnit(HeroData::create(HeroType::Warrior, Owner::Player));
        gc.state().board().placeUnit(hero, QPoint(3, 6));
        gc.capturePlayerFormation();

        bool overEmitted = false;
        bool overWon = false;
        QObject::connect(&gc, &GameController::gameOver, [&](bool won) { overEmitted = true; overWon = won; });

        gc.applyRoundResult(true);
        r.check(overEmitted && overWon,
                QStringLiteral("FinalWin: beating the final stage triggers gameOver(true)"));
    }

    // --- 生命归零：触发 gameOver(false) ---
    {
        GameController gc;
        gc.newGame();
        gc.state().setRound(2);
        gc.state().player().setHp(1);
        Unit* hero = gc.state().addUnit(HeroData::create(HeroType::Warrior, Owner::Player));
        gc.state().board().placeUnit(hero, QPoint(3, 6));
        gc.capturePlayerFormation();
        Unit* enemy = gc.state().addUnit(HeroData::create(HeroType::Mage, Owner::Enemy));
        gc.state().board().placeUnit(enemy, QPoint(3, 1));

        bool overEmitted = false;
        bool overWon = true;
        QObject::connect(&gc, &GameController::gameOver, [&](bool won) { overEmitted = true; overWon = won; });

        gc.applyRoundResult(false);
        r.check(overEmitted && !overWon,
                QStringLiteral("Defeat: zero hp triggers gameOver(false)"));
    }
}

// ---- 护盾伤害结算（DamageResult：区分护盾吸收与生命损失，不改战斗数值）----
void testShieldDamage(Report& r)
{
    // 选用 0 护甲、0 百分比减伤的英雄（法师），使“结算伤害 == 原始伤害”，便于精确断言。
    // 1) 无护盾：生命正常扣除，吸收为 0。
    {
        auto u = HeroData::create(HeroType::Mage, Owner::Enemy);
        u->beginCombat();
        const double maxHp = u->effectiveMaxHp();
        const DamageResult dr = u->applyDamage(100.0);
        r.check(nearlyEqual(dr.shieldAbsorbed, 0.0) && nearlyEqual(dr.hpLoss, 100.0)
                    && nearlyEqual(u->hp(), maxHp - 100.0) && !dr.lethal,
                QStringLiteral("Shield: no shield deducts hp normally"));
    }
    // 2) 护盾完全吸收：生命不变，吸收量等于结算伤害。
    {
        auto u = HeroData::create(HeroType::Mage, Owner::Enemy);
        u->beginCombat();
        const double hpBefore = u->hp();
        u->setShield(200.0);
        const DamageResult dr = u->applyDamage(100.0);
        r.check(nearlyEqual(dr.shieldAbsorbed, 100.0) && nearlyEqual(dr.hpLoss, 0.0)
                    && nearlyEqual(u->hp(), hpBefore) && nearlyEqual(u->shield(), 100.0),
                QStringLiteral("Shield: full absorb keeps hp, absorbs correctly"));
    }
    // 3) 护盾部分吸收：护盾归零，余下伤害扣生命。
    {
        auto u = HeroData::create(HeroType::Mage, Owner::Enemy);
        u->beginCombat();
        const double hpBefore = u->hp();
        u->setShield(40.0);
        const DamageResult dr = u->applyDamage(100.0);
        r.check(nearlyEqual(dr.shieldAbsorbed, 40.0) && nearlyEqual(dr.hpLoss, 60.0)
                    && nearlyEqual(u->shield(), 0.0) && nearlyEqual(u->hp(), hpBefore - 60.0),
                QStringLiteral("Shield: partial absorb drains shield then hp"));
    }
    // 4) 死亡判定不受影响：致死一击仍致死，hpLoss 等于剩余生命。
    {
        auto u = HeroData::create(HeroType::Mage, Owner::Enemy);
        u->beginCombat();
        u->setHp(50.0);
        const DamageResult dr = u->applyDamage(100.0);
        r.check(dr.lethal && !u->isAlive() && u->state() == CombatState::Dead
                    && nearlyEqual(dr.hpLoss, 50.0),
                QStringLiteral("Shield: lethal hit still kills (death rule unchanged)"));
    }
}

// ---- 装备穿戴业务规则（点击穿戴与拖拽穿戴共用 EquipmentSystem::equip）----
void testEquipRules(Report& r)
{
    GameState st; // 默认即准备阶段
    Unit* ally = st.addUnit(HeroData::create(HeroType::Warrior, Owner::Player));
    Unit* enemy = st.addUnit(HeroData::create(HeroType::Knight, Owner::Enemy));
    st.equipmentPool().append(EquipmentType::Sword);
    st.equipmentPool().append(EquipmentType::Armor);

    QString err;
    // 成功穿戴：装备池减一，单位获得装备。
    const int before = st.equipmentPool().size();
    const bool ok = EquipmentSystem::equip(st, ally, 0, err);
    r.check(ok && ally->hasEquipment() && st.equipmentPool().size() == before - 1,
            QStringLiteral("Equip: valid target equips and consumes one from pool"));

    // 每个单位最多一件：再次穿戴被拒，装备池不变。
    const int afterOne = st.equipmentPool().size();
    const bool second = EquipmentSystem::equip(st, ally, 0, err);
    r.check(!second && st.equipmentPool().size() == afterOne,
            QStringLiteral("Equip: rejects second equipment (one per unit), pool unchanged"));

    // 敌方单位不可穿戴，装备池不变。
    const bool onEnemy = EquipmentSystem::equip(st, enemy, 0, err);
    r.check(!onEnemy && !enemy->hasEquipment() && st.equipmentPool().size() == afterOne,
            QStringLiteral("Equip: rejects enemy unit, pool unchanged"));

    // 非法装备下标被拒，装备池不变。
    Unit* ally2 = st.addUnit(HeroData::create(HeroType::Mage, Owner::Player));
    const bool badIndex = EquipmentSystem::equip(st, ally2, 99, err);
    r.check(!badIndex && !ally2->hasEquipment() && st.equipmentPool().size() == afterOne,
            QStringLiteral("Equip: rejects invalid pool index, pool unchanged"));

    // 仅准备阶段允许：战斗阶段拒绝穿戴，装备池不变。
    st.setPhase(Phase::Combat);
    const bool inCombat = EquipmentSystem::equip(st, ally2, 0, err);
    r.check(!inCombat && !ally2->hasEquipment() && st.equipmentPool().size() == afterOne,
            QStringLiteral("Equip: rejects equipping outside Prep phase, pool unchanged"));
}

// ---- 骑士铁壁护盾：有界刷新，禁止无限叠加 ----
void testKnightIronWallShield(Report& r)
{
    GameState st;
    Unit* knight = st.addUnit(HeroData::create(HeroType::Knight, Owner::Player));
    st.board().placeUnit(knight, QPoint(3, 7));
    knight->beginCombat();

    const QVector<Unit*> boardUnits = st.board().allUnits();
    std::vector<Unit*> participants(boardUnits.begin(), boardUnits.end());
    CombatContext ctx(participants, st.board(), [](const QString&) {});

    const double skillShield = 0.45 * knight->effectiveMaxHp();

    knight->castSkill(ctx);
    r.check(nearlyEqual(knight->shield(), skillShield),
            QStringLiteral("KnightShield: first cast produces correct shield"));

    knight->castSkill(ctx);
    r.check(nearlyEqual(knight->shield(), skillShield),
            QStringLiteral("KnightShield: recast without consumption does not stack to 2x"));

    knight->setShield(skillShield * 0.5);
    knight->castSkill(ctx);
    r.check(nearlyEqual(knight->shield(), skillShield),
            QStringLiteral("KnightShield: recast after partial use restores to skill value"));

    knight->normalizeForPrep();
    r.check(knight->shield() == 0.0,
            QStringLiteral("KnightShield: shield cleared when returning to prep"));

    knight->beginCombat();
    knight->setShield(200.0);
    const double hpBefore = knight->hp();
    const DamageResult dr = knight->applyDamage(100.0);
    r.check(dr.shieldAbsorbed > 0.0 && nearlyEqual(dr.hpLoss, 0.0)
                && nearlyEqual(knight->hp(), hpBefore),
            QStringLiteral("KnightShield: DamageResult unchanged under bounded shield"));
}

// ---- 战斗日志：事件级输出，不改战斗数值 ----
void testCombatLogging(Report& r)
{
    // 1) 普攻扣血但不输出“攻击”过程日志。
    {
        GameState st;
        Unit* player = st.addUnit(HeroData::create(HeroType::Warrior, Owner::Player));
        st.board().placeUnit(player, QPoint(3, 7));
        Unit* enemy = st.addUnit(HeroData::create(HeroType::Mage, Owner::Enemy));
        st.board().placeUnit(enemy, QPoint(3, 6));
        const double hpBefore = enemy->hp();

        QStringList logs;
        CombatSystem cs([&logs](const QString& msg) { logs.append(msg); });
        cs.begin(st);
        cs.tick(st);

        bool hasAttackLog = false;
        for (const QString& line : logs) {
            if (line.contains(QStringLiteral("攻击"))) {
                hasAttackLog = true;
                break;
            }
        }
        r.check(enemy->hp() < hpBefore && !hasAttackLog,
                QStringLiteral("CombatLog: normal attack damages but no attack process log"));
    }

    // 2) 普攻致死输出一次死亡日志。
    {
        GameState st;
        Unit* player = st.addUnit(HeroData::create(HeroType::Warrior, Owner::Player));
        player->setStar(3);
        st.board().placeUnit(player, QPoint(3, 7));
        Unit* enemy = st.addUnit(HeroData::create(HeroType::Mage, Owner::Enemy));
        st.board().placeUnit(enemy, QPoint(3, 6));

        QStringList logs;
        CombatSystem cs([&logs](const QString& msg) { logs.append(msg); });
        cs.begin(st);
        enemy->setHp(1.0);
        cs.tick(st);

        int deathLogs = 0;
        for (const QString& line : logs) {
            if (line.contains(QStringLiteral("普通攻击")) && line.contains(QStringLiteral("击败"))) {
                ++deathLogs;
            }
        }
        r.check(!enemy->isAlive() && deathLogs == 1,
                QStringLiteral("CombatLog: normal attack kill logs death once"));
    }

    // 3) 技能日志含我方/敌方标签。
    {
        GameState st;
        Unit* assassin = st.addUnit(HeroData::create(HeroType::Assassin, Owner::Player));
        st.board().placeUnit(assassin, QPoint(3, 7));
        Unit* priest = st.addUnit(HeroData::create(HeroType::Priest, Owner::Enemy));
        st.board().placeUnit(priest, QPoint(3, 0));
        assassin->beginCombat();
        priest->beginCombat();

        QStringList logs;
        const QVector<Unit*> boardUnits = st.board().allUnits();
        std::vector<Unit*> participants(boardUnits.begin(), boardUnits.end());
        CombatContext ctx(participants, st.board(),
                          [&logs](const QString& msg) { logs.append(msg); });
        assassin->castSkill(ctx);

        r.check(!logs.isEmpty() && logs.last().contains(QStringLiteral("我方"))
                    && logs.last().contains(QStringLiteral("敌方")),
                QStringLiteral("CombatLog: skill lines use ally/enemy labels"));
    }

    // 4) 影袭显示目标与实际伤害。
    {
        GameState st;
        Unit* assassin = st.addUnit(HeroData::create(HeroType::Assassin, Owner::Enemy));
        st.board().placeUnit(assassin, QPoint(3, 0));
        Unit* priest = st.addUnit(HeroData::create(HeroType::Priest, Owner::Player));
        st.board().placeUnit(priest, QPoint(3, 7));
        assassin->beginCombat();
        priest->beginCombat();

        QStringList logs;
        const QVector<Unit*> boardUnits = st.board().allUnits();
        std::vector<Unit*> participants(boardUnits.begin(), boardUnits.end());
        CombatContext ctx(participants, st.board(),
                          [&logs](const QString& msg) { logs.append(msg); });
        assassin->castSkill(ctx);

        r.check(!logs.isEmpty() && logs.last().contains(QStringLiteral("影袭"))
                    && logs.last().contains(QStringLiteral("我方祭司"))
                    && logs.last().contains(QStringLiteral("生命损失")),
                QStringLiteral("CombatLog: assassin skill shows target and hp loss"));
    }

    // 5) 冰霜新星一条日志含命中名称与汇总伤害。
    {
        GameState st;
        Unit* mage = st.addUnit(HeroData::create(HeroType::Mage, Owner::Enemy));
        st.board().placeUnit(mage, QPoint(3, 0));
        Unit* knight = st.addUnit(HeroData::create(HeroType::Knight, Owner::Player));
        st.board().placeUnit(knight, QPoint(3, 1));
        Unit* priest = st.addUnit(HeroData::create(HeroType::Priest, Owner::Player));
        st.board().placeUnit(priest, QPoint(4, 1));
        mage->beginCombat();
        knight->beginCombat();
        priest->beginCombat();
        mage->setTargetId(knight->id());

        QStringList logs;
        const QVector<Unit*> boardUnits = st.board().allUnits();
        std::vector<Unit*> participants(boardUnits.begin(), boardUnits.end());
        CombatContext ctx(participants, st.board(),
                          [&logs](const QString& msg) { logs.append(msg); });
        mage->castSkill(ctx);

        int novaLogs = 0;
        QString novaLine;
        for (const QString& line : logs) {
            if (line.contains(QStringLiteral("冰霜新星"))) {
                ++novaLogs;
                novaLine = line;
            }
        }
        r.check(novaLogs == 1 && novaLine.contains(QStringLiteral("我方骑士"))
                    && novaLine.contains(QStringLiteral("我方祭司"))
                    && novaLine.contains(QStringLiteral("生命损失")),
                QStringLiteral("CombatLog: frost nova aggregates victims in one line"));
    }

    // 6) 祭司日志显示实际恢复量（非理论值）。
    {
        GameState st;
        Unit* priest = st.addUnit(HeroData::create(HeroType::Priest, Owner::Player));
        st.board().placeUnit(priest, QPoint(3, 7));
        Unit* ally = st.addUnit(HeroData::create(HeroType::Mage, Owner::Player));
        st.board().placeUnit(ally, QPoint(4, 7));
        priest->beginCombat();
        ally->beginCombat();
        const double maxHp = ally->effectiveMaxHp();
        ally->setHp(maxHp - 50.0);

        QStringList logs;
        const QVector<Unit*> boardUnits = st.board().allUnits();
        std::vector<Unit*> participants(boardUnits.begin(), boardUnits.end());
        CombatContext ctx(participants, st.board(),
                          [&logs](const QString& msg) { logs.append(msg); });
        priest->castSkill(ctx);

        r.check(!logs.isEmpty() && logs.last().contains(QStringLiteral("实际恢复 50 HP"))
                    && !logs.last().contains(QStringLiteral("实际恢复 220 HP")),
                QStringLiteral("CombatLog: priest heal shows actual recovery amount"));
    }

    // 7) 同一单位死亡日志只出现一次（多段技能）。
    {
        GameState st;
        Unit* ranger = st.addUnit(HeroData::create(HeroType::Ranger, Owner::Player));
        ranger->setStar(3);
        st.board().placeUnit(ranger, QPoint(3, 7));
        Unit* enemy = st.addUnit(HeroData::create(HeroType::Mage, Owner::Enemy));
        st.board().placeUnit(enemy, QPoint(3, 0));
        ranger->beginCombat();
        enemy->beginCombat();
        enemy->setHp(5.0);
        ranger->setTargetId(enemy->id());

        QStringList logs;
        const QVector<Unit*> boardUnits = st.board().allUnits();
        std::vector<Unit*> participants(boardUnits.begin(), boardUnits.end());
        CombatContext ctx(participants, st.board(),
                          [&logs](const QString& msg) { logs.append(msg); });
        ranger->castSkill(ctx);

        int deathLogs = 0;
        for (const QString& line : logs) {
            if (line.contains(QStringLiteral("敌方法师")) && line.contains(QStringLiteral("击败"))) {
                ++deathLogs;
            }
        }
        r.check(!enemy->isAlive() && deathLogs == 1,
                QStringLiteral("CombatLog: multi-hit skill logs victim death only once"));
    }
}

// ---- 十关敌人生成配置：合法性与强度单调增长 ----
void testLevelConfig(Report& r)
{
    constexpr int kMaxSlots = Board::ROWS / 2 * Board::COLS;

    for (int round = 1; round <= LevelConfig::stageCount(); ++round) {
        const LevelConfig::StageConfig cfg = LevelConfig::configForRound(round);
        r.check(LevelConfig::isValid(cfg, kMaxSlots),
                QStringLiteral("LevelConfig: round %1 config is valid").arg(round));
        r.check(cfg.enemyCount <= kMaxSlots,
                QStringLiteral("LevelConfig: round %1 enemy count fits board").arg(round));
    }

    double prevStrength = 0.0;
    for (int round = 1; round <= LevelConfig::stageCount(); ++round) {
        const double s = LevelConfig::estimatedStrength(LevelConfig::configForRound(round));
        r.check(s >= prevStrength - 0.001,
                QStringLiteral("LevelConfig: round %1 strength non-decreasing").arg(round));
        prevStrength = s;
    }

    const double s7 = LevelConfig::estimatedStrength(LevelConfig::configForRound(7));
    const double s10 = LevelConfig::estimatedStrength(LevelConfig::configForRound(10));
    r.check(s10 > s7,
            QStringLiteral("LevelConfig: round 10 stronger than round 7"));
}


QString shopFingerprint(const GameState& state)
{
    QString result;
    for (const auto& slot : state.shop()) result += slot.filled ? heroTypeKey(slot.type) : "-";
    return result;
}
QString enemyFingerprint(const GameState& state)
{
    QString result;
    for (const Unit* unit : state.enemyBoardUnits())
        result += heroTypeKey(unit->heroType()) + QString::number(unit->star())
            + QString::number(unit->hp()) + QString::number(unit->position().x());
    return result;
}
void testRandomReplay(Report& r)
{
    RandomStream known(5489);
    r.check(known.bounded(100) == 12, QStringLiteral("Seed: MT19937 reference first draw and bounded mapping"));
    GameState a(12345), b(12345), different(54321);
    bool same = true, differs = false, inRange = true;
    for (int roll = 0; roll < 40; ++roll) {
        ShopSystem::refill(a); ShopSystem::refill(b); ShopSystem::refill(different);
        same = same && shopFingerprint(a) == shopFingerprint(b);
        differs = differs || shopFingerprint(a) != shopFingerprint(different);
        const auto eqA = EquipmentSystem::randomEquipment(a);
        const auto eqB = EquipmentSystem::randomEquipment(b);
        same = same && eqA == eqB;
        const int draw = different.random().bounded(7);
        inRange = inRange && draw >= 0 && draw < 7;
    }
    r.check(same, QStringLiteral("Seed: repeated shops and equipment draws replay exactly"));
    r.check(differs && inRange, QStringLiteral("Seed: different seeds diverge with bounded output"));
    const QString path = "seed-save.json";
    QString error;
    r.check(SaveManager::save(a, path, error) && SaveManager::load(b, path, error),
            QStringLiteral("Seed: persist advanced random stream"));
    bool resumed = a.random().seed() == b.random().seed();
    for (int i = 0; i < 100; ++i) resumed = resumed && a.random().bounded(1000) == b.random().bounded(1000);
    r.check(resumed, QStringLiteral("Seed: 100 subsequent draws survive save/load"));
    auto play = [](quint32 seed) {
        GameController controller(nullptr, seed);
        QStringList transcript;
        QObject::connect(&controller, &GameController::logMessage, &controller,
                         [&](const QString& text) { transcript.append(text); });
        controller.newGame();
        const auto initialShop = shopFingerprint(controller.state());
        controller.newGame();
        transcript.append(initialShop == shopFingerprint(controller.state()) ? "reset-ok" : "reset-failed");
        auto* hero = controller.state().addUnit(HeroData::create(HeroType::Assassin, Owner::Player));
        hero->setStar(3);
        controller.state().board().placeUnit(hero, {3, 7});
        controller.startCombat();
        transcript.append(enemyFingerprint(controller.state()));
        for (int i = 0; i < 610 && controller.isCombatActive(); ++i)
            QMetaObject::invokeMethod(&controller, "onCombatTick", Qt::DirectConnection);
        transcript.append(shopFingerprint(controller.state()));
        for (auto eq : controller.state().equipmentPool()) transcript.append(equipmentTypeKey(eq));
        transcript.append(QString::number(controller.state().round()));
        return transcript;
    };
    const auto first = play(12345);
    r.check(first == play(12345), QStringLiteral("Seed: enemies, skill log, outcome, loot and next shop replay"));
    r.check(first.contains("reset-ok"), QStringLiteral("Seed: new game resets the original seed"));
    r.check(first != play(54321), QStringLiteral("Seed: enemy and combat trace changes with seed"));

    GameController original(nullptr, 91), restored(nullptr, 999);
    original.newGame();
    original.refreshShop();
    auto* hero = original.state().addUnit(HeroData::create(HeroType::Warrior, Owner::Player));
    original.state().board().placeUnit(hero, {3, 7});
    bool loadOk = original.saveGame(path) && restored.loadGame(path);
    original.startCombat(); restored.startCombat();
    r.check(loadOk && enemyFingerprint(original.state()) == enemyFingerprint(restored.state()),
            QStringLiteral("Seed: loaded controller generates the same next enemies"));
}
void testAuditRegressions(Report& r)
{
    GameState state;
    auto* unit = state.addUnit(HeroData::create(HeroType::Warrior, Owner::Player));
    state.board().placeUnit(unit, {2, 6});
    state.board().placeUnit(unit, {4, 7});
    r.check(!state.board().hasUnitAt({2, 6}) && state.board().allUnits().size() == 1,
            QStringLiteral("Board: relocating a unit leaves one occupancy"));
    r.check(!state.board().placeUnit(unit, {-1, 8}) && unit->position() == QPoint(4, 7),
            QStringLiteral("Board: rejected placement preserves source"));
    state.board().clear();
    r.check(unit->position() == QPoint(-1, -1) && state.board().allUnits().isEmpty(),
            QStringLiteral("Board: clear invalidates positions"));
    r.check(state.bench().slotOf(nullptr) == -1, QStringLiteral("Bench: null is not an empty-slot occupant"));
    QPoint next(99, 99);
    r.check(!Pathfinder::nextStep(state.board(), {-1, 0}, {3, 3}, 1, next)
         && !Pathfinder::nextStep(state.board(), {0, 0}, {8, 0}, 1, next)
         && !Pathfinder::nextStep(state.board(), {0, 0}, {3, 3}, -1, next)
         && !Pathfinder::nextStep(state.board(), {0, 0}, {3, 3}, std::nan(""), next)
         && next == QPoint(99, 99), QStringLiteral("BFS: invalid coordinates/range rejected without indexing"));
    state.player().setGold(std::numeric_limits<int>::max());
    state.player().addGold(100);
    r.check(state.player().gold() == std::numeric_limits<int>::max(),
            QStringLiteral("Economy: positive income cannot overflow gold"));
    GameController orphan;
    orphan.newGame();
    auto* unplaced = orphan.state().addUnit(HeroData::create(HeroType::Mage, Owner::Player));
    r.check(!orphan.requestPlace(unplaced->id(), PlacementTarget::toBench(0)),
            QStringLiteral("Placement: unregistered source rejected"));
    for (bool won : {false, true}) {
        GameController economy;
        economy.newGame();
        economy.state().player().setGold(59);
        if (won) economy.state().player().setWinStreak(4);
        else economy.state().player().setLossStreak(4);
        economy.capturePlayerFormation();
        economy.applyRoundResult(won);
        r.check(economy.state().player().gold() == 59 + (won ? 6 : 3) + 5 + 3,
                won ? "Economy: win base plus capped interest plus streak"
                    : "Economy: loss base plus capped interest plus streak");
    }
    GameController capacity;
    capacity.newGame();
    capacity.state().player().setGold(100);
    const int upgradeCost = capacity.state().player().upgradeCost();
    capacity.upgradePopulation();
    r.check(capacity.state().populationCap() == 4 && capacity.state().player().gold() == 100 - upgradeCost,
            "Economy: population upgrade charges displayed cost");
    bool placed = true;
    for (int i = 0; i < 5; ++i) {
        auto* u = capacity.state().addUnit(HeroData::create(HeroType::Warrior, Owner::Player));
        capacity.state().bench().placeAt(i, u);
        const bool ok = capacity.requestPlace(u->id(), PlacementTarget::toBoard({i, 6}));
        placed = placed && (ok == (i < 4));
    }
    r.check(placed && capacity.state().populationUsed() == 4 && capacity.state().bench().count() == 1,
            "Placement: population cap rejects excess unit without losing it");
    Unit* reserve = capacity.state().bench().unitAt(4);
    Unit* deployed = capacity.state().board().unitAt({0, 6});
    r.check(capacity.requestPlace(reserve->id(), PlacementTarget::toBoard({0, 6}))
         && capacity.state().bench().unitAt(4) == deployed
         && capacity.state().populationUsed() == 4,
            "Placement: bench-board swap works at full population");
    GameState stage;
    stage.setRound(10);
    auto* enemy = stage.addUnit(HeroData::create(HeroType::Warrior, Owner::Enemy));
    stage.board().placeUnit(enemy, {3, 2});
    CombatSystem combat({});
    combat.begin(stage);
    r.check(nearlyEqual(enemy->effectiveMaxHp(), 650 * 1.6, 0.001)
         && nearlyEqual(enemy->effectiveAttackDamage(), 55 * 1.4, 0.001)
         && nearlyEqual(enemy->effectiveSkillDamagePct(), 0.32, 0.001),
            QStringLiteral("Combat: stage 10 multipliers survive trait initialization"));
    combat.begin(stage);
    r.check(nearlyEqual(enemy->effectiveMaxHp(), 650 * 1.6, 0.001),
            QStringLiteral("Combat: reinitializing does not stack stage multipliers"));
    auto* target = stage.addUnit(HeroData::create(HeroType::Knight, Owner::Player));
    stage.board().placeUnit(target, {3, 7});
    combat.begin(stage);
    CombatResult result = CombatResult::Ongoing;
    for (int i = 0; i < 610 && result == CombatResult::Ongoing; ++i) result = combat.tick(stage);
    r.check(result != CombatResult::Ongoing, QStringLiteral("Combat: missing logger is safe through completion"));
}
void testStrictSaves(Report& r)
{
    const QString path = "strict-save.json";
    QString error;
    auto reject = [&](const QString& name, const std::function<void(QJsonObject&)>& mutation) {
        const bool fixture = writeBaselineSave(path) && mutateSaveJson(path, mutation);
        GameState target(42);
        target.player().setGold(777);
        auto* survivor = target.addUnit(HeroData::create(HeroType::Knight, Owner::Player));
        target.board().placeUnit(survivor, {2, 5});
        const auto rng = target.random().encode();
        const bool loaded = SaveManager::load(target, path, error);
        r.check(fixture && !loaded && !error.isEmpty() && target.player().gold() == 777
             && target.board().unitAt({2, 5}) == survivor && target.random().encode() == rng,
                "SaveStrict: " + name);
    };
    const auto playerField = [](const char* key, QJsonValue value) {
        return [=](QJsonObject& root) { auto p = root["player"].toObject(); p[key] = value; root["player"] = p; };
    };
    reject("fractional player integer", playerField("gold", 2.5));
    reject("overflow integer", playerField("gold", 1e30));
    reject("negative gold", playerField("gold", -1));
    reject("invalid population", playerField("level", 10));
    reject("zero HP", playerField("hp", 0));
    reject("out-of-range HP", playerField("hp", 101));
    reject("wrong streak type", playerField("winStreak", "2"));
    reject("simultaneous streaks", [](QJsonObject& root) {
        auto p = root["player"].toObject(); p["winStreak"] = 2; p["lossStreak"] = 1; root["player"] = p;
    });
    reject("round past final stage", [](QJsonObject& root) { root["round"] = 11; });
    reject("fractional version", [](QJsonObject& root) { root["version"] = 1.5; });
    reject("combat snapshot", [](QJsonObject& root) { root["phase"] = "Combat"; });
    reject("wrong shop value type", [](QJsonObject& root) { auto a = root["shop"].toArray(); a[0] = 123; root["shop"] = a; });
    reject("missing RNG", [](QJsonObject& root) { root.remove("random"); });
    reject("invalid RNG", [](QJsonObject& root) { root["random"] = QJsonObject{{"seed", 1}, {"engine", "broken"}}; });
    const auto unitField = [](const char* key, QJsonValue value) {
        return [=](QJsonObject& root) {
            auto a = root["units"].toArray(); auto u = a[0].toObject(); u[key] = value; a[0] = u; root["units"] = a;
        };
    };
    reject("fractional star", unitField("star", 1.5));
    reject("missing star", [](QJsonObject& root) {
        auto a = root["units"].toArray(); auto u = a[0].toObject(); u.remove("star"); a[0] = u; root["units"] = a;
    });
    reject("fractional coordinate", unitField("x", 2.5));
    reject("enemy half", unitField("y", 0));
    reject("enemy owner in Prep", unitField("owner", "Enemy"));
    reject("negative unit HP", unitField("hp", -1));
    reject("wrong equipment type", unitField("equip", 4));
    reject("duplicate occupancy", [](QJsonObject& root) { auto a = root["units"].toArray(); a.append(a[0]); root["units"] = a; });
    reject("too many items", [](QJsonObject& root) {
        QJsonArray a; for (int i = 0; i < 1025; ++i) a.append("Sword"); root["equipmentPool"] = a;
    });
    writeBaselineSave(path);
    mutateSaveJson(path, [](QJsonObject& root) { root["version"] = 1; root.remove("random"); });
    GameState migrated(99);
    r.check(SaveManager::load(migrated, path, error) && migrated.random().seed() == 0,
            QStringLiteral("SaveStrict: valid v1 Prep save migrates with seed 0"));
    writeBaselineSave(path);
    QFile original(path);
    original.open(QIODevice::ReadOnly);
    const auto before = original.readAll();
    original.close();
    GameState invalid;
    invalid.setPhase(Phase::Combat);
    r.check(!SaveManager::save(invalid, path, error), QStringLiteral("SaveAtomic: reject non-Prep save at system boundary"));
    original.open(QIODevice::ReadOnly);
    r.check(original.readAll() == before, QStringLiteral("SaveAtomic: rejected save preserves existing file bytes"));
    original.close();
    GameState good;
    r.check(!SaveManager::save(good, "missing-directory/save.json", error) && !error.isEmpty(),
            QStringLiteral("SaveAtomic: write failure propagates"));
    good.player().setGold(987);
    r.check(SaveManager::save(good, path, error) && SaveManager::load(migrated, path, error)
         && migrated.player().gold() == 987, QStringLiteral("SaveAtomic: replacing an existing save round-trips"));
    QFile huge(path);
    huge.open(QIODevice::WriteOnly);
    huge.write(QByteArray(1024 * 1024 + 1, ' ')); huge.close();
    r.check(!SaveManager::load(migrated, path, error) && migrated.player().gold() == 987,
            QStringLiteral("SaveStrict: oversized file rejected without state mutation"));
}

} // namespace

int runSelfTest(const QString& reportPath)
{
    const QString absoluteReport = QFileInfo(reportPath).absoluteFilePath();
    QTemporaryDir scratch;
    if (!scratch.isValid()) return 1;
    struct WorkingDirectoryGuard {
        QString previous = QDir::currentPath();
        ~WorkingDirectoryGuard() { QDir::setCurrent(previous); }
    } guard;
    if (!QDir::setCurrent(scratch.path())) return 1;
    Report r;
    testBoardAndBench(r);
    testPathfinder(r);
    testTraitsAndEquipment(r);
    testCombat(r);
    testStarUpViaController(r);
    testSaveLoad(r);
    testSaveValidation(r);
    testMultiRoundViaController(r);
    testRoundDecisionPure(r);
    testRoundProgressionViaController(r);
    testShieldDamage(r);
    testKnightIronWallShield(r);
    testCombatLogging(r);
    testLevelConfig(r);
    testEquipRules(r);
    testRandomReplay(r);
    testAuditRegressions(r);
    testStrictSaves(r);

    r.lines << QString();
    // 汇总行统一使用英文 ASCII，避免不同终端/编码下出现乱码。
    r.lines << QStringLiteral("Total: %1 cases, %2 failures.")
                   .arg(r.lines.size() - 1).arg(r.failures);

    const QString text = r.lines.join(QStringLiteral("\n"));

    QFile f(absoluteReport);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QTextStream ts(&f);
        ts << text << "\n";
        ts.flush();
        if (ts.status() != QTextStream::Ok || f.error() != QFileDevice::NoError) return 1;
        f.close();
    } else {
        QTextStream(stderr) << "Cannot write self-test report.\n";
        return 1;
    }
    QTextStream(stdout) << text << "\n";
    return r.failures == 0 ? 0 : 1;
}
