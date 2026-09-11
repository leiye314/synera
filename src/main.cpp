#include <QApplication>
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QRandomGenerator>
#include <QTextStream>
#include <limits>
#include <memory>
#include "gui/gamewindow.h"
#include "core/selftest.h"

int main(int argc, char** argv)
{
    bool selftest = false;
    for (int i = 1; i < argc; ++i)
        if (QString::fromLocal8Bit(argv[i]) == "--selftest") selftest = true;
    std::unique_ptr<QCoreApplication> app;
    if (selftest) app = std::make_unique<QCoreApplication>(argc, argv);
    else app = std::make_unique<QApplication>(argc, argv);
    QCoreApplication::setApplicationName("Synera");
    QCoreApplication::setApplicationVersion("1.1.0");
    QCommandLineParser parser;
    parser.setApplicationDescription("Synera — a C++/Qt auto-battler");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({"seed", "Reproducible new-game seed (0..4294967295)", "integer"});
    parser.addOption({"selftest", "Run core regression checks without a window"});
    parser.addOption({"report", "Self-test report path", "path", "selftest_report.txt"});
    parser.process(*app);
    quint32 seed = QRandomGenerator::system()->generate();
    if (parser.isSet("seed")) {
        const QString value = parser.value("seed");
        bool ok = false;
        const qulonglong parsed = value.toULongLong(&ok);
        for (QChar ch : value) ok = ok && ch >= '0' && ch <= '9';
        if (!ok || parsed > std::numeric_limits<quint32>::max()) {
            QTextStream(stderr) << "Invalid --seed: expected an unsigned 32-bit decimal integer.\n";
            return 2;
        }
        seed = static_cast<quint32>(parsed);
    }
    if (selftest) return runSelfTest(parser.value("report"));
    GameWindow window(nullptr, seed);
    window.setWindowTitle(QStringLiteral("Synera · 自动战棋"));
    window.resize(1180, 860);
    window.show();
    return app->exec();
}
