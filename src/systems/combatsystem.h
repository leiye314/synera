#ifndef SYSTEMS_COMBATSYSTEM_H
#define SYSTEMS_COMBATSYSTEM_H

#include <functional>
#include <vector>
#include <QString>

class GameState;
class Unit;

// 战斗结果。
enum class CombatResult {
    Ongoing,
    PlayerWin,
    PlayerLose
};

// CombatSystem：由 GameController 的 QTimer 每 100ms 调用一次 tick()。
//
// 单个 tick 内：
//  1) 清理本 tick 已死亡单位的棋盘占位。
//  2) 每个存活单位按稳定顺序（id）执行状态机：索敌 → 满法力则施法 → 在范围内则普攻 → 否则移动（仅生成意图）。
//  3) 统一提交移动意图：按稳定顺序占用空格，禁止两个单位进入同一格、禁止重叠。
//  4) 判定胜负。
//
// 不阻塞 GUI：每次只推进一帧，决不在内部跑 while 主循环。
class CombatSystem
{
public:
    explicit CombatSystem(std::function<void(const QString&)> logger);

    int tickIntervalMs() const { return 100; }

    void begin(GameState& state);          // 收集参战单位、重置战斗属性、应用双方羁绊
    CombatResult tick(GameState& state);   // 推进一帧

private:
    // 为 unit 选择目标：最小化欧氏距离平方；平局按（更低 HP → 更靠左 x → 更靠下 y）决胜。
    Unit* acquireTarget(GameState& state, Unit* unit) const;
    CombatResult evaluate(GameState& state) const;

    std::function<void(const QString&)> m_logger;
    std::vector<Unit*> m_participants;
    int m_elapsedTicks;
    int m_maxTicks;
};

#endif // SYSTEMS_COMBATSYSTEM_H
