#include <QCoreApplication>
#include <QCommandLineParser>
#include "core/selftest.h"

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({"report", "Report file", "path", "selftest_report.txt"});
    parser.process(app);
    return runSelfTest(parser.value("report"));
}
