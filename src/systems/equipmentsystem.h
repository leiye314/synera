#ifndef SYSTEMS_EQUIPMENTSYSTEM_H
#define SYSTEMS_EQUIPMENTSYSTEM_H

#include <QString>
#include "entity/types.h"

class GameState;
class Unit;

// 装备系统：装备池的掉落与穿戴。
// 属性应用不在这里——装备效果在 TraitSystem::applyModifiers 重算时统一折算，避免重复叠加。
namespace EquipmentSystem {

EquipmentType randomEquipment(GameState& state);
void addToPool(GameState& state, EquipmentType type);

// 给单位穿戴装备池中第 poolIndex 件装备。
// 约束：仅准备阶段、单位存在、单位未持装备、索引合法。失败写 errorOut 返回 false。
bool equip(GameState& state, Unit* unit, int poolIndex, QString& errorOut);

} // namespace EquipmentSystem

#endif // SYSTEMS_EQUIPMENTSYSTEM_H
