#include "systems/equipmentsystem.h"
#include "core/gamestate.h"
#include "entity/unit.h"


namespace EquipmentSystem {

EquipmentType randomEquipment(GameState& state)
{
    const EquipmentType all[] = {
        EquipmentType::Sword, EquipmentType::Armor,
        EquipmentType::Gloves, EquipmentType::Crystal
    };
    const int pick = state.random().bounded(4);
    return all[pick];
}

void addToPool(GameState& state, EquipmentType type)
{
    state.equipmentPool().append(type);
}

bool equip(GameState& state, Unit* unit, int poolIndex, QString& errorOut)
{
    if (state.phase() != Phase::Prep) {
        errorOut = QStringLiteral("只能在准备阶段穿戴装备。");
        return false;
    }
    errorOut.clear();
    if (!unit || state.findUnit(unit->id()) != unit || !unit->isAlive()) {
        errorOut = QStringLiteral("未选择单位。");
        return false;
    }
    if (unit->owner() != Owner::Player) {
        errorOut = QStringLiteral("只能给我方单位穿戴装备。");
        return false;
    }
    QVector<EquipmentType>& pool = state.equipmentPool();
    if (poolIndex < 0 || poolIndex >= pool.size()) {
        errorOut = QStringLiteral("装备无效。");
        return false;
    }
    if (unit->hasEquipment()) {
        errorOut = QStringLiteral("该单位已装备，每个单位最多一件。");
        return false;
    }

    unit->setEquipment(pool.at(poolIndex));
    pool.removeAt(poolIndex);
    return true;
}

} // namespace EquipmentSystem
