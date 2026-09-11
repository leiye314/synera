#include "systems/combatsystem.h"
#include "systems/combatcontext.h"
#include "systems/pathfinder.h"
#include "systems/traitsystem.h"
#include "core/gamestate.h"
#include "core/levelconfig.h"
#include "entity/unit.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr int kTickMs = 100;
constexpr int kManaPerAttack = 10;
constexpr int kMoveIntervalTicks = 2;       // 每 200ms 移动一格
constexpr int kMaxCombatTicks = 600;        // 60s 安全上限，避免僵持

int attackIntervalTicks(const Unit* u)
{
    const int ticks = static_cast<int>(std::round(u->effectiveAttackIntervalSec() * 1000.0 / kTickMs));
    return std::max(2, ticks);
}
}

CombatSystem::CombatSystem(std::function<void(const QString&)> logger)
    : m_logger(std::move(logger))
    , m_elapsedTicks(0)
    , m_maxTicks(kMaxCombatTicks)
{}

void CombatSystem::begin(GameState& state)
{
    m_elapsedTicks = 0;
    m_participants.clear();

    // 双方上阵单位都参战；备战区不参战。
    for (Unit* u : state.playerBoardUnits()) {
        m_participants.push_back(u);
    }
    for (Unit* u : state.enemyBoardUnits()) {
        m_participants.push_back(u);
    }

    // 战斗开始前，按各自阵容重算羁绊/装备修正（敌方也享受其羁绊）。
    TraitSystem::applyModifiers(state.playerBoardUnits());
    TraitSystem::applyModifiers(state.enemyBoardUnits());
    const auto stage = LevelConfig::configForRound(state.round());
    for (Unit* enemy : state.enemyBoardUnits()) {
        StatModifiers mods = enemy->modifiers();
        mods.pctMaxHp += stage.hpMul - 1.0;
        mods.flatAttackDamage += (enemy->effectiveAttackDamage() - mods.flatAttackDamage) * (stage.adMul - 1.0);
        mods.skillDamagePct += stage.skillMul - 1.0;
        enemy->setModifiers(mods);
    }

    for (Unit* u : m_participants) {
        u->beginCombat();
    }

    // 稳定的处理顺序：按 id。
    std::sort(m_participants.begin(), m_participants.end(),
              [](Unit* a, Unit* b) { return a->id() < b->id(); });
}

Unit* CombatSystem::acquireTarget(GameState& state, Unit* unit) const
{
    Q_UNUSED(state);
    Unit* best = nullptr;
    double bestDist2 = 1e18;

    for (Unit* other : m_participants) {
        if (!other || !other->isAlive() || other->owner() == unit->owner()) {
            continue;
        }
        const QPoint a = unit->position();
        const QPoint b = other->position();
        const double dx = a.x() - b.x();
        const double dy = a.y() - b.y();
        const double d2 = dx * dx + dy * dy;

        bool better = false;
        if (d2 < bestDist2 - 1e-9) {
            better = true;
        } else if (std::abs(d2 - bestDist2) <= 1e-9 && best) {
            // 平局决胜：更低 HP → 更靠左(x 小) → 更靠下(y 大)。
            if (other->hp() < best->hp() - 1e-9) {
                better = true;
            } else if (std::abs(other->hp() - best->hp()) <= 1e-9) {
                if (b.x() < best->position().x()) {
                    better = true;
                } else if (b.x() == best->position().x() && b.y() > best->position().y()) {
                    better = true;
                }
            }
        }
        if (better) {
            best = other;
            bestDist2 = d2;
        }
    }
    return best;
}

CombatResult CombatSystem::evaluate(GameState& state) const
{
    bool playerAlive = false;
    bool enemyAlive = false;
    for (Unit* u : m_participants) {
        if (!u->isAlive()) {
            continue;
        }
        if (u->owner() == Owner::Player) playerAlive = true;
        else enemyAlive = true;
    }
    Q_UNUSED(state);
    if (!enemyAlive && !playerAlive) {
        return CombatResult::PlayerLose; // 同归于尽按失败处理
    }
    if (!enemyAlive) return CombatResult::PlayerWin;
    if (!playerAlive) return CombatResult::PlayerLose;
    return CombatResult::Ongoing;
}

