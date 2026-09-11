#ifndef CORE_SELFTEST_H
#define CORE_SELFTEST_H

#include <QString>

// 核心逻辑无界面自检：在没有 GUI 点击的环境下验证棋盘/寻路/羁绊/战斗/存档等关键算法。
// 通过命令行 `Synera_Starter.exe --selftest` 触发。
// 返回 0 表示全部通过；非 0 表示失败的用例数。结果同时写入 reportPath。
int runSelfTest(const QString& reportPath);

#endif // CORE_SELFTEST_H
