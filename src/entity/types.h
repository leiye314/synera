#ifndef ENTITY_TYPES_H
#define ENTITY_TYPES_H

#include <QString>

// 全局共享的枚举与基础数据结构。
// 单独放在一个头文件里，避免 Unit / 系统 / GUI 之间的循环包含。

// 单位归属：我方与敌方共用同一套 Unit 体系，仅以 Owner 区分。
enum class Owner {
    Player,
    Enemy
};

// 具体英雄类型。每个类型对应一个重写了技能的 Unit 子类。
enum class HeroType {
    Warrior,   // 剑士：近战重击
    Knight,    // 骑士：近战护盾坦克
    Mage,      // 法师：远程 AOE
    Ranger,    // 游侠：远程高攻速连射
    Priest,    // 祭司：远程治疗
    Assassin   // 影刺：近战爆发
};

// 羁绊标签。每个英雄携带 1~2 个标签，由 TraitSystem 统计后给出加成。
enum class Trait {
    Warrior,   // 战士：自身阵营内 +最大生命
    Guardian,  // 守卫：自身阵营内 减伤
    Mage,      // 法师：全队 +技能伤害
    Ranger,    // 游侠：自身阵营内 +攻速
    Spirit     // 灵族：全队 +初始法力
};

// 单位战斗状态机。
enum class CombatState {
    Idle,
    Moving,
    Attacking,
    Casting,
    Dead
};

// 装备类型。
enum class EquipmentType {
    Sword,    // 铁剑：ATK +15
    Armor,    // 锁子甲：最大 HP +150（并同步当前 HP）
    Gloves,   // 急速手套：攻击间隔 -20%
    Crystal   // 蓝水晶：最大法力 -30（有下限）
};

// 游戏阶段。
enum class Phase {
    Prep,     // 准备：可拖拽、买卖、升级
    Combat,   // 战斗：QTimer 驱动，禁止手动拖拽
    Resolve   // 结算：计算金币/扣血/掉装备
};

// 单位的基础属性（来自英雄目录 × 星级倍率）。
// 不含羁绊/装备加成，这些通过 StatModifiers 叠加后由 Unit 计算出有效属性。
struct Stats {
    double maxHp = 100.0;
    double attackDamage = 10.0;
    double attackRange = 1.0;        // 以格为单位（欧氏），1=近战相邻
    double attackIntervalSec = 1.0;  // 两次普攻的基础间隔
    double spellPower = 1.0;         // 技能伤害基础系数
    double armor = 0.0;              // 平坦减伤（先于百分比减伤）
    double damageReductionPct = 0.0; // 百分比减伤 0~1
    int    maxMana = 100;
    int    startMana = 0;
};

// 一次伤害结算的结果。普通攻击与技能共用同一结构，避免各英雄/各处重复计算，
// 并让日志能清晰区分“结算伤害 / 护盾吸收 / 实际生命损失”。
//  - raw：进入结算前的原始伤害（技能已含加成）。
//  - resolved：经护甲与百分比减伤后的结算伤害（进入护盾/生命之前）。
//  - shieldAbsorbed：被护盾吸收的部分。
//  - hpLoss：实际损失的生命（即旧返回值，死亡判定据此不变）。
//  - lethal：本次伤害是否致死。
struct DamageResult {
    double raw = 0.0;
    double resolved = 0.0;
    double shieldAbsorbed = 0.0;
    double hpLoss = 0.0;
    bool   lethal = false;

    // 多段/多目标技能按段累加，得到一次施法的合计结果。
    DamageResult& operator+=(const DamageResult& other) {
        raw += other.raw;
        resolved += other.resolved;
        shieldAbsorbed += other.shieldAbsorbed;
        hpLoss += other.hpLoss;
        lethal = lethal || other.lethal;
        return *this;
    }
};

// 羁绊/装备产生的可叠加修正。
// 关键不变量：每次阵容/装备改变，TraitSystem 与 EquipmentSystem 会“从零重建”整份修正，
// 因此绝不会出现重复累计。
struct StatModifiers {
    double flatMaxHp = 0.0;
    double pctMaxHp = 0.0;
    double flatAttackDamage = 0.0;
    double pctAttackSpeed = 0.0;       // 提高攻速 => 缩短攻击间隔
    double flatArmor = 0.0;
    double damageReductionPct = 0.0;
    double skillDamagePct = 0.0;       // 技能伤害加成
    int    flatStartMana = 0;
    int    flatMaxManaDelta = 0;       // 蓝水晶为负
    double doubleAttackChance = 0.0;   // 机制类（暂作扩展位）
};

inline QString heroTypeKey(HeroType type)
{
    switch (type) {
    case HeroType::Warrior:  return QStringLiteral("Warrior");
    case HeroType::Knight:   return QStringLiteral("Knight");
    case HeroType::Mage:     return QStringLiteral("Mage");
    case HeroType::Ranger:   return QStringLiteral("Ranger");
    case HeroType::Priest:   return QStringLiteral("Priest");
    case HeroType::Assassin: return QStringLiteral("Assassin");
    }
    return QStringLiteral("Warrior");
}

inline bool heroTypeFromKey(const QString& key, HeroType& out)
{
    if (key == QLatin1String("Warrior"))  { out = HeroType::Warrior;  return true; }
    if (key == QLatin1String("Knight"))   { out = HeroType::Knight;   return true; }
    if (key == QLatin1String("Mage"))     { out = HeroType::Mage;     return true; }
    if (key == QLatin1String("Ranger"))   { out = HeroType::Ranger;   return true; }
    if (key == QLatin1String("Priest"))   { out = HeroType::Priest;   return true; }
    if (key == QLatin1String("Assassin")) { out = HeroType::Assassin; return true; }
    return false;
}

inline QString traitName(Trait trait)
{
    switch (trait) {
    case Trait::Warrior:  return QStringLiteral("战士");
    case Trait::Guardian: return QStringLiteral("守卫");
    case Trait::Mage:     return QStringLiteral("法师");
    case Trait::Ranger:   return QStringLiteral("游侠");
    case Trait::Spirit:   return QStringLiteral("灵族");
    }
    return QString();
}

inline QString equipmentTypeKey(EquipmentType type)
{
    switch (type) {
    case EquipmentType::Sword:   return QStringLiteral("Sword");
    case EquipmentType::Armor:   return QStringLiteral("Armor");
    case EquipmentType::Gloves:  return QStringLiteral("Gloves");
    case EquipmentType::Crystal: return QStringLiteral("Crystal");
    }
    return QStringLiteral("Sword");
}

inline bool equipmentTypeFromKey(const QString& key, EquipmentType& out)
{
    if (key == QLatin1String("Sword"))   { out = EquipmentType::Sword;   return true; }
    if (key == QLatin1String("Armor"))   { out = EquipmentType::Armor;   return true; }
    if (key == QLatin1String("Gloves"))  { out = EquipmentType::Gloves;  return true; }
    if (key == QLatin1String("Crystal")) { out = EquipmentType::Crystal; return true; }
    return false;
}

#endif // ENTITY_TYPES_H
