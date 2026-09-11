#include "systems/shopsystem.h"
#include "core/gamestate.h"
#include "entity/unit.h"
#include "entity/herodata.h"


namespace ShopSystem {

void refill(GameState& state)
{
    const QVector<HeroData::HeroInfo>& catalog = HeroData::catalog();
    if (catalog.isEmpty()) {
        return;
    }
    QVector<ShopSlot>& shop = state.shop();
    for (ShopSlot& slot : shop) {
        const int pick = state.random().bounded(static_cast<int>(catalog.size()));
        slot.filled = true;
        slot.type = catalog.at(pick).type;
    }
}

Unit* buy(GameState& state, int slot, QString& errorOut)
{
    errorOut.clear();
    QVector<ShopSlot>& shop = state.shop();
    if (slot < 0 || slot >= shop.size() || !shop[slot].filled) {
        errorOut = QStringLiteral("该商店位为空。");
        return nullptr;
    }
    const HeroType type = shop[slot].type;
    const int cost = HeroData::info(type).cost;

    if (state.bench().isFull()) {
        errorOut = QStringLiteral("备战区已满，无法购买。");
        return nullptr;
    }
    if (state.player().gold() < cost) {
        errorOut = QStringLiteral("金币不足，无法购买。");
        return nullptr;
    }

    state.player().spendGold(cost);
    Unit* unit = state.addUnit(HeroData::create(type, Owner::Player));
    if (!state.bench().addUnit(unit)) {
        // 理论上不会发生（已检查未满）；保险起见回滚。
        state.player().addGold(cost);
        state.removeUnit(unit);
        errorOut = QStringLiteral("备战区放置失败。");
        return nullptr;
    }
    shop[slot].filled = false;
    return unit;
}

} // namespace ShopSystem
