#include "core/gamecontroller.h"
#include "core/levelconfig.h"
#include "systems/combatsystem.h"
#include "systems/traitsystem.h"
#include "systems/shopsystem.h"
#include "systems/equipmentsystem.h"
#include "systems/savemanager.h"
#include "entity/herodata.h"
#include "entity/equipment.h"
#include "entity/unit.h"

#include <QTimer>
#include <algorithm>

namespace {
int streakBonusFor(int streak)
{
    if (streak >= 5) return 3;
    if (streak >= 3) return 2;
    if (streak >= 2) return 1;
    return 0;
}
}

GameController::GameController(QObject* parent, quint32 seed)
    : QObject(parent)
    , m_state(seed)
    , m_combatTimer(new QTimer(this))
    , m_gameEnded(false)
{
    m_combat = std::make_unique<CombatSystem>([this](const QString& msg) { log(msg); });
    m_combatTimer->setInterval(m_combat->tickIntervalMs());
    connect(m_combatTimer, &QTimer::timeout, this, &GameController::onCombatTick);
}

GameController::~GameController() = default;

bool GameController::isCombatActive() const
{
    return m_state.phase() == Phase::Combat;
}

void GameController::reportFormationLocked()
{
    if (m_gameEnded) {
        log(QStringLiteral("游戏已经结束，请开始新游戏。"));
        return;
    }
    switch (m_state.phase()) {
    case Phase::Combat:
        log(QStringLiteral("战斗阶段不能调整阵容。"));
        break;
    case Phase::Resolve:
        log(QStringLiteral("正在结算，请稍候。"));
        break;
    case Phase::Prep:
        break; // 准备阶段可正常编辑，无需提示
    }
}

QString GameController::traitSummary() const
{
    const QVector<TraitSystem::TraitStatus> list = TraitSystem::status(m_state.playerBoardUnits());
    if (list.isEmpty()) {
        return QStringLiteral("<i style='color:#9a9a9a'>（暂无羁绊：上阵不同英雄以激活）</i>");
    }

    QStringList lines;
    for (const TraitSystem::TraitStatus& s : list) {
        const QVector<int> tiers = TraitSystem::thresholds(s.trait);
        const int maxTier = tiers.isEmpty() ? 0 : tiers.last();
        // 下一个尚未达到的档位（用作分母与“还差几名”）。
        int nextTier = maxTier;
        for (int t : tiers) {
            if (t > s.count) { nextTier = t; break; }
        }
        const bool active = s.activeThreshold > 0;
        const int denom = (s.count < maxTier) ? nextTier : maxTier;
        const QString head = QStringLiteral("%1 %2/%3")
                                 .arg(traitName(s.trait)).arg(s.count).arg(denom);

        QString line;
        if (active) {
            const QString eff = TraitSystem::effectTextForTier(s.trait, s.activeThreshold);
            line = QStringLiteral("<span style='color:#8fe28f'>✓ %1：%2</span>").arg(head, eff);
            if (s.count < maxTier) {
                line += QStringLiteral("<span style='color:#9a9a9a'>（距下一档还差 %1 名）</span>")
                            .arg(nextTier - s.count);
            }
        } else {
            line = QStringLiteral("<span style='color:#cfcfcf'>%1：还差 %2 名</span>")
                       .arg(head).arg(nextTier - s.count);
        }
        lines << line;
    }
    return lines.join(QStringLiteral("<br>"));
}

// ---------------- 命令 ----------------

void GameController::newGame()
{
    m_combatTimer->stop();
    m_state.resetToNewGame();
    m_gameEnded = false;
    m_state.setRound(1);
    m_playerSnapshot.clear();
    startPrepRound(false);
    log(QStringLiteral("新游戏开始 · 随机种子 %1").arg(m_state.random().seed()));
}

void GameController::buyUnit(int shopSlot)
{
    if (m_gameEnded || m_state.phase() != Phase::Prep) {
        log(QStringLiteral("当前无法购买。"));
        return;
    }
    QString err;
    Unit* unit = ShopSystem::buy(m_state, shopSlot, err);
    if (!unit) {
        log(err);
        return;
    }
    log(QStringLiteral("购买了 %1。").arg(unit->name()));
    checkStarUps();
    recomputePlayerModifiers();
    emitAll();
}

