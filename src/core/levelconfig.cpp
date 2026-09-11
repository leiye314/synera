#include "core/levelconfig.h"

#include <algorithm>
#include <cmath>

namespace LevelConfig {

namespace {

constexpr int kStageCount = 10;

const QVector<StageConfig>& allStages()
{
    static const QVector<StageConfig> kStages = {
        // 1–3：逐步增加数量，属性微增
        { 2, 1.00, 1.00, 1.00, 0, 0, {} },
        { 3, 1.00, 1.02, 1.00, 0, 0, {} },
        { 4, 1.05, 1.05, 1.02, 0, 0, {} },
        // 4–6：引入二星，6→7 平滑过渡（7 敌 → 8 敌 + 适度倍率）
        { 5, 1.10, 1.08, 1.05, 0, 1, {} },
        { 6, 1.15, 1.10, 1.08, 0, 2, {} },
        { 7, 1.20, 1.14, 1.10, 0, 2, {} },
        // 7–10：敌人数均为 8，通过倍率、星级与英雄池递增强度
        { 8, 1.28, 1.18, 1.12, 0, 3, {} },
        { 8, 1.38, 1.25, 1.18, 1, 3,
          { HeroType::Warrior, HeroType::Knight, HeroType::Mage,
            HeroType::Ranger, HeroType::Assassin } },
        { 8, 1.48, 1.32, 1.24, 2, 3,
          { HeroType::Warrior, HeroType::Knight, HeroType::Mage,
            HeroType::Ranger, HeroType::Assassin } },
        { 8, 1.60, 1.40, 1.32, 3, 3,
          { HeroType::Warrior, HeroType::Knight, HeroType::Mage,
            HeroType::Assassin } },
    };
    return kStages;
}

} // namespace

int stageCount() { return kStageCount; }

StageConfig configForRound(int round)
{
    const QVector<StageConfig>& stages = allStages();
    const int idx = std::clamp(round, 1, kStageCount) - 1;
    return stages.at(idx);
}

int starForSlot(const StageConfig& cfg, int slotIndex)
{
    if (slotIndex < cfg.threeStarCount) {
        return 3;
    }
    if (slotIndex < cfg.threeStarCount + cfg.twoStarCount) {
        return 2;
    }
    return 1;
}

double estimatedStrength(const StageConfig& cfg)
{
    if (cfg.enemyCount <= 0) {
        return 0.0;
    }
    double starSum = 0.0;
    for (int i = 0; i < cfg.enemyCount; ++i) {
        const int star = starForSlot(cfg, i);
        starSum += std::pow(1.7, std::max(0, star - 1));
    }
    const double avgStarMul = starSum / cfg.enemyCount;
    return cfg.enemyCount * cfg.hpMul * cfg.adMul * cfg.skillMul * avgStarMul;
}

bool isValid(const StageConfig& cfg, int maxEnemySlots)
{
    if (cfg.enemyCount < 1 || cfg.enemyCount > maxEnemySlots) {
        return false;
    }
    if (cfg.hpMul <= 0.0 || cfg.adMul <= 0.0 || cfg.skillMul <= 0.0) {
        return false;
    }
    if (cfg.threeStarCount < 0 || cfg.twoStarCount < 0) {
        return false;
    }
    if (cfg.threeStarCount + cfg.twoStarCount > cfg.enemyCount) {
        return false;
    }
    return true;
}

} // namespace LevelConfig
