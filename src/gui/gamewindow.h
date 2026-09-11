#ifndef GUI_GAMEWINDOW_H
#define GUI_GAMEWINDOW_H

#include <QMainWindow>
#include <QVector>
#include "entity/types.h"

class GameController;
class BoardView;
class QLabel;
class QPushButton;
class QPlainTextEdit;
class QHBoxLayout;
class QWidget;

// 主窗口：组织棋盘视图 + 侧边信息/经营面板 + 控制按钮 + 日志。
// 它只把按钮/选择翻译成 GameController 命令，并在收到信号后刷新展示。
class GameWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit GameWindow(QWidget* parent = nullptr, quint32 seed = 0);
    ~GameWindow() override;

private slots:
    void onStateChanged();
    void onPhaseChanged(Phase phase);
    void onLogMessage(const QString& msg);
    void onGameOver(bool playerWon);
    void onUnitSelected(int unitId);

    void onShopButtonClicked(int slot);
    void onEquipButtonClicked(int poolIndex);
    void onStartCombat();
    void onSellSelected();
    void onSave();
    void onLoad();
    void onNewGame();

private:
    void setupUI();
    void refresh();
    void rebuildEquipmentButtons();

    GameController* m_controller;
    BoardView* m_board;

    QLabel* m_infoLabel;
    QLabel* m_traitLabel;
    QLabel* m_selectedLabel;

    QVector<QPushButton*> m_shopButtons;
    QPushButton* m_refreshButton;
    QPushButton* m_upgradeButton;
    QPushButton* m_startButton;
    QPushButton* m_sellButton;
    QPushButton* m_saveButton;
    QPushButton* m_loadButton;
    QPushButton* m_newButton;

    QWidget* m_equipContainer;
    QHBoxLayout* m_equipLayout;
    QVector<QPushButton*> m_equipButtons;

    QPlainTextEdit* m_log;

    int m_selectedUnitId;
    bool m_gameOver;
    QVector<EquipmentType> m_displayedEquipment;
    bool m_equipmentPrep = false;
    QString m_savePath;
};

#endif // GUI_GAMEWINDOW_H
