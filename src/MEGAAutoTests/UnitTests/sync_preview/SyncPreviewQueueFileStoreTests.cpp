#include "SyncPreviewQueueFileStore.h"

#include <QFile>
#include <QSaveFile>
#include <QTemporaryDir>

#include <catch.hpp>

using namespace SyncPreview;

namespace
{
    Queue sampleQueue()
    {
        Queue queue;

        Pair pair;
        pair.id = QStringLiteral("pair-1");
        pair.localPath = QStringLiteral("/tmp/local");
        pair.remotePath = QStringLiteral("Cloud Drive/remote");
        pair.remoteHandle = QStringLiteral("h");

        RowDecision decision;
        decision.relativePath = QStringLiteral("docs/a.txt");
        decision.action = Action::LocalToRemote;
        decision.approved = false;
        decision.kind = static_cast<int>(RowKind::LocalOnly);
        pair.decisions.append(decision);

        queue.pairs.append(pair);
        return queue;
    }
}

TEST_CASE("QueueFileStore round-trips a queue through a file")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString filePath = dir.filePath(QStringLiteral("sync-preview-queue.json"));

    const QueueFileStore store(filePath);
    REQUIRE(store.save(sampleQueue()));
    REQUIRE(QFile::exists(filePath));

    const Queue restored = store.load();
    REQUIRE(restored.pairs.size() == 1);
    CHECK(restored.pairs.first().id == QStringLiteral("pair-1"));
    REQUIRE(restored.pairs.first().decisions.size() == 1);
    CHECK(restored.pairs.first().decisions.first().relativePath == QStringLiteral("docs/a.txt"));
    CHECK(restored.pairs.first().decisions.first().action == Action::LocalToRemote);
    CHECK(restored.pairs.first().decisions.first().kind == static_cast<int>(RowKind::LocalOnly));
}

TEST_CASE("A missing file restores an empty queue")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    const QueueFileStore store(dir.filePath(QStringLiteral("never-written.json")));

    CHECK(store.load().pairs.isEmpty());
}

TEST_CASE("A corrupted file restores an empty queue")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString filePath = dir.filePath(QStringLiteral("sync-preview-queue.json"));

    const QueueFileStore store(filePath);

    // Save a valid queue, then corrupt the file on disk.
    REQUIRE(store.save(sampleQueue()));
    {
        QFile file(filePath);
        REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(QByteArray("{{{ not a queue"));
    }

    CHECK(store.load().pairs.isEmpty());

    // Saving over the corrupted file repairs it.
    REQUIRE(store.save(sampleQueue()));
    CHECK(store.load().pairs.size() == 1);
}

TEST_CASE("Saving to an unwritable path fails without creating the file")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    // The parent directory does not exist: QSaveFile cannot open.
    const QString filePath = dir.filePath(QStringLiteral("no/such/dir/queue.json"));

    const QueueFileStore store(filePath);
    CHECK_FALSE(store.save(sampleQueue()));
    CHECK_FALSE(QFile::exists(filePath));
}