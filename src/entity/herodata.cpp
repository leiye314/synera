#include "entity/herodata.h"
#include "entity/heroes.h"

namespace HeroData {

namespace {
// 构造一星基础属性的小助手，保持目录表清晰可读。
Stats makeStats(double hp, double ad, double range, double interval,
                double spell, double armor, double drPct, int maxMana, int startMana)
{
    Stats s;
    s.maxHp = hp;
    s.attackDamage = ad;
    s.attackRange = range;
    s.attackIntervalSec = interval;
    s.spellPower = spell;
    s.armor = armor;
    s.damageReductionPct = drPct;
    s.maxMana = maxMana;
    s.startMana = startMana;
    return s;
}

const QVector<HeroInfo>& buildCatalog()
{
    using T = Trait;
    static const QVector<HeroInfo> kCatalog = {
        // 剑士：近战肉搏，血厚攻中，重击眩晕。
        { HeroType::Warrior, QStringLiteral("剑士"), 1,
          makeStats(650, 55, 1.0, 1.00, 1.0, 8, 0.00, 100, 0),
          { T::Warrior, T::Guardian } },

        // 骑士：高血量坦克，技能获得护盾。
        { HeroType::Knight, QStringLiteral("骑士"), 2,
          makeStats(950, 38, 1.0, 1.10, 1.0, 15, 0.05, 80, 0),
          { T::Guardian, T::Warrior } },

        // 法师：远程范围法术。
        { HeroType::Mage, QStringLiteral("法师"), 3,
          makeStats(450, 35, 3.0, 1.30, 2.2, 0, 0.00, 100, 0),
          { T::Mage, T::Spirit } },

        // 游侠：远程高攻速，连射。
        { HeroType::Ranger, QStringLiteral("游侠"), 2,
          makeStats(520, 48, 3.0, 0.70, 1.0, 0, 0.00, 60, 0),
          { T::Ranger, T::Spirit } },

        // 祭司：远程治疗，自带初始法力。
        { HeroType::Priest, QStringLiteral("祭司"), 3,
          makeStats(480, 30, 3.0, 1.20, 1.8, 0, 0.00, 90, 20),
          { T::Mage, T::Ranger } },

        // 影刺：近战高爆发，低血量。
        { HeroType::Assassin, QStringLiteral("影刺"), 2,
          makeStats(540, 72, 1.0, 0.90, 1.0, 0, 0.00, 80, 0),
          { T::Warrior, T::Ranger } },
    };
    return kCatalog;
}
} // namespace

const QVector<HeroInfo>& catalog()
{
    return buildCatalog();
}

const HeroInfo& info(HeroType type)
{
    const QVector<HeroInfo>& c = catalog();
    for (const HeroInfo& h : c) {
        if (h.type == type) {
            return h;
        }
    }
    return c.first();
}

std::unique_ptr<Unit> create(HeroType type, Owner owner)
{
    switch (type) {
    case HeroType::Warrior:  return std::make_unique<WarriorUnit>(owner);
    case HeroType::Knight:   return std::make_unique<KnightUnit>(owner);
    case HeroType::Mage:     return std::make_unique<MageUnit>(owner);
    case HeroType::Ranger:   return std::make_unique<RangerUnit>(owner);
    case HeroType::Priest:   return std::make_unique<PriestUnit>(owner);
    case HeroType::Assassin: return std::make_unique<AssassinUnit>(owner);
    }
    return std::make_unique<WarriorUnit>(owner);
}

} // namespace HeroData
