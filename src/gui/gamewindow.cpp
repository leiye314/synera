#include "gui/gamewindow.h"
#include "gui/boardview.h"
#include "gui/equipmentbutton.h"
#include "core/gamecontroller.h"
#include "core/gamestate.h"
#include "entity/unit.h"
#include "entity/herodata.h"
#include "entity/equipment.h"

#include <QFileDialog>
#include <QDir>
#include <QGroupBox>
#include <QScrollArea>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStringList>
#include <QStandardPaths>
#include <QVBoxLayout>
#include <algorithm>

namespace {
QString phaseName(Phase p)
{
    switch (p) {
    case Phase::Prep:    return QStringLiteral("准备");
    case Phase::Combat:  return QStringLiteral("战斗");
    case Phase::Resolve: return QStringLiteral("结算");
    }
    return QString();
}

// 把单位实际生效的属性修正（羁绊 + 装备“从零重建”后的结果）整理成可读文本，
// 让玩家直观看到“该单位实际获得了哪些加成”。
QString modifierSummary(const StatModifiers& m)
{
    QStringList parts;
    if (qAbs(m.flatMaxHp) > 0.5)             parts << QStringLiteral("最大生命 +%1").arg(int(m.flatMaxHp));
    if (qAbs(m.pctMaxHp) > 0.001)            parts << QStringLiteral("最大生命 +%1%").arg(int(m.pctMaxHp * 100));
    if (qAbs(m.flatAttackDamage) > 0.5)      parts << QStringLiteral("攻击 +%1").arg(int(m.flatAttackDamage));
    if (qAbs(m.pctAttackSpeed) > 0.001)      parts << QStringLiteral("攻速 +%1%").arg(int(m.pctAttackSpeed * 100));
    if (qAbs(m.flatArmor) > 0.5)             parts << QStringLiteral("护甲 +%1").arg(int(m.flatArmor));
    if (qAbs(m.damageReductionPct) > 0.001)  parts << QStringLiteral("减伤 +%1%").arg(int(m.damageReductionPct * 100));
    if (qAbs(m.skillDamagePct) > 0.001)      parts << QStringLiteral("技能伤害 +%1%").arg(int(m.skillDamagePct * 100));
    if (m.flatStartMana != 0)                parts << QStringLiteral("初始法力 +%1").arg(m.flatStartMana);
    if (m.flatMaxManaDelta != 0)             parts << QStringLiteral("最大法力 %1%2")
                                                      .arg(m.flatMaxManaDelta > 0 ? QStringLiteral("+") : QString())
                                                      .arg(m.flatMaxManaDelta);
    return parts.isEmpty() ? QStringLiteral("无") : parts.join(QStringLiteral("，"));
}
}

GameWindow::GameWindow(QWidget* parent, quint32 seed)
    : QMainWindow(parent)
    , m_controller(new GameController(this, seed))
    , m_board(new BoardView(this))
    , m_infoLabel(nullptr)
    , m_traitLabel(nullptr)
    , m_selectedLabel(nullptr)
    , m_refreshButton(nullptr)
    , m_upgradeButton(nullptr)
    , m_startButton(nullptr)
    , m_sellButton(nullptr)
    , m_saveButton(nullptr)
    , m_loadButton(nullptr)
    , m_newButton(nullptr)
    , m_equipContainer(nullptr)
    , m_equipLayout(nullptr)
    , m_log(nullptr)
    , m_selectedUnitId(-1)
    , m_gameOver(false)
{
    QString saveDirectory = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (saveDirectory.isEmpty() || !QDir(saveDirectory).exists()) {
        saveDirectory = QDir::homePath();
    }
    m_savePath = QDir(saveDirectory).filePath(QStringLiteral("synera_save.json"));
    setupUI();

    connect(m_controller, &GameController::stateChanged, this, &GameWindow::onStateChanged);
    connect(m_controller, &GameController::phaseChanged, this, &GameWindow::onPhaseChanged);
    connect(m_controller, &GameController::logMessage, this, &GameWindow::onLogMessage);
    connect(m_controller, &GameController::gameOver, this, &GameWindow::onGameOver);

    m_board->setController(m_controller);
    connect(m_board, &BoardView::unitSelected, this, &GameWindow::onUnitSelected);

    m_controller->newGame();
}

GameWindow::~GameWindow() = default;

