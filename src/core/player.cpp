#include "core/player.h"

#include <algorithm>
#include <limits>

Player::Player()
    : m_hp(100)
    , m_gold(10)
    , m_level(3)          // 初始人口上限 3
    , m_winStreak(0)
    , m_lossStreak(0)
{}

void Player::addGold(int amount)
{
    m_gold = static_cast<int>(std::clamp(static_cast<long long>(m_gold) + amount,
        0LL, static_cast<long long>(std::numeric_limits<int>::max())));
}

bool Player::spendGold(int amount)
{
    if (amount < 0 || m_gold < amount) {
        return false;
    }
    m_gold -= amount;
    return true;
}

void Player::setLevel(int level)
{
    m_level = std::clamp(level, 1, s_maxLevel);
}

int Player::upgradeCost() const
{
    if (m_level >= s_maxLevel) {
        return -1;
    }
    // 递增费用：升到下一级所需金币随等级增加。
    return 4 + (m_level - 1) * 2;   // 3->4:8, 4->5:10, 5->6:12 ...
}

void Player::recordWin()
{
    if (m_winStreak < std::numeric_limits<int>::max()) ++m_winStreak;
    m_lossStreak = 0;
}

void Player::recordLoss()
{
    if (m_lossStreak < std::numeric_limits<int>::max()) ++m_lossStreak;
    m_winStreak = 0;
}
