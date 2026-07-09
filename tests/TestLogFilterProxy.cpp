#include "../src/LogFilterProxy.h"
#include "../src/LogModel.h"

#include <QTemporaryFile>
#include <QtTest/QtTest>

class TestLogFilterProxy : public QObject {
    Q_OBJECT

private slots:
    void filtersBySubstringAndLevel();
    void filtersByRegexAndReportsErrors();
};

namespace {
void loadSample(LogModel* model) {
    QTemporaryFile file;
    QVERIFY(file.open());
    QVERIFY(file.write("INFO startup complete\n") > 0);
    QVERIFY(file.write("ERROR request failed\n") > 0);
    QVERIFY(file.write("DEBUG request details\n") > 0);
    QVERIFY(file.write("WARN request warning\n") > 0);
    file.close();

    QString error;
    QVERIFY2(model->loadFile(file.fileName(), &error),
             qPrintable(QStringLiteral("load failed: %1").arg(error)));
}
} // namespace

void TestLogFilterProxy::filtersBySubstringAndLevel() {
    LogModel model;
    loadSample(&model);

    LogFilterProxy proxy;
    proxy.setSourceModel(&model);

    QCOMPARE(proxy.rowCount(), 4);

    proxy.setQuery(QStringLiteral("request"));
    QCOMPARE(proxy.rowCount(), 3);
    QCOMPARE(proxy.index(0, LogModel::Col_Line).data().toInt(), 2);
    QCOMPARE(proxy.index(1, LogModel::Col_Line).data().toInt(), 3);
    QCOMPARE(proxy.index(2, LogModel::Col_Line).data().toInt(), 4);

    proxy.setLevelEnabled(LogModel::Level::Debug, false);
    QCOMPARE(proxy.rowCount(), 2);
    QCOMPARE(proxy.index(0, LogModel::Col_Level).data().toString(),
             QStringLiteral("ERROR"));
    QCOMPARE(proxy.index(1, LogModel::Col_Level).data().toString(),
             QStringLiteral("WARN"));

    proxy.setLevelEnabled(LogModel::Level::Error, false);
    QCOMPARE(proxy.rowCount(), 1);
    QCOMPARE(proxy.index(0, LogModel::Col_Level).data().toString(),
             QStringLiteral("WARN"));
}

void TestLogFilterProxy::filtersByRegexAndReportsErrors() {
    LogModel model;
    loadSample(&model);

    LogFilterProxy proxy;
    proxy.setSourceModel(&model);
    proxy.setUseRegex(true);

    proxy.setQuery(QStringLiteral("request (failed|warning)"));
    QVERIFY(!proxy.hasRegexError());
    QCOMPARE(proxy.rowCount(), 2);
    QCOMPARE(proxy.index(0, LogModel::Col_Line).data().toInt(), 2);
    QCOMPARE(proxy.index(1, LogModel::Col_Line).data().toInt(), 4);

    proxy.setQuery(QStringLiteral("("));
    QVERIFY(proxy.hasRegexError());
    QVERIFY(!proxy.regexErrorString().isEmpty());
    QCOMPARE(proxy.rowCount(), 0);
}

QTEST_MAIN(TestLogFilterProxy)
#include "TestLogFilterProxy.moc"
