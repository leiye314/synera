#ifndef SYSTEMS_PATHFINDER_H
#define SYSTEMS_PATHFINDER_H

#include <QPoint>

class Board;

// 寻路：在 8×8 无权网格上用 BFS 求最短路。
// BFS 适用于等权网格，可求最短路并保持实现简洁。
namespace Pathfinder {

// a 与 b 的欧氏距离是否 <= range（用平方比较避免开方）。
bool inAttackRange(const QPoint& a, const QPoint& b, double range);

// 计算从 start 出发、朝“能攻击到 target 的最近可达格”前进的下一步（4 邻接）。
//  - 其他单位（board 上的占位）视为阻挡；start 自身格视为可离开。
//  - target 所在格本身是占用的，不要求进入。
// 返回 true 并写出 outStep（下一格）表示应当移动；返回 false 表示无需/无法移动
// （已在攻击范围内，或被完全阻挡）。调用方应另行用 inAttackRange 判断“已在范围内”。
bool nextStep(const Board& board, const QPoint& start, const QPoint& target,
              double range, QPoint& outStep);

} // namespace Pathfinder

#endif // SYSTEMS_PATHFINDER_H
