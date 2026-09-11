#pragma once
#include <array>
#include <QPoint>
#include <QVector>
class Unit;

// Non-owning row-major occupancy. GameState detaches units before deleting them.
// Scanning 64 cells keeps a single source of truth without a mutable reverse index.
class Board final
{
public:
    static constexpr int ROWS = 8;
    static constexpr int COLS = 8;
    bool placeUnit(Unit* unit, const QPoint& pos);
    void removeUnit(Unit* unit);
    void removeAt(const QPoint& pos);
    bool moveUnit(const QPoint& from, const QPoint& to);
    bool swapUnits(const QPoint& a, const QPoint& b);
    Unit* unitAt(const QPoint& pos) const;
    bool hasUnitAt(const QPoint& pos) const { return unitAt(pos) != nullptr; }
    bool isBlocked(const QPoint& pos) const { return hasUnitAt(pos); }
    bool isValidPosition(const QPoint& pos) const;
    bool isPlayerHalf(const QPoint& pos) const;
    bool isEnemyHalf(const QPoint& pos) const;
    QVector<Unit*> allUnits() const;
    void clear();
private:
    static std::size_t offset(const QPoint& pos);
    std::array<Unit*, ROWS * COLS> m_cells{};
};
