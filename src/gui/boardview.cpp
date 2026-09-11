#include "gui/boardview.h"
#include "gui/griditem.h"
#include "gui/unititem.h"
#include "gui/equipmentbutton.h"
#include "entity/unit.h"
#include <QApplication>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QGraphicsScene>
#include <QMimeData>
#include <QMouseEvent>
#include <QSet>
#include <cmath>

QPointF BoardView::boardCenter(const QPoint& cell)
{
    return {(cell.x() + 0.5) * Cell, (cell.y() + 0.5) * Cell};
}
QPointF BoardView::benchCenter(int slot)
{
    return {(slot + 0.5) * Cell, BenchTop + Cell / 2};
}
BoardView::BoardView(QWidget* parent) : QGraphicsView(parent)
{
    setScene(new QGraphicsScene(this));
    setObjectName("boardView");
    setAccessibleName(QStringLiteral("棋盘与备战区"));
    setRenderHint(QPainter::Antialiasing);
    setBackgroundBrush(QColor("#182129"));
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setAcceptDrops(true);
    viewport()->setAcceptDrops(true);
    setMouseTracking(true);
    for (int row = 0; row < Board::ROWS; ++row) {
        for (int col = 0; col < Board::COLS; ++col) {
            auto* tile = new GridItem({col * Cell + 1, row * Cell + 1, Cell - 2, Cell - 2}, row >= 4);
            scene()->addItem(tile);
            m_tiles[row * Board::COLS + col] = tile;
        }
    }
    for (int slot = 0; slot < Bench::SLOTS; ++slot) {
        scene()->addRect({slot * Cell + 3, BenchTop + 3, Cell - 6, Cell - 6},
                         QPen(QColor("#627381")), QBrush(QColor("#27313b")));
    }
    const auto label = [this](QString text, QPointF pos, QColor color) {
        auto* item = scene()->addText(text);
        item->setDefaultTextColor(color);
        item->setPos(pos);
    };
    label(QStringLiteral("敌方 / ENEMY"), {0, -31}, QColor("#f4989e"));
    label(QStringLiteral("我方：下半场 · 拖拽布阵"), {Cell * 4, -31}, QColor("#75c4ee"));
    label(QStringLiteral("备战区 / BENCH"), {0, BenchTop - 28}, QColor("#c2cdd7"));
    scene()->setSceneRect(-12, -34, Cell * Board::COLS + 24, BenchTop + Cell + 46);
}
void BoardView::resizeEvent(QResizeEvent* event)
{
    QGraphicsView::resizeEvent(event);
    fitInView(sceneRect(), Qt::KeepAspectRatio);
}
void BoardView::setController(GameController* controller)
{
    if (m_controller) disconnect(m_controller, nullptr, this, nullptr);
    cancelGesture();
    clearSelection();
    m_controller = controller;
    if (controller) {
        connect(controller, &GameController::stateChanged, this, &BoardView::syncFromModel);
        connect(controller, &QObject::destroyed, this, [this] { syncFromModel(); });
    }
    syncFromModel();
}
void BoardView::syncFromModel()
{
    QSet<int> present;
    if (m_controller) {
        const auto& state = m_controller->state();
        if (!m_controller->canEditFormation()) cancelGesture();
        for (const auto& owned : state.units()) {
            const Unit& unit = *owned;
            const int slot = state.bench().slotOf(&unit);
            const bool onBoard = state.board().unitAt(unit.position()) == &unit;
            if (slot < 0 && !onBoard) continue;
            present.insert(unit.id());
            auto* item = m_tokens.value(unit.id());
            if (!item) {
                item = new UnitItem(unit);
                scene()->addItem(item);
                m_tokens.insert(unit.id(), item);
            }
            item->sync(unit, unit.id() == m_selected, unit.id() == m_equipHover);
            if (!(m_dragging && unit.id() == m_pressed)) {
                item->setPos(slot >= 0 ? benchCenter(slot) : boardCenter(unit.position()));
            }
            item->setZValue(m_dragging && unit.id() == m_pressed ? 3 : 2);
        }
    }
    for (int id : m_tokens.keys()) {
        if (!present.contains(id)) delete m_tokens.take(id);
    }
    if (!present.contains(m_pressed)) cancelGesture();
    if (m_selected >= 0 && !present.contains(m_selected)) select(-1);
}
void BoardView::select(int id)
{
    if (m_selected == id) return;
    m_selected = id;
    syncFromModel();
    emit unitSelected(id);
}
void BoardView::clearSelection() { select(-1); }
void BoardView::cancelGesture()
{
    m_pressed = -1;
    m_dragging = false;
    highlight(std::nullopt);
}
void BoardView::highlight(const std::optional<PlacementTarget>& target)
{
    for (auto* tile : m_tiles) tile->highlight(false);
    if (target && target->kind == PlacementTarget::Board && target->boardPos.y() >= 4) {
        m_tiles[target->boardPos.y() * Board::COLS + target->boardPos.x()]->highlight(true);
    }
}
std::optional<PlacementTarget> BoardView::targetAt(const QPointF& p) const
{
    if (p.x() < 0 || p.x() >= Board::COLS * Cell) return std::nullopt;
    const int col = int(std::floor(p.x() / Cell));
    if (p.y() >= 0 && p.y() < Board::ROWS * Cell)
        return PlacementTarget::toBoard({col, int(std::floor(p.y() / Cell))});
    if (p.y() >= BenchTop && p.y() < BenchTop + Cell)
        return PlacementTarget::toBench(col);
    return std::nullopt;
}
UnitItem* BoardView::tokenAt(const QPoint& pos) const
{
    for (auto* item : items(pos)) {
        if (auto* token = dynamic_cast<UnitItem*>(item)) return token;
    }
    return nullptr;
}
void BoardView::mousePressEvent(QMouseEvent* event)
{
    cancelGesture();
    if (event->button() != Qt::LeftButton) {
        QGraphicsView::mousePressEvent(event);
        return;
    }
    if (auto* token = tokenAt(event->pos())) {
        m_pressed = token->unitId();
        m_pressPos = event->pos();
        m_grabOffset = token->pos() - mapToScene(event->pos());
    } else {
        clearSelection();
    }
    event->accept();
}
void BoardView::mouseMoveEvent(QMouseEvent* event)
{
    if (m_controller && m_pressed >= 0 && (event->buttons() & Qt::LeftButton)) {
        if (!m_dragging && (event->pos() - m_pressPos).manhattanLength() >= QApplication::startDragDistance()) {
            const Unit* unit = m_controller->state().findUnit(m_pressed);
            if (!m_controller->canEditFormation() || !unit || unit->owner() != Owner::Player) {
                m_controller->reportFormationLocked();
                cancelGesture();
                return;
            }
            m_dragging = true;
        }
        if (m_dragging) {
            if (auto* token = m_tokens.value(m_pressed)) {
                token->setPos(mapToScene(event->pos()) + m_grabOffset);
                token->setZValue(3);
            }
            highlight(targetAt(mapToScene(event->pos())));
        }
    }
    QGraphicsView::mouseMoveEvent(event);
}
void BoardView::mouseReleaseEvent(QMouseEvent* event)
{
    const int id = m_pressed;
    const bool dragged = m_dragging;
    cancelGesture();
    if (event->button() == Qt::LeftButton && id >= 0) {
        if (dragged && m_controller) {
            if (auto target = targetAt(mapToScene(event->pos())))
                m_controller->requestPlace(id, *target);
        } else {
            select(id == m_selected ? -1 : id);
        }
    }
    syncFromModel(); // Invalid drops snap back to the authoritative model.
    event->accept();
}
bool BoardView::acceptsEquipment(const QMimeData* mime, QObject* source) const
{
    return m_controller && m_controller->canEditFormation()
        && qobject_cast<EquipmentButton*>(source) && mime->hasFormat(EquipmentButton::mimeType());
}
void BoardView::dragEnterEvent(QDragEnterEvent* event)
{
    if (acceptsEquipment(event->mimeData(), event->source())) event->acceptProposedAction();
    else event->ignore();
}
void BoardView::dragMoveEvent(QDragMoveEvent* event)
{
    m_equipHover = -1;
    if (!acceptsEquipment(event->mimeData(), event->source())) { event->ignore(); return; }
    if (auto* token = tokenAt(event->position().toPoint())) {
        const Unit* unit = m_controller->state().findUnit(token->unitId());
        if (unit && unit->isAlive() && unit->owner() == Owner::Player && !unit->hasEquipment())
            m_equipHover = unit->id();
    }
    syncFromModel();
    event->acceptProposedAction();
}
void BoardView::dragLeaveEvent(QDragLeaveEvent* event)
{
    m_equipHover = -1;
    syncFromModel();
    event->accept();
}
void BoardView::dropEvent(QDropEvent* event)
{
    m_equipHover = -1;
    if (!acceptsEquipment(event->mimeData(), event->source())) { event->ignore(); return; }
    bool ok = false;
    const int index = event->mimeData()->data(EquipmentButton::mimeType()).toInt(&ok);
    auto* token = tokenAt(event->position().toPoint());
    if (!ok) { event->ignore(); return; }
    if (token) {
        const int id = token->unitId();
        // Let QDrag's nested event loop unwind before the equipment buttons change.
        QMetaObject::invokeMethod(m_controller, [controller = m_controller, id, index] {
            if (controller) controller->equipUnit(id, index);
        }, Qt::QueuedConnection);
    } else {
        m_controller->reportEquipDropMissed();
    }
    syncFromModel();
    event->acceptProposedAction();
}
