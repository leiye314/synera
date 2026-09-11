#ifndef CORE_PLAYER_H
#define CORE_PLAYER_H

// 玩家状态：生命、金币、人口上限（等级）、连胜/连败。
// 纯数据 + 少量规则（升级费用、连胜计数），不依赖 Qt 之外的东西。
class Player
{
public:
    Player();

    int hp() const { return m_hp; }
    void setHp(int hp) { m_hp = hp < 0 ? 0 : hp; }
    bool isDefeated() const { return m_hp <= 0; }

    int gold() const { return m_gold; }
    void setGold(int gold) { m_gold = gold < 0 ? 0 : gold; }
    void addGold(int amount);
    bool spendGold(int amount);              // 金币不足返回 false

    int level() const { return m_level; }    // 人口上限
    void setLevel(int level);
    int maxLevel() const { return s_maxLevel; }
    int upgradeCost() const;                 // 当前升级所需金币；满级返回 -1
    bool canUpgrade() const { return m_level < s_maxLevel; }

    int winStreak() const { return m_winStreak; }
    int lossStreak() const { return m_lossStreak; }
    void recordWin();
    void recordLoss();
    void resetStreaks() { m_winStreak = 0; m_lossStreak = 0; }
    void setWinStreak(int v) { m_winStreak = v < 0 ? 0 : v; }
    void setLossStreak(int v) { m_lossStreak = v < 0 ? 0 : v; }

private:
    static constexpr int s_maxLevel = 9;

    int m_hp;
    int m_gold;
    int m_level;
    int m_winStreak;
    int m_lossStreak;
};

#endif // CORE_PLAYER_H
