#include "core/board.h"
#include "entity/unit.h"
#include <algorithm>
#include <utility>

std::size_t Board::offset(const QPoint& pos)
{
    return static_cast<std::size_t>(pos.y() * COLS + pos.x());
}
bool Board::isValidPosition(const QPoint& pos) const
{
    return pos.x() >= 0 && pos.y() >= 0 && pos.x() < COLS && pos.y() < ROWS;
}
bool Board::isPlayerHalf(const QPoint& pos) const
{
    return isValidPosition(pos) && pos.y() >= ROWS / 2;
}
bool Board::isEnemyHalf(const QPoint& pos) const
{
    return isValidPosition(pos) && !isPlayerHalf(pos);
}
Unit* Board::unitAt(const QPoint& pos) const
{
    return isValidPosition(pos) ? m_cells[offset(pos)] : nullptr;
}
bool Board::placeUnit(Unit* unit, const QPoint& pos)
{
    if (!unit || !isValidPosition(pos) || hasUnitAt(pos)) return false;
    removeUnit(unit);
    m_cells[offset(pos)] = unit;
    unit->setPosition(pos);
    return true;
}
void Board::removeUnit(Unit* unit)
{
    if (!unit) return;
    const auto found = std::find(m_cells.begin(), m_cells.end(), unit);
    if (found == m_cells.end()) return;
    *found = nullptr;
    unit->setPosition({-1, -1});
}
void Board::removeAt(const QPoint& pos) { removeUnit(unitAt(pos)); }
bool Board::moveUnit(const QPoint& from, const QPoint& to)
{
    Unit* unit = unitAt(from);
    if (!unit || !isValidPosition(to) || hasUnitAt(to)) return false;
    std::swap(m_cells[offset(from)], m_cells[offset(to)]);
    unit->setPosition(to);
    return true;
}
bool Board::swapUnits(const QPoint& a, const QPoint& b)
{
    Unit* first = unitAt(a);
    Unit* second = unitAt(b);
    if (!first || !second || a == b) return false;
    std::swap(m_cells[offset(a)], m_cells[offset(b)]);
    first->setPosition(b);
    second->setPosition(a);
    return true;
}
QVector<Unit*> Board::allUnits() const
{
    QVector<Unit*> occupied;
    for (Unit* unit : m_cells) if (unit) occupied.append(unit);
    return occupied;
}
void Board::clear()
{
    for (Unit*& unit : m_cells) {
        if (unit) unit->setPosition({-1, -1});
        unit = nullptr;
    }
}
