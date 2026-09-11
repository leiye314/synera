#include "gui/griditem.h"
#include <QPainter>

GridItem::GridItem(const QRectF& rect, bool friendly)
    : QGraphicsRectItem(rect), m_fill(friendly ? QColor("#23394b") : QColor("#49313b"))
{
    setAcceptedMouseButtons(Qt::NoButton);
    setPen(Qt::NoPen);
}
void GridItem::highlight(bool active)
{
    if (m_highlight == active) return;
    m_highlight = active;
    update();
}
void GridItem::paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*)
{
    p->setPen(QPen(m_highlight ? QColor("#83e5b6") : m_fill.lighter(125), 1));
    p->setBrush(m_highlight ? m_fill.lighter(160) : m_fill);
    p->drawRoundedRect(rect().adjusted(1, 1, -1, -1), 5, 5);
}
