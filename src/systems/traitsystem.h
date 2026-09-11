#ifndef SYSTEMS_TRAITSYSTEM_H
#define SYSTEMS_TRAITSYSTEM_H

#include <QString>
#include <QVector>
#include "entity/types.h"

class Unit;

// 羁绊系统：统计标签、决定生效档位、并重算每个单位的属性修正。
//
// 关键设计（避免重复累计）：
//  - 羁绊数量按“不同英雄类型”计数；同名重复或升星不会叠加羁绊。
//  - applyModifiers 每次都把单位的 StatModifiers 从零重建（装备 + 当前生效羁绊），
//    因此无论上阵/下阵多少次，都不会出现叠加残留。
namespace TraitSystem {

struct TraitStatus {
    Trait trait;
    int count;             // 不同英雄类型计数
    int activeThreshold;   // 当前生效档位（0 表示未激活）
};

QVector<int> thresholds(Trait trait);     // 该羁绊的档位，如 {2,3}
QString effectText(Trait trait);          // 人类可读的效果说明（全档位，用于 GUI 概览）
QString effectTextForTier(Trait trait, int tier); // 指定生效档位下的“具体效果”说明

// 统计一组单位（通常是某一方的上阵单位）的羁绊状态。
QVector<TraitStatus> status(const QVector<Unit*>& units);

// 依据“装备 + 当前生效羁绊”重算这组单位的 StatModifiers。
void applyModifiers(const QVector<Unit*>& units);

} // namespace TraitSystem

#endif // SYSTEMS_TRAITSYSTEM_H