void GameController::refreshShop()
{
    if (m_gameEnded || m_state.phase() != Phase::Prep) {
        return;
    }
    constexpr int kRefreshCost = 2;
    if (!m_state.player().spendGold(kRefreshCost)) {
        log(QStringLiteral("金币不足，无法刷新商店。"));
        return;
    }
    ShopSystem::refill(m_state);
    log(QStringLiteral("刷新商店（-%1 金币）。").arg(kRefreshCost));
    emitAll();
}

void GameController::upgradePopulation()
{
    if (m_gameEnded || m_state.phase() != Phase::Prep) {
        return;
    }
    Player& p = m_state.player();
    const int cost = p.upgradeCost();
    if (cost < 0) {
        log(QStringLiteral("人口已达上限。"));
        return;
    }
    if (!p.spendGold(cost)) {
        log(QStringLiteral("金币不足，无法升级人口（需 %1）。").arg(cost));
        return;
    }
    p.setLevel(p.level() + 1);
    log(QStringLiteral("人口上限提升至 %1（-%2 金币）。").arg(p.level()).arg(cost));
    emitAll();
}

void GameController::sellUnit(int unitId)
{
    if (m_gameEnded || m_state.phase() != Phase::Prep) {
        return;
    }
    Unit* unit = m_state.findUnit(unitId);
    if (!unit || unit->owner() != Owner::Player) {
        return;
    }
    const int refund = unit->cost() * unit->star();
    const QString name = unit->name();
    // 若单位带装备，装备回到装备池。
    if (unit->hasEquipment()) {
        EquipmentSystem::addToPool(m_state, unit->equipment());
    }
    m_state.removeUnit(unit);
    m_state.player().addGold(refund);
    log(QStringLiteral("卖出 %1（+%2 金币）。").arg(name).arg(refund));
    recomputePlayerModifiers();
    emitAll();
}

bool GameController::requestPlace(int unitId, const PlacementTarget& target)
{
    if (!canEditFormation()) {
        reportFormationLocked();
        return false;
    }
    Unit* unit = m_state.findUnit(unitId);
    if (!unit || unit->owner() != Owner::Player) {
        return false;
    }

    Board& board = m_state.board();
    Bench& bench = m_state.bench();

    const bool onBoard = board.isValidPosition(unit->position())
                         && board.unitAt(unit->position()) == unit;
    const int benchSlotSrc = bench.slotOf(unit);
    if (!onBoard && benchSlotSrc < 0) return false;

    bool ok = false;

    if (target.kind == PlacementTarget::Board) {
        const QPoint to = target.boardPos;
        if (!board.isValidPosition(to)) {
            return false; // 拖出棋盘：回弹
        }
        if (!board.isPlayerHalf(to)) {
            log(QStringLiteral("只能把单位放在我方半场。"));
            return false;
        }
        Unit* occupant = board.unitAt(to);
        if (occupant == unit) {
            return true; // 原地
        }
        if (onBoard) {
            const QPoint from = unit->position();
            if (!occupant) {
                ok = board.moveUnit(from, to);
            } else if (occupant->owner() == Owner::Player) {
                ok = board.swapUnits(from, to);
            } else {
                log(QStringLiteral("无法与敌方单位交换。"));
                return false;
            }
        } else { // 来自备战区
            if (!occupant) {
                if (m_state.populationUsed() >= m_state.populationCap()) {
                    log(QStringLiteral("已达人口上限（%1），无法继续上阵。").arg(m_state.populationCap()));
                    return false;
                }
                bench.removeUnit(unit);
                ok = board.placeUnit(unit, to);
            } else if (occupant->owner() == Owner::Player) {
                // 备战单位 <-> 棋盘单位 交换（人口不变）
                board.removeUnit(occupant);
                bench.removeUnit(unit);
                ok = board.placeUnit(unit, to);
                bench.placeAt(benchSlotSrc, occupant);
            } else {
                return false;
            }
        }
    } else { // Bench
        const int slot = target.benchSlot;
        if (!bench.isValidSlot(slot)) {
            return false;
        }
        Unit* benchOcc = bench.unitAt(slot);
        if (benchOcc == unit) {
            return true;
        }
        if (onBoard) {
            const QPoint from = unit->position();
            if (!benchOcc) {
                board.removeUnit(unit);
                ok = bench.placeAt(slot, unit);
            } else {
                // 棋盘单位 <-> 备战单位 交换
                bench.removeAt(slot);
                board.removeUnit(unit);
                ok = bench.placeAt(slot, unit) && board.placeUnit(benchOcc, from);
            }
        } else { // bench -> bench
            if (!benchOcc) {
                bench.removeUnit(unit);
                ok = bench.placeAt(slot, unit);
            } else {
                bench.removeAt(benchSlotSrc);
                bench.removeAt(slot);
                ok = bench.placeAt(slot, unit) && bench.placeAt(benchSlotSrc, benchOcc);
            }
        }
    }

    if (ok) {
        recomputePlayerModifiers();
        emitAll();
    }
    return ok;
}

