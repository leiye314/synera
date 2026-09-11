#ifndef SYSTEMS_SHOPSYSTEM_H
#define SYSTEMS_SHOPSYSTEM_H

#include <QString>

class GameState;
class Unit;

// 商店系统：刷新 5 个商店位、购买英雄到备战区。
// 不持有状态，所有数据读写 GameState（便于存档）。
namespace ShopSystem {

// 重新随机填满 5 个商店位（不扣费，用于轮次开始与付费刷新的内部实现）。
void refill(GameState& state);

// 购买第 slot 个商店位的英雄。失败时写入 errorOut 并返回 nullptr。
// 成功：扣金币、创建单位、放入第一个空备战槽、清空该商店位。
Unit* buy(GameState& state, int slot, QString& errorOut);

} // namespace ShopSystem

#endif // SYSTEMS_SHOPSYSTEM_H
