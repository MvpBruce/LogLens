#include "LogLoader.h"

#include <QFile>
#include <QStringConverter>
#include <QTextStream>

namespace {
constexpr int kBatchSize = 10000; // lines per emitted batch
}

LogLoader::LogLoader(QObject* parent) : QObject(parent) {}

void LogLoader::cancel() {
    m_cancel.storeRelaxed(1);
}

void LogLoader::load(const QString& path, quint64 generation) {
    m_cancel.storeRelaxed(0);
    emit started(generation);

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit finished(generation, false, file.errorString(), 0);
        return;
    }

    const qint64 total = qMax<qint64>(file.size(), 1);
    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);

    QVector<LogModel::Entry> batch;
    batch.reserve(kBatchSize);
    int lineNo = 0;
    int lastPercent = -1;

    while (!in.atEnd()) {
        if (m_cancel.loadRelaxed()) {
            emit finished(generation, false, QStringLiteral("canceled"),
                          file.pos());
            return;
        }

        const QString line = in.readLine();
        ++lineNo;
        batch.push_back(LogModel::parseLine(lineNo, line));

        if (batch.size() >= kBatchSize) {
            emit batchReady(generation, batch);
            batch.clear();
            batch.reserve(kBatchSize);

            const int percent = static_cast<int>(file.pos() * 100 / total);
            if (percent != lastPercent) {
                lastPercent = percent;
                emit progress(generation, percent);
            }
        }
    }

    if (!batch.isEmpty())
        emit batchReady(generation, batch);

    emit progress(generation, 100);
    emit finished(generation, true, QString(), file.pos());
}
