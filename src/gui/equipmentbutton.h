#ifndef GUI_EQUIPMENTBUTTON_H
#define GUI_EQUIPMENTBUTTON_H

#include <QPoint>
#include <QPushButton>
#include <QString>
#include "entity/types.h"

// 装备栏里的一种可用装备按钮。
//
// 它同时支持两种穿戴入口，且共用同一套装备业务逻辑（GameController::equipUnit）：
//  1. 点击：为当前选中单位穿戴（保留原有交互，由外部连接 clicked 信号处理）。
//  2. 拖拽：用 Qt 原生 QDrag + MIME 把“装备池下标”拖到棋盘，BoardView 接收落点后
//     解析出单位并调用同一个 equipUnit，因此不复制任何装备规则。
//
// 不持有规则：按钮只携带装备池下标与类型，规则校验仍在 EquipmentSystem/GameController。
class EquipmentButton : public QPushButton
{
    Q_OBJECT

public:
    // 跨控件拖放使用的 MIME 类型；BoardView 用同一字符串识别装备拖放。
    static const QString& mimeType();

    EquipmentButton(int poolIndex, EquipmentType type, bool draggable,
                    QWidget* parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    int m_poolIndex;
    bool m_draggable;
    QPoint m_pressPos;
};

#endif // GUI_EQUIPMENTBUTTON_H
