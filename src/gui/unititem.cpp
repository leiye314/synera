#include "gui/unititem.h"
#include "entity/unit.h"
#include "entity/equipment.h"
#include <QPainter>
#include <algorithm>

UnitItem::UnitItem(const Unit& unit) : m_id(unit.id())
{
    setAcceptedMouseButtons(Qt::NoButton);
    setZValue(2);
    sync(unit, false, false);
}
void UnitItem::sync(const Unit& u, bool selected, bool equipTarget)
{
    m_name = u.name();
    m_owner = u.owner();
    m_star = u.star();
    m_state = u.state();
    m_hpRatio = std::clamp(u.hp() / std::max(1.0, u.effectiveMaxHp()), 0.0, 1.0);
    m_manaRatio = double(u.mana()) / std::max(1, u.effectiveMaxMana());
    m_equipment = u.hasEquipment() ? EquipmentData::name(u.equipment()).left(1) : QString();
    m_shield = u.shield() > 0;
    m_selected = selected;
    m_equipTarget = equipTarget;
    setToolTip(QStringLiteral("%1 %2★\nHP %3/%4 · Mana %5/%6\n%7: %8")
        .arg(m_name).arg(m_star).arg(int(u.hp())).arg(int(u.effectiveMaxHp()))
        .arg(u.mana()).arg(u.effectiveMaxMana()).arg(u.skillName(), u.skillDescription()));
    update();
}
void UnitItem::paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*)
{
    const QColor accent = m_owner == Owner::Player ? QColor("#75c4ee") : QColor("#f4989e");
    p->setOpacity(m_state == CombatState::Dead ? 0.35 : 1.0);
    p->setPen(QPen(m_equipTarget ? QColor("#8dffc3") : (m_selected ? QColor("#ffe19c") : accent),
                  m_selected || m_equipTarget ? 3 : 1.5));
    p->setBrush(m_owner == Owner::Player ? QColor("#123246") : QColor("#462532"));
    p->drawRoundedRect(QRectF(-26, -27, 52, 54), 9, 9);
    if (m_state == CombatState::Casting || m_shield) {
        p->setPen(QPen(m_shield ? QColor("#bcaaff") : QColor("#ffde80"), 2));
        p->drawEllipse(QRectF(-22, -23, 44, 38));
    }
    QFont font = p->font();
    font.setPixelSize(13);
    font.setBold(true);
    p->setFont(font);
    p->setPen(Qt::white);
    p->drawText(QRectF(-25, -18, 50, 22), Qt::AlignCenter, m_name);
    font.setPixelSize(10);
    p->setFont(font);
    p->setPen(QColor("#ffe19c"));
    p->drawText(QRectF(-23, -28, 46, 12), Qt::AlignCenter, QString(m_star, QChar(0x2605)));
    p->setPen(Qt::NoPen);
    const auto bar = [p](qreal y, double ratio, QColor color) {
        p->setBrush(QColor("#101820"));
        p->drawRect(QRectF(-21, y, 42, 4));
        p->setBrush(color);
        p->drawRect(QRectF(-21, y, 42 * ratio, 4));
    };
    bar(10, m_hpRatio, QColor("#72d5a0"));
    bar(17, m_manaRatio, QColor("#69baff"));
    if (!m_equipment.isEmpty()) {
        p->setPen(QColor("#ffe19c"));
        p->drawText(QRectF(14, -10, 13, 18), Qt::AlignCenter, m_equipment);
    }
}
