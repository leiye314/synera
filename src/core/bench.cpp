#include "core/bench.h"
#include "entity/unit.h"

Bench::Bench()
    : m_slots(SLOTS, nullptr)
{}

int Bench::firstEmptySlot() const
{
    for (int i = 0; i < SLOTS; ++i) {
        if (!m_slots[i]) {
            return i;
        }
    }
    return -1;
}

int Bench::count() const
{
    int n = 0;
    for (Unit* u : m_slots) {
        if (u) {
            ++n;
        }
    }
    return n;
}

bool Bench::addUnit(Unit* unit)
{
    const int slot = firstEmptySlot();
    if (slot < 0) {
        return false;
    }
    return placeAt(slot, unit);
}

bool Bench::placeAt(int slot, Unit* unit)
{
    if (!unit || !isValidSlot(slot) || m_slots[slot]) {
        return false;
    }
    removeUnit(unit); // 若已在其它槽，先清除
    m_slots[slot] = unit;
    unit->setPosition(QPoint(-1, -1));
    return true;
}

void Bench::removeAt(int slot)
{
    if (isValidSlot(slot)) {
        m_slots[slot] = nullptr;
    }
}

void Bench::removeUnit(Unit* unit)
{
    if (!unit) {
        return;
    }
    for (int i = 0; i < SLOTS; ++i) {
        if (m_slots[i] == unit) {
            m_slots[i] = nullptr;
        }
    }
}

Unit* Bench::unitAt(int slot) const
{
    return isValidSlot(slot) ? m_slots[slot] : nullptr;
}

int Bench::slotOf(const Unit* unit) const
{
    if (!unit) return -1;
    for (int i = 0; i < SLOTS; ++i) {
        if (m_slots[i] == unit) {
            return i;
        }
    }
    return -1;
}

QVector<Unit*> Bench::allUnits() const
{
    QVector<Unit*> result;
    for (Unit* u : m_slots) {
        if (u) {
            result.append(u);
        }
    }
    return result;
}

void Bench::clear()
{
    for (int i = 0; i < SLOTS; ++i) {
        m_slots[i] = nullptr;
    }
}
