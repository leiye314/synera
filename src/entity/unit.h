#ifndef ENTITY_UNIT_H
#define ENTITY_UNIT_H

#include <QPoint>
#include <QString>
#include <QVector>
#include "entity/types.h"

class CombatContext; // 由 CombatSystem 提供，技能通过它影响战场

// Unit 是我方/敌方共用的单位基类。
//
// 职责：
//  - 持有公共属性（基础属性、星级、羁绊、装备）与战斗运行时状态（HP/Mana/状态机/位置）。
//  - 提供“有效属性”计算：基础属性 × 星级，再叠加 StatModifiers（羁绊+装备）。
//  - 暴露纯虚 castSkill()，由具体英雄重写以实现多态技能。
//
// 所有权：Unit 由 GameState 通过 std::unique_ptr 集中持有；Board/Bench/系统只持有非拥有指针或 id。
// 不变量：
//  - id 全局唯一且稳定，可用于存档与跨模块引用。
//  - m_mods 永远是“从零重建”的结果（TraitSystem/EquipmentSystem 负责），不会重复累计。
class Unit
{
public:
    Unit(HeroType heroType, Owner owner, const QString& name,
         const Stats& baseStar1, const QVector<Trait>& traits, int cost);
    virtual ~Unit() = default;

    // ---- 多态技能接口 ----
    virtual void castSkill(CombatContext& ctx) = 0;
    virtual QString skillName() const = 0;
    virtual QString skillDescription() const = 0;

    // ---- 身份与目录 ----
    int id() const { return m_id; }
    HeroType heroType() const { return m_heroType; }
    QString name() const { return m_name; }
    Owner owner() const { return m_owner; }
    int cost() const { return m_cost; }
    const QVector<Trait>& traits() const { return m_traits; }
    bool hasTrait(Trait t) const { return m_traits.contains(t); }

    void setOwner(Owner owner) { m_owner = owner; }

    // ---- 星级 ----
    int star() const { return m_star; }
    void setStar(int star);

    // ---- 装备 ----
    bool hasEquipment() const { return m_hasEquipment; }
    EquipmentType equipment() const { return m_equipment; }
    void setEquipment(EquipmentType type) { m_hasEquipment = true; m_equipment = type; }
    void clearEquipment() { m_hasEquipment = false; }

    // ---- 修正与有效属性 ----
    void setModifiers(const StatModifiers& mods) { m_mods = mods; }
    const StatModifiers& modifiers() const { return m_mods; }

    double effectiveMaxHp() const;
    double effectiveAttackDamage() const;
    double effectiveAttackRange() const;
    double effectiveAttackIntervalSec() const;
    double effectiveSpellPower() const;
    double effectiveArmor() const;
    double effectiveDamageReductionPct() const;
    int    effectiveMaxMana() const;
    int    effectiveStartMana() const;
    double effectiveSkillDamagePct() const { return m_mods.skillDamagePct; }

    // ---- 棋盘位置（备战区时为 (-1,-1)）----
    QPoint position() const { return m_position; }
    void setPosition(const QPoint& pos) { m_position = pos; }

    // ---- 战斗运行时状态 ----
    CombatState state() const { return m_state; }
    void setState(CombatState s) { m_state = s; }
    bool isAlive() const { return m_state != CombatState::Dead; }

    double hp() const { return m_hp; }
    void setHp(double hp);
    int mana() const { return m_mana; }
    void setMana(int mana);
    double shield() const { return m_shield; }
    void setShield(double s) { m_shield = s < 0 ? 0 : s; }

    int stunTicks() const { return m_stunTicks; }
    void setStunTicks(int t) { m_stunTicks = t < 0 ? 0 : t; }

    int targetId() const { return m_targetId; }
    void setTargetId(int id) { m_targetId = id; }

    int attackCooldownTicks() const { return m_attackCooldownTicks; }
    void setAttackCooldownTicks(int t) { m_attackCooldownTicks = t; }
    int moveCooldownTicks() const { return m_moveCooldownTicks; }
    void setMoveCooldownTicks(int t) { m_moveCooldownTicks = t; }

    // 准备阶段保持满血满状态、清空临时法力（受灵族影响的初始法力在战斗开始时给出）。
    void normalizeForPrep();
    // 战斗开始：重置 HP/Mana/状态/护盾/冷却。
    void beginCombat();

    // 受到一次伤害：先按护甲/百分比减伤得到结算伤害，再优先扣护盾，余下扣生命。
    // 返回区分“结算伤害 / 护盾吸收 / 实际生命损失”的 DamageResult。HP<=0 转为 Dead
    // （死亡判定与旧实现完全一致：仍以生命损失后的 HP<=0 为准）。
    DamageResult applyDamage(double rawAmount);
    void heal(double amount);
    void gainMana(int amount);

    bool isMelee() const { return effectiveAttackRange() <= 1.5; }

protected:
    Stats scaledBase() const; // 基础属性 × 星级倍率

private:
    static int s_nextId;

    int m_id;
    HeroType m_heroType;
    Owner m_owner;
    QString m_name;
    int m_cost;
    QVector<Trait> m_traits;

    Stats m_base1;              // 一星基础属性
    int m_star;
    StatModifiers m_mods;

    bool m_hasEquipment;
    EquipmentType m_equipment;

    QPoint m_position;          // 棋盘格；(-1,-1)=不在棋盘

    // 战斗运行时
    CombatState m_state;
    double m_hp;
    int m_mana;
    double m_shield;
    int m_stunTicks;
    int m_targetId;
    int m_attackCooldownTicks;
    int m_moveCooldownTicks;
};

#endif // ENTITY_UNIT_H
