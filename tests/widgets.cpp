#include <QtTest>
#include <QGraphicsScene>
#include <QImage>
#include <QPainter>
#include "core/gamecontroller.h"
#include "entity/herodata.h"
#include "entity/unit.h"
#include "gui/boardview.h"
#include "gui/unititem.h"

class WidgetChecks : public QObject
{
    Q_OBJECT
private:
    static void mouse(BoardView& view, QEvent::Type type, QPoint pos, Qt::MouseButton button, Qt::MouseButtons buttons)
    {
        QMouseEvent event(type, QPointF(pos), QPointF(view.viewport()->mapToGlobal(pos)), button, buttons, Qt::NoModifier);
        QApplication::sendEvent(view.viewport(), &event);
    }
    static void drag(BoardView& view, QPointF from, QPointF to)
    {
        const QPoint start = view.mapFromScene(from), end = view.mapFromScene(to);
        mouse(view, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
        mouse(view, QEvent::MouseMove, end, Qt::NoButton, Qt::LeftButton);
        mouse(view, QEvent::MouseButtonRelease, end, Qt::LeftButton, Qt::NoButton);
    }
    static int tokenCount(const BoardView& view)
    {
        int count = 0;
        for (auto* item : view.scene()->items()) if (dynamic_cast<UnitItem*>(item)) ++count;
        return count;
    }
private slots:
    void snapshotsOutliveModel()
    {
        for (const auto& hero : HeroData::catalog()) {
            for (Owner owner : {Owner::Player, Owner::Enemy}) {
                auto unit = HeroData::create(hero.type, owner);
                UnitItem item(*unit);
                unit.reset();
                QImage image(80, 80, QImage::Format_ARGB32_Premultiplied);
                image.fill(Qt::transparent);
                QPainter painter(&image);
                painter.translate(40, 40);
                item.paint(&painter, nullptr, nullptr);
                painter.end();
                QVERIFY(qAlpha(image.pixel(40, 40)) > 0);
            }
        }
    }
    void selectionAndDestroyedController()
    {
        BoardView view;
        auto controller = std::make_unique<GameController>();
        controller->newGame();
        auto* unit = controller->state().addUnit(HeroData::create(HeroType::Mage, Owner::Player));
        controller->state().board().placeUnit(unit, {2, 6});
        view.setController(controller.get());
        view.resize(720, 840); view.show(); QApplication::processEvents();
        const QPoint pos = view.mapFromScene({165, 429});
        QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, pos);
        QCOMPARE(view.selectedUnitId(), unit->id());
        controller.reset();
        QApplication::processEvents();
        QCOMPARE(tokenCount(view), 0);
        QCOMPARE(view.selectedUnitId(), -1);
    }
    void rebindAndDeletion()
    {
        GameController first, second;
        first.newGame(); second.newGame();
        auto* unit = first.state().addUnit(HeroData::create(HeroType::Mage, Owner::Player));
        first.state().bench().placeAt(0, unit);
        BoardView view;
        view.setController(&first);
        QCOMPARE(tokenCount(view), 1);
        view.setController(&second);
        QCOMPARE(tokenCount(view), 0);
        first.buyUnit(0); // Old controller must no longer refresh this view.
        QCOMPARE(tokenCount(view), 0);
        second.buyUnit(0);
        QCOMPARE(tokenCount(view), 1);
        second.sellUnit(second.state().playerRoster().first()->id());
        QCOMPARE(tokenCount(view), 0);
    }
    void dragFormationAndSnapback()
    {
        GameController controller;
        controller.newGame();
        auto* unit = controller.state().addUnit(HeroData::create(HeroType::Warrior, Owner::Player));
        controller.state().bench().placeAt(0, unit);
        BoardView view;
        view.setController(&controller);
        view.resize(720, 840); view.show(); QApplication::processEvents();
        drag(view, {33, 599}, {231, 429});
        QCOMPARE(controller.state().board().unitAt({3, 6}), unit);
        QCOMPARE(controller.state().populationUsed(), 1);
        drag(view, {231, 429}, {-5, 429});
        QCOMPARE(unit->position(), QPoint(3, 6));
        drag(view, {231, 429}, {231, 99});
        QCOMPARE(unit->position(), QPoint(3, 6));
        drag(view, {231, 429}, {99, 599});
        QCOMPARE(controller.state().bench().unitAt(1), unit);
        QCOMPARE(controller.state().populationUsed(), 0);
    }
    void combatLocksDragging()
    {
        GameController controller;
        controller.newGame();
        auto* unit = controller.state().addUnit(HeroData::create(HeroType::Warrior, Owner::Player));
        controller.state().board().placeUnit(unit, {3, 6});
        BoardView view;
        view.setController(&controller);
        view.resize(720, 840); view.show(); QApplication::processEvents();
        controller.startCombat();
        drag(view, {231, 429}, {99, 599});
        QCOMPARE(unit->position(), QPoint(3, 6));
        QCOMPARE(controller.state().bench().count(), 0);
    }
};
QTEST_MAIN(WidgetChecks)
#include "widgets.moc"
