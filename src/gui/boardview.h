#pragma once
#include <QGraphicsView>
#include <QHash>
#include <QPointer>
#include <array>
#include <optional>
#include "core/gamecontroller.h"

class QMimeData;
class GridItem;
class UnitItem;

// Scene owns all items. Commands use IDs; model pointers never escape a refresh.
class BoardView final : public QGraphicsView
{
    Q_OBJECT
public:
    explicit BoardView(QWidget* parent = nullptr);
    void setController(GameController* controller);
    int selectedUnitId() const { return m_selected; }
    void clearSelection();
public slots:
    void syncFromModel();
signals:
    void unitSelected(int unitId);
protected:
    void resizeEvent(QResizeEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void dragEnterEvent(QDragEnterEvent*) override;
    void dragMoveEvent(QDragMoveEvent*) override;
    void dragLeaveEvent(QDragLeaveEvent*) override;
    void dropEvent(QDropEvent*) override;
private:
    static constexpr qreal Cell = 66;
    static constexpr qreal BenchTop = Cell * Board::ROWS + 38;
    static QPointF boardCenter(const QPoint& cell);
    static QPointF benchCenter(int slot);
    std::optional<PlacementTarget> targetAt(const QPointF& pos) const;
    UnitItem* tokenAt(const QPoint& viewportPos) const;
    void select(int id);
    void highlight(const std::optional<PlacementTarget>& target);
    void cancelGesture();
    bool acceptsEquipment(const QMimeData* mime, QObject* source) const;

    QPointer<GameController> m_controller;
    std::array<GridItem*, Board::ROWS * Board::COLS> m_tiles{};
    QHash<int, UnitItem*> m_tokens;
    int m_selected = -1, m_pressed = -1, m_equipHover = -1;
    QPoint m_pressPos;
    QPointF m_grabOffset;
    bool m_dragging = false;
};
