#include "LogTailer.h"

#include "LogModel.h"

#include <QFile>
#include <QFileSystemWatcher>

LogTailer::LogTailer(LogModel* model, QObject* parent)
    : QObject(parent), m_model(model),
      m_watcher(new QFileSystemWatcher(this)) {
    connect(m_watcher, &QFileSystemWatcher::fileChanged, this,
            &LogTailer::onFileChanged);
}

void LogTailer::start(const QString& path, qint64 offset) {
    stop();
    m_path = path;
    m_offset = offset;
    m_active = true;
    m_watcher->addPath(m_path);
    readNew();
}

void LogTailer::stop() {
    if (!m_path.isEmpty())
        m_watcher->removePath(m_path);
    m_path.clear();
    m_active = false;
}

void LogTailer::onFileChanged(const QString& /*path*/) {
    readNew();
    // Some platforms drop the watch after the file changes; re-arm it so we
    // keep receiving notifications.
    if (m_active && !m_watcher->files().contains(m_path))
        m_watcher->addPath(m_path);
}

void LogTailer::readNew() {
    // Binary mode: byte offsets stay exact (no \r\n translation).
    QFile f(m_path);
    if (!f.open(QIODevice::ReadOnly))
        return;

    const qint64 size = f.size();
    if (size < m_offset) {
        // Truncated/rotated: resync to the new end to avoid duplicate lines.
        m_offset = size;
        return;
    }
    if (size == m_offset)
        return; // metadata-only change, nothing new

    if (!f.seek(m_offset))
        return;
    const QByteArray chunk = f.readAll();

    // Only consume up to the last complete line; a half-written final line is
    // left for the next notification.
    const int lastNl = chunk.lastIndexOf('\n');
    if (lastNl < 0)
        return;
    const QByteArray complete = chunk.left(lastNl + 1);
    m_offset += complete.size();

    QVector<LogModel::Entry> batch;
    int lineNo = m_model->rowCount(); // continue file line numbering
    const QList<QByteArray> lines = complete.split('\n');
    for (int i = 0; i < lines.size(); ++i) {
        QByteArray raw = lines.at(i);
        if (i == lines.size() - 1 && raw.isEmpty())
            continue; // trailing element after the final newline
        if (raw.endsWith('\r'))
            raw.chop(1); // tolerate CRLF logs
        const QString line = QString::fromUtf8(raw);
        batch.push_back({++lineNo, LogModel::detectLevel(line), line});
    }

    if (!batch.isEmpty()) {
        m_model->appendBatch(batch);
        emit appended();
    }
}
