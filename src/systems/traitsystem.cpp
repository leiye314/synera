#include "systems/traitsystem.h"
#include "entity/unit.h"
#include "entity/equipment.h"

#include <QMap>
#include <QSet>

namespace TraitSystem {

QVector<int> thresholds(Trait trait)
{
    switch (trait) {
    case Trait::Warrior:  return {2, 3};
    case Trait::Guardian: return {2};
    case Trait::Mage:     return {2};
    case Trait::Ranger:   return {2, 3};
    case Trait::Spirit:   return {2};
    }
    return {};
}

QString effectText(Trait trait)
{
    switch (trait) {
    case Trait::Warrior:  return QStringLiteral("战士最大生命 +180 / +400");
    case Trait::Guardian: return QStringLiteral("守卫受到伤害降低 18%");
    case Trait::Mage:     return QStringLiteral("全队技能伤害 +30%");
    case Trait::Ranger:   return QStringLiteral("游侠攻速 +25% / +55%");
    case Trait::Spirit:   return QStringLiteral("全队初始法力 +35");
    }
    return QString();
}

QString effectTextForTier(Trait trait, int tier)
{
    switch (trait) {
    case Trait::Warrior:  return tier >= 3 ? QStringLiteral("战士最大生命 +400")
                                           : QStringLiteral("战士最大生命 +180");
    case Trait::Guardian: return QStringLiteral("守卫受到伤害 -18%");
    case Trait::Mage:     return QStringLiteral("全队技能伤害 +30%");
    case Trait::Ranger:   return tier >= 3 ? QStringLiteral("游侠攻速 +55%")
                                           : QStringLiteral("游侠攻速 +25%");
    case Trait::Spirit:   return QStringLiteral("全队初始法力 +35");
    }
    return QString();
}

namespace {
// 该羁绊在给定数量下的生效档位（返回最高满足的阈值，未达最低阈值返回 0）。
int activeThreshold(Trait trait, int count)
{
    int active = 0;
    for (int t : thresholds(trait)) {
        if (count >= t) {
            active = t;
        }
    }
    return active;
}

// 统计每个羁绊的“不同英雄类型”数量。
QMap<Trait, int> countTraits(const QVector<Unit*>& units)
{
    QMap<Trait, QSet<int>> typesByTrait; // trait -> set of HeroType(int)
    for (Unit* u : units) {
        if (!u) {
            continue;
        }
        for (Trait t : u->traits()) {
            typesByTrait[t].insert(static_cast<int>(u->heroType()));
        }
    }
    QMap<Trait, int> counts;
    const Trait all[] = { Trait::Warrior, Trait::Guardian, Trait::Mage, Trait::Ranger, Trait::Spirit };
    for (Trait t : all) {
        counts[t] = static_cast<int>(typesByTrait.value(t).size());
    }
    return counts;
}
} // namespace

QVector<TraitStatus> status(const QVector<Unit*>& units)
{
    const QMap<Trait, int> counts = countTraits(units);
    QVector<TraitStatus> result;
    const Trait order[] = { Trait::Warrior, Trait::Guardian, Trait::Mage, Trait::Ranger, Trait::Spirit };
    for (Trait t : order) {
        const int c = counts.value(t, 0);
        if (c > 0) {
            result.append({ t, c, activeThreshold(t, c) });
        }
    }
    return result;
}

void applyModifiers(const QVector<Unit*>& units)
{
    const QMap<Trait, int> counts = countTraits(units);

    const int warriorTier  = activeThreshold(Trait::Warrior, counts.value(Trait::Warrior, 0));
    const int guardianTier = activeThreshold(Trait::Guardian, counts.value(Trait::Guardian, 0));
    const int mageTier     = activeThreshold(Trait::Mage, counts.value(Trait::Mage, 0));
    const int rangerTier   = activeThreshold(Trait::Ranger, counts.value(Trait::Ranger, 0));
    const int spiritTier   = activeThreshold(Trait::Spirit, counts.value(Trait::Spirit, 0));

    for (Unit* u : units) {
        if (!u) {
            continue;
        }
        StatModifiers m; // 从零重建

        // 1) 装备
        if (u->hasEquipment()) {
            EquipmentData::apply(u->equipment(), m);
        }

        // 2) 羁绊（只取最高生效档位，不累计同一羁绊的多个档位）
        // 战士（自身）：+最大生命
        if (u->hasTrait(Trait::Warrior)) {
            if (warriorTier >= 3)      m.flatMaxHp += 400.0;
            else if (warriorTier >= 2) m.flatMaxHp += 180.0;
        }
        // 守卫（自身）：减伤
        if (u->hasTrait(Trait::Guardian) && guardianTier >= 2) {
            m.damageReductionPct += 0.18;
        }
        // 法师（全队）：技能伤害
        if (mageTier >= 2) {
            m.skillDamagePct += 0.30;
        }
        // 游侠（自身）：攻速
        if (u->hasTrait(Trait::Ranger)) {
            if (rangerTier >= 3)      m.pctAttackSpeed += 0.55;
            else if (rangerTier >= 2) m.pctAttackSpeed += 0.25;
        }
        // 灵族（全队）：初始法力
        if (spiritTier >= 2) {
            m.flatStartMana += 35;
        }

        u->setModifiers(m);
    }
}

} // namespace TraitSystem
