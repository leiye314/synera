#include "entity/equipment.h"

namespace EquipmentData {

QString name(EquipmentType type)
{
    switch (type) {
    case EquipmentType::Sword:   return QStringLiteral("铁剑");
    case EquipmentType::Armor:   return QStringLiteral("锁子甲");
    case EquipmentType::Gloves:  return QStringLiteral("急速手套");
    case EquipmentType::Crystal: return QStringLiteral("蓝水晶");
    }
    return QString();
}

QString description(EquipmentType type)
{
    switch (type) {
    case EquipmentType::Sword:   return QStringLiteral("攻击力 +15");
    case EquipmentType::Armor:   return QStringLiteral("最大生命 +150");
    case EquipmentType::Gloves:  return QStringLiteral("攻击间隔 -20%");
    case EquipmentType::Crystal: return QStringLiteral("最大法力 -30");
    }
    return QString();
}

void apply(EquipmentType type, StatModifiers& mods)
{
    switch (type) {
    case EquipmentType::Sword:
        mods.flatAttackDamage += 15.0;
        break;
    case EquipmentType::Armor:
        // +150 最大生命；当前 HP 在准备阶段与最大值同步（由 GameController 同步）。
        mods.flatMaxHp += 150.0;
        break;
    case EquipmentType::Gloves:
        // 间隔 -20% 等价于攻速 /0.8 = ×1.25 => +0.25 攻速。
        mods.pctAttackSpeed += 0.25;
        break;
    case EquipmentType::Crystal:
        // 最大法力 -30，下限在 Unit 计算有效法力时保证。
        mods.flatMaxManaDelta -= 30;
        break;
    }
}

} // namespace EquipmentData
