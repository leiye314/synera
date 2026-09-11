#ifndef ENTITY_HERODATA_H
#define ENTITY_HERODATA_H

#include <memory>
#include <QString>
#include <QVector>
#include "entity/types.h"

class Unit;

// 英雄目录：所有英雄的“静态数据”单一来源（名称、价格、基础属性、羁绊）。
// 单位的运行时状态不在这里——这里只描述模板。
namespace HeroData {

struct HeroInfo {
    HeroType type;
    QString name;
    int cost;
    Stats base;                 // 一星基础属性
    QVector<Trait> traits;
};

const QVector<HeroInfo>& catalog();
const HeroInfo& info(HeroType type);

// 工厂：按类型与归属创建具体英雄子类，返回集中所有权的 unique_ptr。
std::unique_ptr<Unit> create(HeroType type, Owner owner);

} // namespace HeroData

#endif // ENTITY_HERODATA_H
