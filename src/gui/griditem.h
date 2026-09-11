#pragma once
#include <QGraphicsRectItem>

// Passive tile; BoardView owns hit testing and input.
class GridItem final : public QGraphicsRectItem
{
public:
    GridItem(const QRectF& rect, bool friendly);
    void highlight(bool active);
    void paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override;
private:
    QColor m_fill;
    bool m_highlight = false;
};
