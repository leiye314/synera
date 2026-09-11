#ifndef CORE_BENCH_H
#define CORE_BENCH_H

#include <QVector>

class Unit;

// 固定 8 槽的备战区，存放未上阵的我方单位。
// 只持有非拥有指针；单位本体由 GameState 用 unique_ptr 持有。
class Bench
{
public:
    static constexpr int SLOTS = 8;

    Bench();

    int firstEmptySlot() const;          // 无空位返回 -1
    bool isFull() const { return firstEmptySlot() < 0; }
    int count() const;

    bool addUnit(Unit* unit);            // 放入第一个空槽，满则失败
    bool placeAt(int slot, Unit* unit);  // 放到指定空槽
    void removeAt(int slot);
    void removeUnit(Unit* unit);

    Unit* unitAt(int slot) const;
    int slotOf(const Unit* unit) const;        // 不在备战区返回 -1
    bool isValidSlot(int slot) const { return slot >= 0 && slot < SLOTS; }

    QVector<Unit*> allUnits() const;     // 非空槽中的单位
    void clear();

private:
    QVector<Unit*> m_slots;
};

#endif // CORE_BENCH_H
