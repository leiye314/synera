#ifndef CORE_LEVELCONFIG_H
#define CORE_LEVELCONFIG_H

#include <QVector>
#include "entity/types.h"

// 十关 PvE 敌人生成配置：敌人数、属性倍率、星级分布与可选英雄池。
// 强度指标 estimatedStrength() 供确定性自测验证单调增长。
namespace LevelConfig {

struct StageConfig {
    int enemyCount = 2;
    double hpMul = 1.0;
    double adMul = 1.0;
    double skillMul = 1.0;
    int threeStarCount = 0;   // 按放置顺序前 N 个为三星
    int twoStarCount = 0;     // 接着 M 个为二星，其余一星
    QVector<HeroType> heroPool; // 空 = 全英雄目录
};

int stageCount();
StageConfig configForRound(int round); // round 为 1..stageCount()

// 第 slotIndex 个敌人（0 起）的星级。
int starForSlot(const StageConfig& cfg, int slotIndex);

// 综合强度估计（敌人数 × 属性倍率 × 平均星级倍率），用于自测断言。
double estimatedStrength(const StageConfig& cfg);

// 配置合法性：敌人数、倍率为正且不超过敌方半场容量。
bool isValid(const StageConfig& cfg, int maxEnemySlots);

} // namespace LevelConfig

#endif // CORE_LEVELCONFIG_H
