#ifndef SYSTEMS_COMBATCONTEXT_H
#define SYSTEMS_COMBATCONTEXT_H

#include <functional>
#include <vector>
#include <QPoint>
#include <QString>
#include <QVector>
#include "entity/types.h"

class Unit;
class Board;

// CombatContext 是技能与战斗逻辑访问战场的统一接口。
// 它不拥有任何单位，只引用当前参战单位列表与棋盘，并提供查询/伤害/治疗等帮助函数。
// 这样具体英雄的 castSkill 只依赖这个接口，而不直接耦合 CombatSystem 的内部实现。
class CombatContext
{
public:
    CombatContext(std::vector<Unit*>& participants,
                  Board& board,
                  std::function<void(const QString&)> logger);

    Unit* unitById(int id) const;

    // 存活且在棋盘上的友军/敌军（按 owner 区分）。
    QVector<Unit*> allies(const Unit* unit, bool includeSelf = true) const;
    QVector<Unit*> enemies(const Unit* unit) const;

    Unit* lowestHpAlly(const Unit* unit, bool includeSelf = true) const;
    Unit* lowestHpEnemy(const Unit* unit) const;

    // 以 center（格坐标）为圆心、radius 格（欧氏）为半径内、指定归属的存活单位。
    QVector<Unit*> aliveInRadius(const QPoint& center, double radius, Owner owner) const;

    // 造成技能伤害：raw 会乘以施法者的技能伤害加成（法师羁绊）。
    // 返回区分护盾吸收/生命损失的 DamageResult，使技能日志与普攻共用同一伤害结构。
    // sourceSkill 非空且致死时，会写入带技能名的死亡日志。
    DamageResult dealSkillDamage(Unit* caster, Unit* target, double raw,
                                 const QString& sourceSkill = QString());
    void healUnit(Unit* target, double amount);
    // 治疗并返回实际增加的生命（不超过缺失生命）。
    double healUnitActual(Unit* target, double amount);

    // 战斗日志专用单位标签：“我方骑士”“敌方法师”（不修改 unit->name()）。
    static QString combatLabel(const Unit* unit);

    // 把一次伤害结算格式化为统一的护盾日志文案：“护盾吸收 X，生命损失 Y”。
    static QString damageBreakdown(const DamageResult& result);

    void logSkillToTarget(Unit* caster, const QString& skillName, Unit* target,
                          const DamageResult& dr, const QString& suffix = QString());
    void logSkillSelf(Unit* caster, const QString& skillName, const QString& effect);
    void logSkillAoE(Unit* caster, const QString& skillName,
                     const QVector<Unit*>& victims, const DamageResult& totalDr);
    void logSkillHeal(Unit* caster, const QString& skillName, Unit* target, double actualHeal);
    // sourceSkill 为空表示普通攻击致死。
    void logUnitDeath(Unit* victim, Unit* killer, const QString& sourceSkill = QString());

    void log(const QString& msg);

    Board& board() const { return m_board; }

private:
    std::vector<Unit*>& m_participants;
    Board& m_board;
    std::function<void(const QString&)> m_logger;
};

#endif // SYSTEMS_COMBATCONTEXT_H
