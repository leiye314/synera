#include "systems/pathfinder.h"
#include "core/board.h"

#include <array>
#include <cmath>
#include <queue>
#include <vector>

namespace Pathfinder {

bool inAttackRange(const QPoint& a, const QPoint& b, double range)
{
    const double dx = a.x() - b.x();
    const double dy = a.y() - b.y();
    return dx * dx + dy * dy <= range * range + 1e-6;
}

bool nextStep(const Board& board, const QPoint& start, const QPoint& target,
              double range, QPoint& outStep)
{
    if (!board.isValidPosition(start) || !board.isValidPosition(target)
        || !std::isfinite(range) || range < 0) return false;
    if (inAttackRange(start, target, range)) {
        return false; // 已在范围内，无需移动
    }

    const int cols = Board::COLS;
    const int rows = Board::ROWS;
    auto idx = [cols](const QPoint& p) { return p.y() * cols + p.x(); };

    std::vector<int> prev(rows * cols, -1);   // 前驱格索引
    std::vector<char> visited(rows * cols, 0);

    std::queue<QPoint> q;
    q.push(start);
    visited[idx(start)] = 1;

    const std::array<QPoint, 4> dirs = {
        QPoint(1, 0), QPoint(-1, 0), QPoint(0, 1), QPoint(0, -1)
    };

    QPoint goal(-1, -1);
    while (!q.empty()) {
        const QPoint cur = q.front();
        q.pop();

        if (cur != start && inAttackRange(cur, target, range)) {
            goal = cur; // BFS 顺序保证这是最近的可攻击格
            break;
        }

        for (const QPoint& d : dirs) {
            const QPoint nxt(cur.x() + d.x(), cur.y() + d.y());
            if (!board.isValidPosition(nxt) || visited[idx(nxt)]) {
                continue;
            }
            // 其它单位阻挡：除起点外，被占用的格不可进入。
            if (board.hasUnitAt(nxt)) {
                continue;
            }
            visited[idx(nxt)] = 1;
            prev[idx(nxt)] = idx(cur);
            q.push(nxt);
        }
    }

    if (goal.x() < 0) {
        return false; // 无可达的攻击位
    }

    // 回溯到 start 的下一步：沿 prev 一直走到“前驱是 start”的那个格。
    QPoint step = goal;
    while (prev[idx(step)] != idx(start)) {
        const int p = prev[idx(step)];
        if (p < 0) {
            return false; // 理论上不会发生
        }
        step = QPoint(p % cols, p / cols);
    }
    outStep = step;
    return true;
}

} // namespace Pathfinder