CombatResult CombatSystem::tick(GameState& state)
{
    Board& board = state.board();
    ++m_elapsedTicks;

    // 1) 清理已死亡单位的棋盘占位。
    for (Unit* u : m_participants) {
        if (!u->isAlive() && board.unitAt(u->position()) == u) {
            board.removeUnit(u);
        }
    }

    CombatContext ctx(m_participants, board, m_logger);

    // 收集移动意图：unit -> 目标格。
    std::vector<std::pair<Unit*, QPoint>> moveIntents;

    for (Unit* unit : m_participants) {
        if (!unit->isAlive()) {
            continue;
        }

        // 冷却递减
        if (unit->attackCooldownTicks() > 0) unit->setAttackCooldownTicks(unit->attackCooldownTicks() - 1);
        if (unit->moveCooldownTicks() > 0) unit->setMoveCooldownTicks(unit->moveCooldownTicks() - 1);

        // 眩晕：本 tick 无法行动
        if (unit->stunTicks() > 0) {
            unit->setStunTicks(unit->stunTicks() - 1);
            unit->setState(CombatState::Idle);
            continue;
        }

        // 索敌：无目标或目标已死则重新索敌
        Unit* target = ctx.unitById(unit->targetId());
        if (!target || !target->isAlive()) {
            target = acquireTarget(state, unit);
            unit->setTargetId(target ? target->id() : -1);
        }
        if (!target) {
            unit->setState(CombatState::Idle);
            continue;
        }

        // 满法力 → 施放技能（多态）
        if (unit->mana() >= unit->effectiveMaxMana()) {
            unit->setState(CombatState::Casting);
            unit->castSkill(ctx);
            unit->setMana(0);
            continue;
        }

        const double range = unit->effectiveAttackRange();
        if (Pathfinder::inAttackRange(unit->position(), target->position(), range)) {
            // 在范围内：普攻
            unit->setState(CombatState::Attacking);
            if (unit->attackCooldownTicks() <= 0) {
                const DamageResult dr = target->applyDamage(unit->effectiveAttackDamage());
                unit->gainMana(kManaPerAttack);
                unit->setAttackCooldownTicks(attackIntervalTicks(unit));
                if (dr.lethal) {
                    ctx.logUnitDeath(target, unit);
                }
                if (!target->isAlive()) {
                    unit->setTargetId(-1);
                }
            }
        } else {
            // 不在范围内：朝可攻击的最近可达格移动（仅生成意图）
            unit->setState(CombatState::Moving);
            if (unit->moveCooldownTicks() <= 0) {
                QPoint step;
                if (Pathfinder::nextStep(board, unit->position(), target->position(), range, step)) {
                    moveIntents.emplace_back(unit, step);
                }
            }
        }
    }

    // 2) 统一提交移动意图：稳定顺序占用空格，禁止重叠/抢同格。
    for (const auto& intent : moveIntents) {
        Unit* unit = intent.first;
        const QPoint to = intent.second;
        if (!unit->isAlive()) {
            continue;
        }
        // 目标格此刻仍为空才移动（其他意图可能已占用）。
        if (!board.hasUnitAt(to)) {
            board.moveUnit(unit->position(), to);
            unit->setMoveCooldownTicks(kMoveIntervalTicks);
        }
    }

    // 3) 胜负判定（含安全上限）。
    CombatResult result = evaluate(state);
    if (result == CombatResult::Ongoing && m_elapsedTicks >= m_maxTicks) {
        // 僵持超时：按存活单位数→总剩余生命决胜。
        int pa = 0, ea = 0;
        double php = 0, ehp = 0;
        for (Unit* u : m_participants) {
            if (!u->isAlive()) continue;
            if (u->owner() == Owner::Player) { ++pa; php += u->hp(); }
            else { ++ea; ehp += u->hp(); }
        }
        if (pa != ea) {
            result = (pa > ea) ? CombatResult::PlayerWin : CombatResult::PlayerLose;
        } else {
            result = (php >= ehp) ? CombatResult::PlayerWin : CombatResult::PlayerLose;
        }
        if (m_logger) m_logger(QStringLiteral("战斗超时，按场上优势判定。"));
    }
    return result;
}