bool GameController::equipUnit(int unitId, int poolIndex)
{
    if (m_gameEnded) {
        return false;
    }
    Unit* unit = m_state.findUnit(unitId);
    QString err;
    if (!EquipmentSystem::equip(m_state, unit, poolIndex, err)) {
        log(err);
        return false;
    }
    log(QStringLiteral("%1 装备了 %2。").arg(unit->name(), EquipmentData::name(unit->equipment())));
    recomputePlayerModifiers();
    emitAll();
    return true;
}

void GameController::reportEquipDropMissed()
{
    log(QStringLiteral("请把装备拖到我方单位身上才能穿戴（装备未消耗）。"));
}

// ---------------- 战斗 ----------------

void GameController::startCombat()
{
    if (m_gameEnded || m_state.phase() != Phase::Prep) {
        return;
    }
    if (m_state.playerBoardUnits().isEmpty()) {
        log(QStringLiteral("请先上阵至少一个单位再开始战斗。"));
        return;
    }

    // 记录我方阵型，战斗后复活归位。
    capturePlayerFormation();

    spawnEnemies(m_state.round());

    m_state.setPhase(Phase::Combat);
    emit phaseChanged(Phase::Combat);
    log(QStringLiteral("第 %1 关战斗开始！").arg(m_state.round()));

    m_combat->begin(m_state);
    m_combatTimer->start();
    emit stateChanged();
}

void GameController::onCombatTick()
{
    if (!isCombatActive()) return;
    const CombatResult result = m_combat->tick(m_state);
    emit stateChanged();
    if (result != CombatResult::Ongoing) {
        m_combatTimer->stop();
        resolveCombat(static_cast<int>(result));
    }
}

GameController::RoundOutcome GameController::decideRoundOutcome(
    bool playerWon, bool playerDefeated, int finishedRound, int maxRounds)
{
    // 生命归零优先：整局失败（即使发生在最后一关）。
    if (playerDefeated) {
        return RoundOutcome::Defeat;
    }
    if (playerWon) {
        // 仅当“击败最后一关”才整局胜利；其余胜利推进下一关。
        return (finishedRound >= maxRounds) ? RoundOutcome::Victory : RoundOutcome::Advance;
    }
    // 失败但仍有生命：保持当前关，返回准备阶段重试。
    // 注意：最后一关失败也走这里，绝不会触发整局胜利。
    return RoundOutcome::Retry;
}

void GameController::capturePlayerFormation()
{
    m_playerSnapshot.clear();
    for (Unit* u : m_state.playerBoardUnits()) {
        m_playerSnapshot.insert(u->id(), u->position());
    }
}

void GameController::resolveCombat(int resultInt)
{
    const CombatResult result = static_cast<CombatResult>(resultInt);
    applyRoundResult(result == CombatResult::PlayerWin);
}

