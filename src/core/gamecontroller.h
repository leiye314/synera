#ifndef CORE_GAMECONTROLLER_H
#define CORE_GAMECONTROLLER_H

#include <QObject>
#include <QHash>
#include <QPoint>
#include <QString>
#include <memory>
#include "core/gamestate.h"
#include "entity/types.h"

class QTimer;
class CombatSystem;

// 拖放目标：棋盘格或备战槽。GUI 把鼠标位置解析成它，再交给控制器统一校验。
struct PlacementTarget {
    enum Kind { Board, Bench } kind = Board;
    QPoint boardPos = QPoint(-1, -1);
    int benchSlot = -1;

    static PlacementTarget toBoard(const QPoint& p) { PlacementTarget t; t.kind = Board; t.boardPos = p; return t; }
    static PlacementTarget toBench(int slot) { PlacementTarget t; t.kind = Bench; t.benchSlot = slot; return t; }
};

// GameController 是“系统协调者”：持有 GameState 与各系统，向 GUI 暴露命令与信号。
// 它负责阶段切换、输入命令校验、QTimer 驱动的非阻塞战斗、结算与经济。
// 关键约束：所有规则都在这里及各系统中，GUI 只发命令、收信号、做展示。
class GameController : public QObject
{
    Q_OBJECT

public:
    explicit GameController(QObject* parent = nullptr, quint32 seed = 0);
    ~GameController() override;

    const GameState& state() const { return m_state; }
    GameState& state() { return m_state; }

    bool isCombatActive() const;

    // 当前是否允许调整阵容（仅准备阶段且未结束整局）。
    bool canEditFormation() const { return !m_gameEnded && m_state.phase() == Phase::Prep; }
    // 当阵容编辑被拦截时，按当前阶段给出对应提示（战斗 / 结算 / 已结束）。
    void reportFormationLocked();

    // 当前我方上阵单位的羁绊状态（供 GUI 显示）。
    // 返回类型放在 cpp，用 traitStatusText() 提供格式化文本，避免头文件耦合。
    QString traitSummary() const;

    // 一轮战斗结算后的关卡状态转移（纯规则，便于确定性测试）：
    //  - 生命归零：整局失败。
    //  - 胜利且为最后一关：整局胜利。
    //  - 胜利且非最后一关：进入下一关。
    //  - 失败但仍有生命：保持当前关，返回准备阶段重试本关。
    enum class RoundOutcome { Defeat, Victory, Advance, Retry };
    static RoundOutcome decideRoundOutcome(bool playerWon, bool playerDefeated,
                                           int finishedRound, int maxRounds);

    // 记录当前上阵阵型，战斗结束后据此复活并归位。startCombat 调用；
    // 也供无界面回归测试在不跑随机战斗时构造一轮结算。
    void capturePlayerFormation();

    // 按给定战斗结果执行经济结算、清理战场并推进关卡状态机。
    // 由战斗计时器结束时经 resolveCombat() 间接调用，也供无界面回归测试直接调用。
    // 注意：这是逻辑层入口，GUI 不直接调用，更不是作弊/调试按钮。
    void applyRoundResult(bool playerWon);

public slots:
    // ---- 准备阶段命令 ----
    void newGame();
    void buyUnit(int shopSlot);
    void refreshShop();           // 付费刷新
    void upgradePopulation();
    void sellUnit(int unitId);
    bool requestPlace(int unitId, const PlacementTarget& target);
    // 装备穿戴的统一业务入口：点击穿戴与拖拽穿戴都调用它，规则只此一处。
    bool equipUnit(int unitId, int poolIndex);
    // 装备被拖到空白/非单位处时由 GUI 调用：给出清晰提示且不消耗装备。
    void reportEquipDropMissed();

    // ---- 战斗 ----
    void startCombat();

    // ---- 存档 ----
    bool saveGame(const QString& path);
    bool loadGame(const QString& path);

signals:
    void stateChanged();                 // 模型变化，GUI 需整体重绘
    void logMessage(const QString& msg); // 文本反馈（非法操作/战斗事件/结算）
    void phaseChanged(Phase phase);
    void gameOver(bool playerWon);

private slots:
    void onCombatTick();

private:
    void startPrepRound(bool freshShopAndIncome);
    void recomputePlayerModifiers();
    void normalizePlayerRosterForPrep();
    void checkStarUps();
    void spawnEnemies(int round);
    void resolveCombat(int result);      // result 为 CombatResult 的 int 值；转发到 applyRoundResult
    void emitAll(const QString& logMsg = QString());

    void log(const QString& msg) { emit logMessage(msg); }

    GameState m_state;
    std::unique_ptr<CombatSystem> m_combat;
    QTimer* m_combatTimer;

    // 战斗前的我方阵型快照，战斗后据此复活并归位。
    QHash<int, QPoint> m_playerSnapshot;
    bool m_gameEnded;
};

#endif // CORE_GAMECONTROLLER_H
