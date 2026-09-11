#ifndef ENTITY_EQUIPMENT_H
#define ENTITY_EQUIPMENT_H

#include <QString>
#include "entity/types.h"

// 装备是“值类型”：每件装备就是一个 EquipmentType。
// 单位最多持有一件；装备池是一组 EquipmentType。
// 装备效果通过把修正加到 StatModifiers 上实现，由 EquipmentSystem 在重算时统一应用，
// 因此不会重复叠加。
namespace EquipmentData {

QString name(EquipmentType type);
QString description(EquipmentType type);

// 将该装备的属性修正叠加进 mods。
void apply(EquipmentType type, StatModifiers& mods);

} // namespace EquipmentData

#endif // ENTITY_EQUIPMENT_H