void GameWindow::setupUI()
{
    setStyleSheet(R"(
        QMainWindow, QWidget { background-color: #232327; color: #f0f0f0; }
        QPushButton {
            background-color: #34343c; color: #f0f0f0; border: 1px solid #565660;
            border-radius: 5px; padding: 6px 8px; font-size: 12px;
        }
        QPushButton:hover:enabled { background-color: #43434d; }
        QPushButton:disabled { color: #777; background-color: #2a2a30; }
        QGroupBox { border: 1px solid #45454f; border-radius: 6px; margin-top: 8px; padding-top: 8px; }
        QGroupBox::title { subcontrol-origin: margin; left: 8px; }
        QPlainTextEdit { background-color: #1c1c20; border: 1px solid #45454f; }
    )");

    QWidget* central = new QWidget(this);
    QHBoxLayout* root = new QHBoxLayout(central);

    // 左：棋盘视图
    root->addWidget(m_board, 1);

    // 右：侧边面板
    QWidget* side = new QWidget(central);
    side->setFixedWidth(330);
    QVBoxLayout* sideLayout = new QVBoxLayout(side);
    sideLayout->setSpacing(6);

    // 玩家信息
    m_infoLabel = new QLabel(side);
    m_infoLabel->setTextFormat(Qt::RichText);
    m_infoLabel->setWordWrap(true);
    sideLayout->addWidget(m_infoLabel);

    // 羁绊
    QGroupBox* traitBox = new QGroupBox(QStringLiteral("羁绊（按不同英雄计数）"), side);
    QVBoxLayout* traitLayout = new QVBoxLayout(traitBox);
    m_traitLabel = new QLabel(traitBox);
    m_traitLabel->setTextFormat(Qt::RichText);
    m_traitLabel->setWordWrap(true);
    traitLayout->addWidget(m_traitLabel);
    sideLayout->addWidget(traitBox);

    // 商店
    QGroupBox* shopBox = new QGroupBox(QStringLiteral("商店"), side);
    QVBoxLayout* shopLayout = new QVBoxLayout(shopBox);
    QHBoxLayout* shopRow = new QHBoxLayout();
    for (int i = 0; i < 5; ++i) {
        QPushButton* btn = new QPushButton(shopBox);
        btn->setMinimumHeight(46);
        connect(btn, &QPushButton::clicked, this, [this, i]() { onShopButtonClicked(i); });
        m_shopButtons.append(btn);
        shopRow->addWidget(btn);
    }
    shopLayout->addLayout(shopRow);
    QHBoxLayout* shopCtrl = new QHBoxLayout();
    m_refreshButton = new QPushButton(QStringLiteral("刷新 (2金)"), shopBox);
    m_upgradeButton = new QPushButton(QStringLiteral("升级人口"), shopBox);
    connect(m_refreshButton, &QPushButton::clicked, m_controller, &GameController::refreshShop);
    connect(m_upgradeButton, &QPushButton::clicked, m_controller, &GameController::upgradePopulation);
    shopCtrl->addWidget(m_refreshButton);
    shopCtrl->addWidget(m_upgradeButton);
    shopLayout->addLayout(shopCtrl);
    sideLayout->addWidget(shopBox);

    // 装备栏
    QGroupBox* equipBox = new QGroupBox(QStringLiteral("装备栏（点击或拖到单位穿戴）"), side);
    QVBoxLayout* equipOuter = new QVBoxLayout(equipBox);
    m_equipContainer = new QWidget(equipBox);
    m_equipLayout = new QHBoxLayout(m_equipContainer);
    m_equipLayout->setContentsMargins(0, 0, 0, 0);
    auto* equipmentScroll = new QScrollArea(equipBox);
    equipmentScroll->setWidgetResizable(true);
    equipmentScroll->setFrameShape(QFrame::NoFrame);
    equipmentScroll->setFixedHeight(62);
    equipmentScroll->setWidget(m_equipContainer);
    equipOuter->addWidget(equipmentScroll);
    sideLayout->addWidget(equipBox);

    // 选中单位信息
    m_selectedLabel = new QLabel(side);
    m_selectedLabel->setWordWrap(true);
    sideLayout->addWidget(m_selectedLabel);

    // 控制按钮
    m_startButton = new QPushButton(QStringLiteral("开始战斗"), side);
    m_startButton->setMinimumHeight(40);
    connect(m_startButton, &QPushButton::clicked, this, &GameWindow::onStartCombat);
    sideLayout->addWidget(m_startButton);

    QHBoxLayout* ctrlRow = new QHBoxLayout();
    m_sellButton = new QPushButton(QStringLiteral("卖出选中"), side);
    m_newButton = new QPushButton(QStringLiteral("新游戏"), side);
    connect(m_sellButton, &QPushButton::clicked, this, &GameWindow::onSellSelected);
    connect(m_newButton, &QPushButton::clicked, this, &GameWindow::onNewGame);
    ctrlRow->addWidget(m_sellButton);
    ctrlRow->addWidget(m_newButton);
    sideLayout->addLayout(ctrlRow);

    QHBoxLayout* saveRow = new QHBoxLayout();
    m_saveButton = new QPushButton(QStringLiteral("存档"), side);
    m_loadButton = new QPushButton(QStringLiteral("读档"), side);
    connect(m_saveButton, &QPushButton::clicked, this, &GameWindow::onSave);
    connect(m_loadButton, &QPushButton::clicked, this, &GameWindow::onLoad);
    saveRow->addWidget(m_saveButton);
    saveRow->addWidget(m_loadButton);
    sideLayout->addLayout(saveRow);

    // 日志
    m_log = new QPlainTextEdit(side);
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(1200);
    m_log->setMinimumHeight(140);
    sideLayout->addWidget(m_log, 1);

    auto* sideScroll = new QScrollArea(central);
    sideScroll->setWidgetResizable(true);
    sideScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    sideScroll->setFrameShape(QFrame::NoFrame);
    sideScroll->setFixedWidth(356);
    sideScroll->setWidget(side);
    root->addWidget(sideScroll);
    setCentralWidget(central);
}

void GameWindow::refresh()
{
    const GameState& s = m_controller->state();
    const Player& p = s.player();
    const bool prep = (s.phase() == Phase::Prep) && !m_gameOver;

    const int interest = std::min(p.gold() / 10, 5);
    // 准备阶段不限时（不引入自动倒计时）；其余阶段显示当前状态说明。
    QString timeHint;
    if (m_gameOver) {
        timeHint = QStringLiteral("游戏已结束");
    } else {
        switch (s.phase()) {
        case Phase::Prep:    timeHint = QStringLiteral("不限时"); break;
        case Phase::Combat:  timeHint = QStringLiteral("战斗进行中"); break;
        case Phase::Resolve: timeHint = QStringLiteral("正在结算"); break;
        }
    }
    m_infoLabel->setText(QStringLiteral(
        "<b>生命</b> %1 &nbsp; <b>金币</b> %2 (利息+%3)<br>"
        "<b>人口</b> %4/%5 &nbsp; <b>等级</b> %6<br>"
        "<b>关卡</b> %7/%8 &nbsp; <b>阶段</b> %9<br>"
        "<b>连胜</b> %10 &nbsp; <b>连败</b> %11<br>"
        "<b>准备时间</b> %12")
        .arg(p.hp()).arg(p.gold()).arg(interest)
        .arg(s.populationUsed()).arg(s.populationCap()).arg(p.level())
        .arg(s.round()).arg(s.maxRounds()).arg(phaseName(s.phase()))
        .arg(p.winStreak()).arg(p.lossStreak())
        .arg(timeHint));

    m_traitLabel->setText(m_controller->traitSummary());

    // 商店
    const QVector<ShopSlot>& shop = s.shop();
    for (int i = 0; i < m_shopButtons.size(); ++i) {
        QPushButton* btn = m_shopButtons[i];
        if (i < shop.size() && shop[i].filled) {
            const HeroData::HeroInfo& info = HeroData::info(shop[i].type);
            btn->setText(QStringLiteral("%1\n%2金").arg(info.name).arg(info.cost));
            btn->setEnabled(prep);
        } else {
            btn->setText(QStringLiteral("—"));
            btn->setEnabled(false);
        }
    }

    const int upCost = p.upgradeCost();
    m_upgradeButton->setText(upCost < 0
        ? QStringLiteral("人口已满级")
        : QStringLiteral("升级人口 →%1 (%2金)").arg(p.level() + 1).arg(upCost));
    m_upgradeButton->setEnabled(prep && upCost >= 0);
    m_refreshButton->setEnabled(prep);

    m_startButton->setText(s.phase() == Phase::Combat
        ? QStringLiteral("战斗进行中…") : QStringLiteral("开始战斗"));
    m_startButton->setEnabled(prep);

    m_sellButton->setEnabled(prep && m_selectedUnitId >= 0);
    m_saveButton->setEnabled(prep);
    m_loadButton->setEnabled(!m_controller->isCombatActive());

    rebuildEquipmentButtons();

    // 选中单位详情：展示当前有效属性、装备与“该单位实际获得的羁绊/装备修正”。
    Unit* u = (m_selectedUnitId >= 0) ? m_controller->state().findUnit(m_selectedUnitId) : nullptr;
    if (u) {
        QString traits;
        for (Trait t : u->traits()) {
            traits += traitName(t) + QStringLiteral(" ");
        }
        const QString equip = u->hasEquipment()
            ? QStringLiteral("%1（%2）").arg(EquipmentData::name(u->equipment()),
                                            EquipmentData::description(u->equipment()))
            : QStringLiteral("无");
        m_selectedLabel->setTextFormat(Qt::RichText);
        m_selectedLabel->setText(QStringLiteral(
            "<b>选中：%1</b> %2★<br>"
            "羁绊：%3<br>"
            "生命 %4/%5 &nbsp; 攻击 %6 &nbsp; 护甲 %7<br>"
            "射程 %8 &nbsp; 攻击间隔 %9s &nbsp; 最大法力 %10<br>"
            "装备：%11<br>"
            "当前加成：%12<br>"
            "技能：%13 — %14")
            .arg(u->name()).arg(u->star()).arg(traits.trimmed())
            .arg(int(u->hp())).arg(int(u->effectiveMaxHp())).arg(int(u->effectiveAttackDamage()))
            .arg(int(u->effectiveArmor()))
            .arg(QString::number(u->effectiveAttackRange(), 'f', 1))
            .arg(QString::number(u->effectiveAttackIntervalSec(), 'f', 2))
            .arg(u->effectiveMaxMana())
            .arg(equip)
            .arg(modifierSummary(u->modifiers()))
            .arg(u->skillName(), u->skillDescription()));
    } else {
        m_selectedLabel->setText(QStringLiteral("未选择单位（点击棋盘/备战区单位选中）。"));
    }
}

void GameWindow::rebuildEquipmentButtons()
{
    const auto& currentPool = m_controller->state().equipmentPool();
    const bool editable = m_controller->canEditFormation();
    if (!m_equipButtons.isEmpty() && currentPool == m_displayedEquipment && editable == m_equipmentPrep) return;
    m_displayedEquipment = currentPool;
    m_equipmentPrep = editable;
    for (QPushButton* b : m_equipButtons) {
        m_equipLayout->removeWidget(b);
        b->hide();
        b->deleteLater();
    }
    m_equipButtons.clear();

    const QVector<EquipmentType>& pool = m_controller->state().equipmentPool();
    const bool prep = (m_controller->state().phase() == Phase::Prep) && !m_gameOver;
    if (pool.isEmpty()) {
        QPushButton* placeholder = new QPushButton(QStringLiteral("（暂无装备）"), m_equipContainer);
        placeholder->setEnabled(false);
        m_equipLayout->addWidget(placeholder);
        m_equipButtons.append(placeholder);
        return;
    }
    for (int i = 0; i < pool.size(); ++i) {
        // EquipmentButton 同时支持“点击穿戴”和“拖拽穿戴”，两者共用 GameController::equipUnit。
        EquipmentButton* b = new EquipmentButton(i, pool[i], prep, m_equipContainer);
        connect(b, &QPushButton::clicked, this, [this, i]() { onEquipButtonClicked(i); });
        m_equipLayout->addWidget(b);
        m_equipButtons.append(b);
    }
}

// ---------------- 槽函数 ----------------

void GameWindow::onStateChanged() { refresh(); }

void GameWindow::onPhaseChanged(Phase) { refresh(); }

void GameWindow::onLogMessage(const QString& msg)
{
    m_log->appendPlainText(msg);
}

void GameWindow::onGameOver(bool playerWon)
{
    m_gameOver = true;
    refresh();
    QMessageBox::information(this, QStringLiteral("游戏结束"),
        playerWon ? QStringLiteral("恭喜！你击败了全部预设关卡，取得最终胜利！")
                  : QStringLiteral("生命归零，游戏失败。点击“新游戏”重新开始。"));
}

void GameWindow::onUnitSelected(int unitId)
{
    m_selectedUnitId = unitId;
    refresh();
}

void GameWindow::onShopButtonClicked(int slot)
{
    m_controller->buyUnit(slot);
}

void GameWindow::onEquipButtonClicked(int poolIndex)
{
    if (m_selectedUnitId < 0) {
        onLogMessage(QStringLiteral("请先点击选中一个单位，再穿戴装备。"));
        return;
    }
    m_controller->equipUnit(m_selectedUnitId, poolIndex);
}

void GameWindow::onStartCombat()
{
    m_board->clearSelection();
    m_selectedUnitId = -1;
    m_controller->startCombat();
}

void GameWindow::onSellSelected()
{
    if (m_selectedUnitId >= 0) {
        const int id = m_selectedUnitId;
        m_board->clearSelection();
        m_selectedUnitId = -1;
        m_controller->sellUnit(id);
    }
}

void GameWindow::onSave()
{
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("保存存档"), m_savePath,
        QStringLiteral("Synera 存档 (*.json)"));
    if (!path.isEmpty() && m_controller->saveGame(path)) {
        m_savePath = path;
    }
}

void GameWindow::onLoad()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("读取存档"), m_savePath,
        QStringLiteral("Synera 存档 (*.json)"));
    if (!path.isEmpty()) {
        m_board->clearSelection();
        m_selectedUnitId = -1;
        if (m_controller->loadGame(path)) {
            m_savePath = path;
            m_gameOver = false;
            refresh();
        }
    }
}

void GameWindow::onNewGame()
{
    m_gameOver = false;
    m_board->clearSelection();
    m_selectedUnitId = -1;
    m_controller->newGame();
}
