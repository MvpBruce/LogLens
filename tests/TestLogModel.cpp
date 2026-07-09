#include "../src/LogModel.h"

#include <QTemporaryFile>
#include <QtTest/QtTest>

class TestLogModel : public QObject {
    Q_OBJECT

private slots:
    void detectLevel_data();
    void detectLevel();
    void loadFileReadsUtf8AndPreservesBlankLines();
};

void TestLogModel::detectLevel_data() {
    QTest::addColumn<QString>("line");
    QTest::addColumn<int>("expected");

    QTest::newRow("error") << QStringLiteral("2026-07-09 ERROR failed")
                           << static_cast<int>(LogModel::Level::Error);
    QTest::newRow("err") << QStringLiteral("[ERR] failed")
                         << static_cast<int>(LogModel::Level::Error);
    QTest::newRow("fatal") << QStringLiteral("FATAL crash")
                           << static_cast<int>(LogModel::Level::Error);
    QTest::newRow("warn") << QStringLiteral("WARN slow request")
                          << static_cast<int>(LogModel::Level::Warn);
    QTest::newRow("warning") << QStringLiteral("WARNING disk almost full")
                             << static_cast<int>(LogModel::Level::Warn);
    QTest::newRow("info") << QStringLiteral("INFO ready")
                          << static_cast<int>(LogModel::Level::Info);
    QTest::newRow("debug") << QStringLiteral("DEBUG cache miss")
                           << static_cast<int>(LogModel::Level::Debug);
    QTest::newRow("trace") << QStringLiteral("TRACE enter function")
                           << static_cast<int>(LogModel::Level::Trace);
    QTest::newRow("unknown") << QStringLiteral("INFORMATION should not match")
                             << static_cast<int>(LogModel::Level::Unknown);
}

void TestLogModel::detectLevel() {
    QFETCH(QString, line);
    QFETCH(int, expected);

    QCOMPARE(static_cast<int>(LogModel::detectLevel(line)), expected);
}

void TestLogModel::loadFileReadsUtf8AndPreservesBlankLines() {
    QTemporaryFile file;
    QVERIFY(file.open());
    QVERIFY(file.write("INFO startup\n") > 0);
    QVERIFY(file.write("WARN caf\xc3\xa9\n") > 0);
    QVERIFY(file.write("\n") > 0);
    QVERIFY(file.write("FATAL boom\n") > 0);
    file.close();

    LogModel model;
    QString error;
    QVERIFY2(model.loadFile(file.fileName(), &error),
             qPrintable(QStringLiteral("load failed: %1").arg(error)));

    QCOMPARE(model.rowCount(), 4);
    QCOMPARE(model.index(0, LogModel::Col_Line).data().toInt(), 1);
    QCOMPARE(model.index(1, LogModel::Col_Message).data().toString(),
             QString::fromUtf8("WARN caf\xc3\xa9"));
    QCOMPARE(model.index(2, LogModel::Col_Message).data().toString(),
             QString());
    QCOMPARE(model.index(3, LogModel::Col_Level)
                 .data(LogModel::LevelRole)
                 .toInt(),
             static_cast<int>(LogModel::Level::Error));
}

QTEST_MAIN(TestLogModel)
#include "TestLogModel.moc"
