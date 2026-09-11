#pragma once
#include <QGraphicsItem>
#include <QString>
#include "entity/types.h"
class Unit;

// Value snapshot: repainting never dereferences a model object that may have been sold.
class UnitItem final : public QGraphicsItem
{
public:
    explicit UnitItem(const Unit& unit);
    void sync(const Unit& unit, bool selected, bool equipTarget);
    int unitId() const { return m_id; }
    QRectF boundingRect() const override { return {-29, -29, 58, 58}; }
    void paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override;
private:
    int m_id;
    QString m_name, m_equipment;
    Owner m_owner = Owner::Player;
    CombatState m_state = CombatState::Idle;
    int m_star = 1;
    double m_hpRatio = 1, m_manaRatio = 0;
    bool m_shield = false, m_selected = false, m_equipTarget = false;
};
