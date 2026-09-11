#ifndef ENTITY_HEROES_H
#define ENTITY_HEROES_H

#include "entity/unit.h"

// 六个具体英雄类，各自重写 castSkill 实现多态技能。
// 技能效果统一通过 CombatContext 作用于战场（造成伤害/治疗/护盾/眩晕）。

// 剑士：近战重击。技能“重斩”——对当前目标造成高额单体伤害并短暂眩晕。
class WarriorUnit : public Unit
{
public:
    explicit WarriorUnit(Owner owner);
    void castSkill(CombatContext& ctx) override;
    QString skillName() const override { return QStringLiteral("重斩"); }
    QString skillDescription() const override
    { return QStringLiteral("对目标造成 2.0×攻击力 的伤害并眩晕 1.0 秒。"); }
};

// 骑士：近战坦克。技能“铁壁”——为自身获得等比例最大生命的护盾。
class KnightUnit : public Unit
{
public:
    explicit KnightUnit(Owner owner);
    void castSkill(CombatContext& ctx) override;
    QString skillName() const override { return QStringLiteral("铁壁"); }
    QString skillDescription() const override
    { return QStringLiteral("获得 = 45%最大生命 的护盾，吸收后续伤害。"); }
};

// 法师：远程 AOE。技能“冰霜新星”——对目标及其周围敌人造成范围法术伤害。
class MageUnit : public Unit
{
public:
    explicit MageUnit(Owner owner);
    void castSkill(CombatContext& ctx) override;
    QString skillName() const override { return QStringLiteral("冰霜新星"); }
    QString skillDescription() const override
    { return QStringLiteral("以目标为中心 1.5 格半径内的敌人受到法术伤害。"); }
};

// 游侠：远程高攻速。技能“急速连射”——立即对当前目标进行多次额外普攻。
class RangerUnit : public Unit
{
public:
    explicit RangerUnit(Owner owner);
    void castSkill(CombatContext& ctx) override;
    QString skillName() const override { return QStringLiteral("急速连射"); }
    QString skillDescription() const override
    { return QStringLiteral("立即对目标发动 3 次强化普攻（每次 0.8×攻击力）。"); }
};

// 祭司：远程辅助。技能“治愈之光”——治疗当前血量最低的友军。
class PriestUnit : public Unit
{
public:
    explicit PriestUnit(Owner owner);
    void castSkill(CombatContext& ctx) override;
    QString skillName() const override { return QStringLiteral("治愈之光"); }
    QString skillDescription() const override
    { return QStringLiteral("治疗最低血量友军，并为其周围友军回复少量生命。"); }
};

// 影刺：近战爆发。技能“影袭”——对血量最低的敌人造成高额单体爆发。
class AssassinUnit : public Unit
{
public:
    explicit AssassinUnit(Owner owner);
    void castSkill(CombatContext& ctx) override;
    QString skillName() const override { return QStringLiteral("影袭"); }
    QString skillDescription() const override
    { return QStringLiteral("锁定血量最低的敌人，造成 3.0×攻击力 的爆发伤害。"); }
};

#endif // ENTITY_HEROES_H