void GameController::applyRoundResult(bool playerWon)
{
    m_combatTimer->stop();
    m_state.setPhase(Phase::Resolve);
    emit phaseChanged(Phase::Resolve);
    Player& p = m_state.player();
    const int finishedRound = m_state.round();

    // 结算前统计存活敌人（用于失败掉血）。清理战场前必须先统计。
    int survivingEnemies = 0;
    for (Unit* u : m_state.enemyBoardUnits()) {
        if (u->isAlive()) {
            ++survivingEnemies;
        }
    }

    if (playerWon) {
        p.recordWin();
        log(QStringLiteral("第 %1 关：胜利！").arg(finishedRound));
        // 胜利按原规则概率掉落装备。
        if (m_state.random().bounded(100) < 60) {
            const EquipmentType eq = EquipmentSystem::randomEquipment(m_state);
            EquipmentSystem::addToPool(m_state, eq);
            log(QStringLiteral("战利品：%1 进入装备栏。").arg(EquipmentData::name(eq)));
        }
    } else {
        p.recordLoss();
        const int hpLoss = std::min(15, 3 + survivingEnemies);
        p.setHp(p.hp() - hpLoss);
        log(QStringLiteral("第 %1 关：失败，损失 %2 点生命（剩余 %3）。")
                .arg(finishedRound).arg(hpLoss).arg(p.hp()));
    }

    // 经济结算：胜利较高基础收入 + 利息 + 连胜奖励；失败较低基础收入 + 利息 + 连败奖励。
    const int base = playerWon ? 6 : 3;
    const int interest = std::min(p.gold() / 10, 5);
    const int streak = playerWon ? p.winStreak() : p.lossStreak();
    const int streakBonus = streakBonusFor(streak);
    const int income = base + interest + streakBonus;
    p.addGold(income);
    log(QStringLiteral("收入 +%1（基础 %2 / 利息 %3 / %4%5 +%6）。")
            .arg(income).arg(base).arg(interest)
            .arg(playerWon ? QStringLiteral("连胜") : QStringLiteral("连败"))
            .arg(streak).arg(streakBonus));

    // 清理战场：删除全部敌人，将我方单位复活并归位到战前快照位置。
    // 配合下方 normalizeForPrep()，可彻底清除死亡状态、目标指针、护盾、临时 Buff 与占用，
    // 因此同一关失败重试时不会遗留上一场的任何战斗状态。
    m_state.removeEnemyUnits();
    Board& board = m_state.board();
    for (Unit* u : m_state.playerRoster()) {
        board.removeUnit(u); // 清掉战斗期间的临时位置
    }
    for (auto it = m_playerSnapshot.begin(); it != m_playerSnapshot.end(); ++it) {
        Unit* u = m_state.findUnit(it.key());
        if (u) {
            board.placeUnit(u, it.value());
        }
    }
    normalizePlayerRosterForPrep();
    recomputePlayerModifiers();

    // 判定整局胜负与关卡推进。
    const RoundOutcome outcome =
        decideRoundOutcome(playerWon, p.isDefeated(), finishedRound, m_state.maxRounds());
    switch (outcome) {
    case RoundOutcome::Defeat:
        m_gameEnded = true;
        log(QStringLiteral("生命归零，游戏结束。"));
        emit stateChanged();
        emit gameOver(false);
        return;
    case RoundOutcome::Victory:
        m_gameEnded = true;
        log(QStringLiteral("击败全部预设关卡（共 %1 关），最终胜利！").arg(m_state.maxRounds()));
        emit stateChanged();
        emit gameOver(true);
        return;
    case RoundOutcome::Advance:
        m_state.setRound(finishedRound + 1);
        log(QStringLiteral("第 %1 关通过，进入第 %2 关。").arg(finishedRound).arg(finishedRound + 1));
        startPrepRound(false); // 收入已在上面结算，避免重复
        return;
    case RoundOutcome::Retry:
        log(QStringLiteral("第 %1 关失败，整理阵容后可再次挑战本关。").arg(finishedRound));
        startPrepRound(false); // 关卡编号保持不变
        return;
    }
}

// ---------------- 存档 ----------------

bool GameController::saveGame(const QString& path)
{
    if (m_state.phase() != Phase::Prep) {
        log(QStringLiteral("只能在准备阶段存档。"));
        return false;
    }
    QString err;
    if (!SaveManager::save(m_state, path, err)) {
        log(QStringLiteral("存档失败：%1").arg(err));
        return false;
    }
    log(QStringLiteral("已存档到 %1。").arg(path));
    return true;
}

bool GameController::loadGame(const QString& path)
{
    QString err;
    // 读档失败不得破坏当前状态：先读到临时对象，成功后再替换。
    if (!SaveManager::load(m_state, path, err)) {
        log(QStringLiteral("读档失败：%1（当前进度未改变）").arg(err));
        return false;
    }
    m_combatTimer->stop();
    m_gameEnded = false;
    m_playerSnapshot.clear();
    recomputePlayerModifiers();
    log(QStringLiteral("已从 %1 读档。").arg(path));
    emit phaseChanged(m_state.phase());
    emitAll();
    return true;
}

// ---------------- 内部辅助 ----------------

void GameController::startPrepRound(bool giveIncome)
{
    m_state.setPhase(Phase::Prep);
    ShopSystem::refill(m_state);

    if (giveIncome) {
        Player& p = m_state.player();
        const int interest = std::min(p.gold() / 10, 5);
        const int income = 5 + interest;
        p.addGold(income);
        log(QStringLiteral("准备阶段收入 +%1（含利息 %2）。").arg(income).arg(interest));
    }

    normalizePlayerRosterForPrep();
    recomputePlayerModifiers();
    emit phaseChanged(Phase::Prep);
    emitAll();
}

