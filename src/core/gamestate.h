#ifndef CORE_GAMESTATE_H
#define CORE_GAMESTATE_H

#include <memory>
#include <vector>
#include <QVector>
#include "core/board.h"
#include "core/bench.h"
#include "core/player.h"
#include "core/randomstream.h"
#include "entity/types.h"

class Unit;

// 商店的一个格子。
struct ShopSlot {
    bool filled = false;
    HeroType type = HeroType::Warrior;
};

// GameState 是可序列化的“游戏模型”，集中持有全部运行时状态。
//
// 所有权（核心约束）：所有 Unit 由 GameState 通过 unique_ptr 集中持有；
// Board / Bench / 各系统只使用非拥有指针或稳定 id。删除单位的唯一入口是 removeUnit()，
// 它会同时把单位从棋盘/备战区移除，避免悬挂指针与双重删除。
class GameState
{
public:
    explicit GameState(quint32 seed = 0);
    ~GameState();
    GameState(GameState&&) noexcept;
    GameState& operator=(GameState&&) noexcept;
    GameState(const GameState&) = delete;
    GameState& operator=(const GameState&) = delete;

    RandomStream& random() { return m_random; }
    const RandomStream& random() const { return m_random; }

    // ---- 子模块访问 ----
    Player& player() { return m_player; }
    const Player& player() const { return m_player; }
    Board& board() { return m_board; }
    const Board& board() const { return m_board; }
    Bench& bench() { return m_bench; }
    const Bench& bench() const { return m_bench; }

    // ---- 阶段与轮次 ----
    Phase phase() const { return m_phase; }
    void setPhase(Phase p) { m_phase = p; }
    int round() const { return m_round; }
    void setRound(int r) { m_round = r; }
    int maxRounds() const { return m_maxRounds; }

    // ---- 商店与装备池 ----
    QVector<ShopSlot>& shop() { return m_shop; }
    const QVector<ShopSlot>& shop() const { return m_shop; }
    QVector<EquipmentType>& equipmentPool() { return m_equipmentPool; }
    const QVector<EquipmentType>& equipmentPool() const { return m_equipmentPool; }

    // ---- 单位所有权 ----
    Unit* addUnit(std::unique_ptr<Unit> unit);   // 接管所有权，返回裸指针
    void removeUnit(Unit* unit);                  // 从模型彻底删除（含棋盘/备战区）
    void clearUnits();                            // 删除全部单位
    void removeEnemyUnits();                       // 战斗结束清理敌人

    Unit* findUnit(int id) const;
    const std::vector<std::unique_ptr<Unit>>& units() const { return m_units; }

    // ---- 派生查询 ----
    QVector<Unit*> playerRoster() const;          // 我方棋盘 + 备战区
    QVector<Unit*> playerBoardUnits() const;      // 我方上阵
    QVector<Unit*> enemyBoardUnits() const;       // 敌方上阵
    int populationUsed() const { return static_cast<int>(playerBoardUnits().size()); }
    int populationCap() const { return m_player.level(); }

    void resetToNewGame();                         // 重新开始一局

private:
    RandomStream m_random;
    Player m_player;
    Board m_board;
    Bench m_bench;
    Phase m_phase;
    int m_round;
    int m_maxRounds;

    std::vector<std::unique_ptr<Unit>> m_units;
    QVector<ShopSlot> m_shop;
    QVector<EquipmentType> m_equipmentPool;
};

#endif // CORE_GAMESTATE_H
