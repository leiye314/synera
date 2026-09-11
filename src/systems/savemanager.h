#ifndef SYSTEMS_SAVEMANAGER_H
#define SYSTEMS_SAVEMANAGER_H

#include <QString>

class GameState;

// 存档系统：使用 Qt JSON 序列化/反序列化 GameState。
//
// 存档健壮性约束：
//  - 只在安全阶段（准备阶段）保存。
//  - 读档对所有字段做合法性检查；任何错误都在“尚未修改当前状态”之前返回 false，
//    因此文件缺失/损坏/版本不兼容都不会破坏当前进度，也不会崩溃。
namespace SaveManager {

constexpr int kVersion = 2;

bool save(const GameState& state, const QString& path, QString& errorOut);
bool load(GameState& state, const QString& path, QString& errorOut);

} // namespace SaveManager

#endif // SYSTEMS_SAVEMANAGER_H