void GameController::recomputePlayerModifiers()
{
    // 上阵单位：装备 + 当前生效羁绊（从零重建，绝不叠加）。
    TraitSystem::applyModifiers(m_state.playerBoardUnits());
    // 备战单位：只享受装备，不触发羁绊。
    for (Unit* u : m_state.bench().allUnits()) {
        StatModifiers m;
        if (u->hasEquipment()) {
            EquipmentData::apply(u->equipment(), m);
        }
        u->setModifiers(m);
    }
    // 准备阶段同步当前生命与最大生命。
    if (m_state.phase() == Phase::Prep) {
        for (Unit* u : m_state.playerRoster()) {
            u->setHp(u->effectiveMaxHp());
        }
    }
}

void GameController::normalizePlayerRosterForPrep()
{
    for (Unit* u : m_state.playerRoster()) {
        u->normalizeForPrep();
    }
}

void GameController::checkStarUps()
{
    // 反复扫描，支持级联（三个二星 -> 三星）。
    bool merged = true;
    while (merged) {
        merged = false;
        const QVector<Unit*> roster = m_state.playerRoster();
        // 按 (heroType, star) 分组。
        for (int typeInt = 0; typeInt < 6 && !merged; ++typeInt) {
            for (int star = 1; star <= 2 && !merged; ++star) {
                QVector<Unit*> group;
                for (Unit* u : roster) {
                    if (static_cast<int>(u->heroType()) == typeInt && u->star() == star) {
                        group.append(u);
                    }
                }
                if (group.size() >= 3) {
                    // 保留最近获得（id 最大）者作为合成结果，其余删除。
                    std::sort(group.begin(), group.end(),
                              [](Unit* a, Unit* b) { return a->id() > b->id(); });
                    Unit* keeper = group.at(0);
                    Unit* drop1 = group.at(1);
                    Unit* drop2 = group.at(2);
                    // 被合成单位若带装备，回收到装备池。
                    if (drop1->hasEquipment()) EquipmentSystem::addToPool(m_state, drop1->equipment());
                    if (drop2->hasEquipment()) EquipmentSystem::addToPool(m_state, drop2->equipment());
                    m_state.removeUnit(drop1);
                    m_state.removeUnit(drop2);
                    keeper->setStar(star + 1);
                    log(QStringLiteral("%1 合成为 %2 星！").arg(keeper->name()).arg(star + 1));
                    merged = true;
                }
            }
        }
    }
}

void GameController::spawnEnemies(int round)
{
    // 敌人格位顺序：前排居中优先，再向两翼与后排扩展。
    static const int colOrder[] = { 3, 4, 2, 5, 1, 6, 0, 7 };
    static const int rowOrder[] = { 3, 2, 1, 0 };

    const LevelConfig::StageConfig cfg = LevelConfig::configForRound(round);
    const int count = cfg.enemyCount;
    const QVector<HeroData::HeroInfo>& catalog = HeroData::catalog();

    int placed = 0;
    for (int r = 0; r < 4 && placed < count; ++r) {
        for (int c = 0; c < 8 && placed < count; ++c) {
            const QPoint cell(colOrder[c], rowOrder[r]);
            if (m_state.board().hasUnitAt(cell)) {
                continue;
            }

            HeroType type = HeroType::Warrior;
            if (cfg.heroPool.isEmpty()) {
                const int pick = m_state.random().bounded(static_cast<int>(catalog.size()));
                type = catalog.at(pick).type;
            } else {
                const int pick = m_state.random().bounded(static_cast<int>(cfg.heroPool.size()));
                type = cfg.heroPool.at(pick);
            }

            Unit* enemy = m_state.addUnit(HeroData::create(type, Owner::Enemy));
            enemy->setStar(LevelConfig::starForSlot(cfg, placed));

            // CombatSystem composes stage scaling with traits at battle initialization.
            m_state.board().placeUnit(enemy, cell);
            ++placed;
        }
    }
    log(QStringLiteral("敌方出现 %1 个单位。").arg(placed));
}

void GameController::emitAll(const QString& logMsg)
{
    if (!logMsg.isEmpty()) {
        log(logMsg);
    }
    emit stateChanged();
}
