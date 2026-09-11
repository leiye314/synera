#include "entity/unit.h"

#include <algorithm>
#include <cmath>

int Unit::s_nextId = 1;

namespace {
// 二星/三星基础属性倍率：每次升星约为前一星级的 1.7 倍。
double starMultiplier(int star)
{
    return std::pow(1.7, std::max(0, star - 1));
}
constexpr int kManaLowerBound = 20; // 蓝水晶等导致最大法力下降时的下限
}

Unit::Unit(HeroType heroType, Owner owner, const QString& name,
           const Stats& baseStar1, const QVector<Trait>& traits, int cost)
    : m_id(s_nextId++)
    , m_heroType(heroType)
    , m_owner(owner)
    , m_name(name)
    , m_cost(cost)
    , m_traits(traits)
    , m_base1(baseStar1)
    , m_star(1)
    , m_hasEquipment(false)
    , m_equipment(EquipmentType::Sword)
    , m_position(-1, -1)
    , m_state(CombatState::Idle)
    , m_hp(baseStar1.maxHp)
    , m_mana(0)
    , m_shield(0.0)
    , m_stunTicks(0)
    , m_targetId(-1)
    , m_attackCooldownTicks(0)
    , m_moveCooldownTicks(0)
{}

void Unit::setStar(int star)
{
    m_star = std::clamp(star, 1, 3);
}

Stats Unit::scaledBase() const
{
    const double m = starMultiplier(m_star);
    Stats s = m_base1;
    s.maxHp = m_base1.maxHp * m;
    s.attackDamage = m_base1.attackDamage * m;
    s.spellPower = m_base1.spellPower * m;
    // 攻击距离/间隔/法力不随星级缩放，保持可解释。
    return s;
}

double Unit::effectiveMaxHp() const
{
    const Stats b = scaledBase();
    return b.maxHp * (1.0 + m_mods.pctMaxHp) + m_mods.flatMaxHp;
}

double Unit::effectiveAttackDamage() const
{
    return scaledBase().attackDamage + m_mods.flatAttackDamage;
}

double Unit::effectiveAttackRange() const
{
    return scaledBase().attackRange;
}

double Unit::effectiveAttackIntervalSec() const
{
    const double interval = scaledBase().attackIntervalSec / (1.0 + m_mods.pctAttackSpeed);
    return std::max(0.15, interval); // 防止攻速过高导致每 tick 多次攻击
}

double Unit::effectiveSpellPower() const
{
    return scaledBase().spellPower;
}

double Unit::effectiveArmor() const
{
    return scaledBase().armor + m_mods.flatArmor;
}

double Unit::effectiveDamageReductionPct() const
{
    const double r = scaledBase().damageReductionPct + m_mods.damageReductionPct;
    return std::clamp(r, 0.0, 0.9);
}

int Unit::effectiveMaxMana() const
{
    const int m = scaledBase().maxMana + m_mods.flatMaxManaDelta;
    return std::max(kManaLowerBound, m);
}

int Unit::effectiveStartMana() const
{
    const int m = scaledBase().startMana + m_mods.flatStartMana;
    return std::clamp(m, 0, effectiveMaxMana());
}

void Unit::setHp(double hp)
{
    m_hp = std::clamp(hp, 0.0, effectiveMaxHp());
    if (m_hp <= 0.0) {
        m_state = CombatState::Dead;
    }
}

void Unit::setMana(int mana)
{
    m_mana = std::clamp(mana, 0, effectiveMaxMana());
}

void Unit::normalizeForPrep()
{
    m_state = CombatState::Idle;
    m_hp = effectiveMaxHp();
    m_mana = 0;
    m_shield = 0.0;
    m_stunTicks = 0;
    m_targetId = -1;
    m_attackCooldownTicks = 0;
    m_moveCooldownTicks = 0;
}

void Unit::beginCombat()
{
    m_state = CombatState::Idle;
    m_hp = effectiveMaxHp();
    m_mana = effectiveStartMana();
    m_shield = 0.0;
    m_stunTicks = 0;
    m_targetId = -1;
    m_attackCooldownTicks = 0;
    m_moveCooldownTicks = 0;
}

DamageResult Unit::applyDamage(double rawAmount)
{
    DamageResult result;
    if (rawAmount <= 0.0 || m_state == CombatState::Dead) {
        return result;
    }
    result.raw = rawAmount;

    // 先按护甲（平坦）与百分比减伤计算结算伤害，至少造成 1 点。
    double dmg = rawAmount - effectiveArmor();
    dmg *= (1.0 - effectiveDamageReductionPct());
    dmg = std::max(1.0, dmg);
    result.resolved = dmg;

    // 护盾优先吸收。
    if (m_shield > 0.0) {
        const double absorbed = std::min(m_shield, dmg);
        m_shield -= absorbed;
        dmg -= absorbed;
        result.shieldAbsorbed = absorbed;
    }

    const double before = m_hp;
    m_hp = std::max(0.0, m_hp - dmg);
    if (m_hp <= 0.0) {
        m_state = CombatState::Dead;
        result.lethal = true;
    }
    result.hpLoss = before - m_hp;
    return result;
}

void Unit::heal(double amount)
{
    if (amount <= 0.0 || m_state == CombatState::Dead) {
        return;
    }
    m_hp = std::min(effectiveMaxHp(), m_hp + amount);
}

void Unit::gainMana(int amount)
{
    if (amount == 0) {
        return;
    }
    m_mana = std::clamp(m_mana + amount, 0, effectiveMaxMana());
}
