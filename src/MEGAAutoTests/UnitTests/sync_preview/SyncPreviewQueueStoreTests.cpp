#include "SyncPreviewQueueStore.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <catch.hpp>

using namespace SyncPreview;

namespace
{
    Queue sampleQueue()
    {
        Queue queue;

        Pair first;
        first.id = QStringLiteral("pair-1");
        first.localPath = QStringLiteral("/home/user/local-root");
        first.remotePath = QStringLiteral("Cloud Drive/remote-root");
        first.remoteHandle = QStringLiteral("abcd1234");

        RowDecision upload;
        upload.relativePath = QStringLiteral("docs/report.txt");
        upload.action = Action::LocalToRemote;
        upload.approved = true;

        RowDecision keepNone;
        keepNone.relativePath = QStringLiteral("photos/img1.png");
        keepNone.action = Action::None;
        keepNone.approved = false;

        first.decisions = {upload, keepNone};
        queue.pairs.append(first);

        Pair second;
        second.id = QStringLiteral("pair-2");
        second.localPath = QStringLiteral("/home/user/other");
        second.remotePath = QStringLiteral("Cloud Drive/other");
        second.remoteHandle = QStringLiteral("ef5678");
        queue.pairs.append(second);

        return queue;
    }
}

TEST_CASE("Queue round-trip preserves pairs and decisions")
{
    const Queue original = sampleQueue();

    const Queue restored = QueueStore::deserialize(QueueStore::serialize(original));

    REQUIRE(restored.pairs.size() == 2);

    const Pair& first = restored.pairs.at(0);
    CHECK(first.id == QStringLiteral("pair-1"));
    CHECK(first.localPath == QStringLiteral("/home/user/local-root"));
    CHECK(first.remotePath == QStringLiteral("Cloud Drive/remote-root"));
    CHECK(first.remoteHandle == QStringLiteral("abcd1234"));
    REQUIRE(first.decisions.size() == 2);
    CHECK(first.decisions.at(0).relativePath == QStringLiteral("docs/report.txt"));
    CHECK(first.decisions.at(0).action == Action::LocalToRemote);
    CHECK(first.decisions.at(0).approved);
    CHECK(first.decisions.at(1).relativePath == QStringLiteral("photos/img1.png"));
    CHECK(first.decisions.at(1).action == Action::None);
    CHECK_FALSE(first.decisions.at(1).approved);

    const Pair& second = restored.pairs.at(1);
    CHECK(second.id == QStringLiteral("pair-2"));
    CHECK(second.decisions.isEmpty());
}

TEST_CASE("Serialized schema carries the version")
{
    const QJsonObject root = QJsonDocument::fromJson(QueueStore::serialize(sampleQueue())).object();

    CHECK(root.value(QStringLiteral("schemaVersion")).toInt(-1) == QueueStore::SCHEMA_VERSION);
    CHECK(root.value(QStringLiteral("pairs")).toArray().size() == 2);
}

TEST_CASE("Garbage bytes deserialize to an empty queue")
{
    const Queue queue = QueueStore::deserialize(QByteArray("this is not json {{{"));

    CHECK(queue.pairs.isEmpty());
}

TEST_CASE("Truncated JSON deserializes to an empty queue")
{
    const QByteArray serialized = QueueStore::serialize(sampleQueue());
    const Queue queue = QueueStore::deserialize(serialized.left(serialized.size() / 2));

    CHECK(queue.pairs.isEmpty());
}

TEST_CASE("An unsupported schema version deserializes to an empty queue")
{
    const Queue future = QueueStore::deserialize(QByteArray(R"({"schemaVersion": 999, "pairs": []})"));
    CHECK(future.pairs.isEmpty());

    const Queue missing = QueueStore::deserialize(QByteArray(R"({"pairs": []})"));
    CHECK(missing.pairs.isEmpty());
}

TEST_CASE("Structural violations deserialize to an empty queue")
{
    // pairs is not an array.
    CHECK(QueueStore::deserialize(QByteArray(R"({"schemaVersion": 1, "pairs": {}})")).pairs.isEmpty());

    // A pair that is not an object.
    CHECK(QueueStore::deserialize(QByteArray(R"({"schemaVersion": 1, "pairs": [42]})")).pairs.isEmpty());

    // A pair missing a required identity field.
    CHECK(QueueStore::deserialize(QByteArray(R"({"schemaVersion": 1, "pairs": [{"id": "x"}]})")).pairs.isEmpty());

    // A decision with an unknown action.
    CHECK(QueueStore::deserialize(QByteArray(R"({
        "schemaVersion": 1,
        "pairs": [{
            "id": "x", "localPath": "l", "remotePath": "r", "remoteHandle": "h",
            "decisions": [{"path": "f.txt", "action": "sideways", "approved": true}]
        }]
    })")).pairs.isEmpty());

    // A decision missing the approval flag.
    CHECK(QueueStore::deserialize(QByteArray(R"({
        "schemaVersion": 1,
        "pairs": [{
            "id": "x", "localPath": "l", "remotePath": "r", "remoteHandle": "h",
            "decisions": [{"path": "f.txt", "action": "local-to-remote"}]
        }]
    })")).pairs.isEmpty());

    // decisions that is not an array.
    CHECK(QueueStore::deserialize(QByteArray(R"({
        "schemaVersion": 1,
        "pairs": [{
            "id": "x", "localPath": "l", "remotePath": "r", "remoteHandle": "h",
            "decisions": {"path": "f.txt"}
        }]
    })")).pairs.isEmpty());
}

TEST_CASE("Empty queue round-trips")
{
    const Queue queue = QueueStore::deserialize(QueueStore::serialize(Queue()));

    CHECK(queue.pairs.isEmpty());
}
