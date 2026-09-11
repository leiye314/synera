#include "gui/equipmentbutton.h"
#include "entity/equipment.h"

#include <QApplication>
#include <QByteArray>
#include <QDrag>
#include <QMimeData>
#include <QMouseEvent>
#include <QPixmap>

const QString& EquipmentButton::mimeType()
{
    static const QString kMime = QStringLiteral("application/x-synera-equipment");
    return kMime;
}

EquipmentButton::EquipmentButton(int poolIndex, EquipmentType type, bool draggable,
                                 QWidget* parent)
    : QPushButton(parent)
    , m_poolIndex(poolIndex)
    , m_draggable(draggable)
{
    setText(EquipmentData::name(type));
    setToolTip(QStringLiteral("%1\n点击为选中单位穿戴，或直接拖到单位身上穿戴")
                   .arg(EquipmentData::description(type)));
    // 仅准备阶段可用：禁用时既不能点击也不能拖拽，与原有规则一致。
    setEnabled(draggable);
    if (draggable) {
        setCursor(Qt::OpenHandCursor);
    }
}

void EquipmentButton::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_pressPos = event->pos();
    }
    QPushButton::mousePressEvent(event); // 保留正常点击行为
}

void EquipmentButton::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_draggable || !(event->buttons() & Qt::LeftButton)) {
        QPushButton::mouseMoveEvent(event);
        return;
    }
    // 位移超过系统拖拽阈值才视为拖拽，否则交回按钮（保证轻点仍是点击穿戴）。
    if ((event->pos() - m_pressPos).manhattanLength() < QApplication::startDragDistance()) {
        QPushButton::mouseMoveEvent(event);
        return;
    }

    QDrag drag(this);
    QMimeData* mime = new QMimeData;
    mime->setData(mimeType(), QByteArray::number(m_poolIndex));
    drag.setMimeData(mime);

    // 拖拽图标：用按钮自身的渲染作为跟随光标的图标，给出清晰的视觉反馈。
    const QPixmap pixmap = grab();
    drag.setPixmap(pixmap);
    drag.setHotSpot(QPoint(pixmap.width() / 2, pixmap.height() / 2));

    // 拖拽会“接管”鼠标，原本的按下不会再收到释放，需手动复位按下态。
    setDown(false);
    drag.exec(Qt::CopyAction);
    setDown(false);
}
