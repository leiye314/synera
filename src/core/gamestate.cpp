#include "core/gamestate.h"
#include "entity/unit.h"

#include <algorithm>

namespace {
constexpr int kShopSlots = 5;
}

GameState::GameState(quint32 seed)
    : m_random(seed)
    , m_phase(Phase::Prep)
    , m_round(1)
    , m_maxRounds(10)
    , m_shop(kShopSlots)
{}

GameState::~GameState() = default;
GameState::GameState(GameState&&) noexcept = default;
GameState& GameState::operator=(GameState&&) noexcept = default;

Unit* GameState::addUnit(std::unique_ptr<Unit> unit)
{
    if (!unit) {
        return nullptr;
    }
    Unit* raw = unit.get();
    m_units.push_back(std::move(unit));
    return raw;
}

void GameState::removeUnit(Unit* unit)
{
    if (!unit) {
        return;
    }
    m_board.removeUnit(unit);
    m_bench.removeUnit(unit);
    for (auto it = m_units.begin(); it != m_units.end(); ++it) {
        if (it->get() == unit) {
            m_units.erase(it);
            return;
        }
    }
}

void GameState::clearUnits()
{
    m_board.clear();
    m_bench.clear();
    m_units.clear();
}

void GameState::removeEnemyUnits()
{
    // 先从棋盘移除占位，再销毁对象，避免悬挂指针。
    for (auto& up : m_units) {
        if (up && up->owner() == Owner::Enemy) {
            m_board.removeUnit(up.get());
            m_bench.removeUnit(up.get());
        }
    }
    m_units.erase(
        std::remove_if(m_units.begin(), m_units.end(),
                       [](const std::unique_ptr<Unit>& up) {
                           return up && up->owner() == Owner::Enemy;
                       }),
        m_units.end());
}

Unit* GameState::findUnit(int id) const
{
    for (const auto& up : m_units) {
        if (up && up->id() == id) {
            return up.get();
        }
    }
    return nullptr;
}

QVector<Unit*> GameState::playerRoster() const
{
    QVector<Unit*> result;
    for (const auto& up : m_units) {
        if (up && up->owner() == Owner::Player) {
            result.append(up.get());
        }
    }
    return result;
}

QVector<Unit*> GameState::playerBoardUnits() const
{
    QVector<Unit*> result;
    for (Unit* u : m_board.allUnits()) {
        if (u && u->owner() == Owner::Player) {
            result.append(u);
        }
    }
    return result;
}

QVector<Unit*> GameState::enemyBoardUnits() const
{
    QVector<Unit*> result;
    for (Unit* u : m_board.allUnits()) {
        if (u && u->owner() == Owner::Enemy) {
            result.append(u);
        }
    }
    return result;
}

void GameState::resetToNewGame()
{
    clearUnits();
    m_random.reset(m_random.seed());
    m_player = Player();
    m_phase = Phase::Prep;
    m_round = 1;
    m_shop = QVector<ShopSlot>(kShopSlots);
    m_equipmentPool.clear();
}
