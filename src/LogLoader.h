#pragma once

#include <QAtomicInt>
#include <QObject>
#include <QVector>

#include "LogModel.h"

// Parses a log file off the UI thread. Lives in a worker QThread; load() reads
// and classifies lines, emitting them in batches so the model (on the UI
// thread) can insert them incrementally while the UI stays responsive.
//
// A monotonically increasing "generation" tags every signal: the UI passes the
// current generation into load() and ignores anything tagged with an older one,
// so switching files mid-load never mixes results.
class LogLoader : public QObject {
    Q_OBJECT
public:
    explicit LogLoader(QObject* parent = nullptr);

    // Thread-safe: called from the UI thread to abort an in-flight load.
    void cancel();

public slots:
    void load(const QString& path, quint64 generation);

signals:
    void started(quint64 generation);
    void batchReady(quint64 generation, QVector<LogModel::Entry> batch);
    void progress(quint64 generation, int percent);
    void finished(quint64 generation, bool ok, QString error);

private:
    QAtomicInt m_cancel{0};
};
