#include "systems/combatcontext.h"
#include "entity/unit.h"

#include <algorithm>
#include <cmath>
#include <QStringList>

CombatContext::CombatContext(std::vector<Unit*>& participants,
                             Board& board,
                             std::function<void(const QString&)> logger)
    : m_participants(participants)
    , m_board(board)
    , m_logger(std::move(logger))
{}

Unit* CombatContext::unitById(int id) const
{
    for (Unit* u : m_participants) {
        if (u && u->id() == id) {
            return u;
        }
    }
    return nullptr;
}

QVector<Unit*> CombatContext::allies(const Unit* unit, bool includeSelf) const
{
    QVector<Unit*> result;
    if (!unit) {
        return result;
    }
    for (Unit* u : m_participants) {
        if (!u || !u->isAlive()) {
            continue;
        }
        if (u->owner() != unit->owner()) {
            continue;
        }
        if (!includeSelf && u == unit) {
            continue;
        }
        result.append(u);
    }
    return result;
}

QVector<Unit*> CombatContext::enemies(const Unit* unit) const
{
    QVector<Unit*> result;
    if (!unit) {
        return result;
    }
    for (Unit* u : m_participants) {
        if (!u || !u->isAlive()) {
            continue;
        }
        if (u->owner() == unit->owner()) {
            continue;
        }
        result.append(u);
    }
    return result;
}

Unit* CombatContext::lowestHpAlly(const Unit* unit, bool includeSelf) const
{
    Unit* best = nullptr;
    for (Unit* u : allies(unit, includeSelf)) {
        if (!best || u->hp() < best->hp()) {
            best = u;
        }
    }
    return best;
}

Unit* CombatContext::lowestHpEnemy(const Unit* unit) const
{
    Unit* best = nullptr;
    for (Unit* u : enemies(unit)) {
        if (!best || u->hp() < best->hp()) {
            best = u;
        }
    }
    return best;
}

QVector<Unit*> CombatContext::aliveInRadius(const QPoint& center, double radius, Owner owner) const
{
    QVector<Unit*> result;
    const double r2 = radius * radius;
    for (Unit* u : m_participants) {
        if (!u || !u->isAlive() || u->owner() != owner) {
            continue;
        }
        const QPoint p = u->position();
        const double dx = p.x() - center.x();
        const double dy = p.y() - center.y();
        if (dx * dx + dy * dy <= r2 + 1e-6) {
            result.append(u);
        }
    }
    return result;
}

DamageResult CombatContext::dealSkillDamage(Unit* caster, Unit* target, double raw,
                                            const QString& sourceSkill)
{
    if (!caster || !target || !target->isAlive() || raw <= 0.0) {
        return DamageResult{};
    }
    const double amplified = raw * (1.0 + caster->effectiveSkillDamagePct());
    const DamageResult dr = target->applyDamage(amplified);
    if (dr.lethal && caster) {
        logUnitDeath(target, caster, sourceSkill);
    }
    return dr;
}

QString CombatContext::combatLabel(const Unit* unit)
{
    if (!unit) {
        return QString();
    }
    const QString prefix = (unit->owner() == Owner::Player)
                               ? QStringLiteral("我方")
                               : QStringLiteral("敌方");
    return prefix + unit->name();
}

QString CombatContext::damageBreakdown(const DamageResult& result)
{
    return QStringLiteral("护盾吸收 %1，生命损失 %2")
        .arg(static_cast<long long>(std::llround(result.shieldAbsorbed)))
        .arg(static_cast<long long>(std::llround(result.hpLoss)));
}

void CombatContext::logSkillToTarget(Unit* caster, const QString& skillName, Unit* target,
                                      const DamageResult& dr, const QString& suffix)
{
    if (!caster || !target) {
        return;
    }
    log(QStringLiteral("%1 释放【%2】→ %3：%4%5")
            .arg(combatLabel(caster), skillName, combatLabel(target),
                 damageBreakdown(dr), suffix));
}

void CombatContext::logSkillSelf(Unit* caster, const QString& skillName, const QString& effect)
{
    if (!caster) {
        return;
    }
    log(QStringLiteral("%1 释放【%2】：%3").arg(combatLabel(caster), skillName, effect));
}

void CombatContext::logSkillAoE(Unit* caster, const QString& skillName,
                                const QVector<Unit*>& victims, const DamageResult& totalDr)
{
    if (!caster) {
        return;
    }
    QVector<Unit*> sorted = victims;
    std::sort(sorted.begin(), sorted.end(),
              [](Unit* a, Unit* b) { return a->id() < b->id(); });
    QStringList names;
    for (Unit* v : sorted) {
        if (v) {
            names.append(combatLabel(v));
        }
    }
    const QString hitLine = names.isEmpty()
                                ? QStringLiteral("未命中任何单位")
                                : QStringLiteral("命中%1").arg(names.join(QStringLiteral("、")));
    log(QStringLiteral("%1 释放【%2】：\n%3；%4")
            .arg(combatLabel(caster), skillName, hitLine, damageBreakdown(totalDr)));
}

void CombatContext::logSkillHeal(Unit* caster, const QString& skillName, Unit* target,
                                 double actualHeal)
{
    if (!caster || !target) {
        return;
    }
    log(QStringLiteral("%1 释放【%2】→ %3：实际恢复 %4 HP")
            .arg(combatLabel(caster), skillName, combatLabel(target))
            .arg(static_cast<long long>(std::llround(actualHeal))));
}

void CombatContext::logUnitDeath(Unit* victim, Unit* killer, const QString& sourceSkill)
{
    if (!victim || !killer) {
        return;
    }
    const QString source = sourceSkill.isEmpty()
                               ? QStringLiteral("%1的普通攻击").arg(combatLabel(killer))
                               : QStringLiteral("%1【%2】").arg(combatLabel(killer), sourceSkill);
    log(QStringLiteral("%1 被%2击败").arg(combatLabel(victim), source));
}

double CombatContext::healUnitActual(Unit* target, double amount)
{
    if (!target || amount <= 0.0 || !target->isAlive()) {
        return 0.0;
    }
    const double before = target->hp();
    target->heal(amount);
    return std::max(0.0, target->hp() - before);
}

void CombatContext::healUnit(Unit* target, double amount)
{
    if (target) {
        target->heal(amount);
    }
}

void CombatContext::log(const QString& msg)
{
    if (m_logger) {
        m_logger(msg);
    }
}
