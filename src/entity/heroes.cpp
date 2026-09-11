#include "entity/heroes.h"
#include "entity/herodata.h"
#include "systems/combatcontext.h"

#include <algorithm>

namespace {
// 1.0 秒 = 10 个战斗 tick（tick 间隔 100ms，见 CombatSystem）。
constexpr int kStunTicks = 10;

// 用目录数据构造单位，保证“静态数据单一来源”。
Stats baseOf(HeroType type) { return HeroData::info(type).base; }
QString nameOf(HeroType type) { return HeroData::info(type).name; }
QVector<Trait> traitsOf(HeroType type) { return HeroData::info(type).traits; }
int costOf(HeroType type) { return HeroData::info(type).cost; }
}

// ---------------- 剑士 ----------------
WarriorUnit::WarriorUnit(Owner owner)
    : Unit(HeroType::Warrior, owner, nameOf(HeroType::Warrior),
           baseOf(HeroType::Warrior), traitsOf(HeroType::Warrior), costOf(HeroType::Warrior))
{}

void WarriorUnit::castSkill(CombatContext& ctx)
{
    Unit* target = ctx.unitById(targetId());
    if (!target || !target->isAlive()) {
        target = ctx.lowestHpEnemy(this);
    }
    if (!target) {
        return;
    }
    const DamageResult dr = ctx.dealSkillDamage(this, target, 2.0 * effectiveAttackDamage(),
                                                 QStringLiteral("重斩"));
    target->setStunTicks(kStunTicks);
    ctx.logSkillToTarget(this, QStringLiteral("重斩"), target, dr,
                         QStringLiteral("，并眩晕"));
}

// ---------------- 骑士 ----------------
KnightUnit::KnightUnit(Owner owner)
    : Unit(HeroType::Knight, owner, nameOf(HeroType::Knight),
           baseOf(HeroType::Knight), traitsOf(HeroType::Knight), costOf(HeroType::Knight))
{}

void KnightUnit::castSkill(CombatContext& ctx)
{
    const double shieldAmount = 0.45 * effectiveMaxHp();
    const double before = shield();
    const double after = std::max(before, shieldAmount);
    setShield(after);
    if (before <= 0.0) {
        ctx.logSkillSelf(this, QStringLiteral("铁壁"),
                         QStringLiteral("获得 %1 点护盾").arg(static_cast<int>(after)));
    } else if (after > before + 0.5) {
        ctx.logSkillSelf(this, QStringLiteral("铁壁"),
                         QStringLiteral("护盾刷新至 %1").arg(static_cast<int>(after)));
    }
    // 已达技能护盾上限时不再重复刷日志，避免僵持战中刷屏。
}

// ---------------- 法师 ----------------
MageUnit::MageUnit(Owner owner)
    : Unit(HeroType::Mage, owner, nameOf(HeroType::Mage),
           baseOf(HeroType::Mage), traitsOf(HeroType::Mage), costOf(HeroType::Mage))
{}

void MageUnit::castSkill(CombatContext& ctx)
{
    Unit* target = ctx.unitById(targetId());
    if (!target || !target->isAlive()) {
        target = ctx.lowestHpEnemy(this);
    }
    if (!target) {
        return;
    }
    const double raw = 110.0 * effectiveSpellPower();
    const Owner enemyOwner = (owner() == Owner::Player) ? Owner::Enemy : Owner::Player;
    const QVector<Unit*> victims = ctx.aliveInRadius(target->position(), 1.5, enemyOwner);
    DamageResult dr;
    for (Unit* v : victims) {
        dr += ctx.dealSkillDamage(this, v, raw, QStringLiteral("冰霜新星"));
    }
    ctx.logSkillAoE(this, QStringLiteral("冰霜新星"), victims, dr);
}

// ---------------- 游侠 ----------------
RangerUnit::RangerUnit(Owner owner)
    : Unit(HeroType::Ranger, owner, nameOf(HeroType::Ranger),
           baseOf(HeroType::Ranger), traitsOf(HeroType::Ranger), costOf(HeroType::Ranger))
{}

void RangerUnit::castSkill(CombatContext& ctx)
{
    Unit* target = ctx.unitById(targetId());
    if (!target || !target->isAlive()) {
        target = ctx.lowestHpEnemy(this);
    }
    if (!target) {
        return;
    }
    DamageResult dr;
    for (int i = 0; i < 3 && target->isAlive(); ++i) {
        dr += ctx.dealSkillDamage(this, target, 0.8 * effectiveAttackDamage(),
                                  QStringLiteral("急速连射"));
    }
    ctx.logSkillToTarget(this, QStringLiteral("急速连射"), target, dr);
}

// ---------------- 祭司 ----------------
PriestUnit::PriestUnit(Owner owner)
    : Unit(HeroType::Priest, owner, nameOf(HeroType::Priest),
           baseOf(HeroType::Priest), traitsOf(HeroType::Priest), costOf(HeroType::Priest))
{}

void PriestUnit::castSkill(CombatContext& ctx)
{
    Unit* target = ctx.lowestHpAlly(this, true);
    if (!target) {
        return;
    }
    const double mainHeal = 220.0 * effectiveSpellPower();
    const double actualMain = ctx.healUnitActual(target, mainHeal);
    // 目标周围友军获得少量治疗。
    const QVector<Unit*> nearby = ctx.aliveInRadius(target->position(), 1.5, owner());
    for (Unit* a : nearby) {
        if (a != target) {
            ctx.healUnit(a, 0.4 * mainHeal);
        }
    }
    ctx.logSkillHeal(this, QStringLiteral("治愈之光"), target, actualMain);
}

// ---------------- 影刺 ----------------
AssassinUnit::AssassinUnit(Owner owner)
    : Unit(HeroType::Assassin, owner, nameOf(HeroType::Assassin),
           baseOf(HeroType::Assassin), traitsOf(HeroType::Assassin), costOf(HeroType::Assassin))
{}

void AssassinUnit::castSkill(CombatContext& ctx)
{
    Unit* target = ctx.lowestHpEnemy(this);
    if (!target) {
        return;
    }
    const DamageResult dr = ctx.dealSkillDamage(this, target, 3.0 * effectiveAttackDamage(),
                                                 QStringLiteral("影袭"));
    ctx.logSkillToTarget(this, QStringLiteral("影袭"), target, dr);
}
