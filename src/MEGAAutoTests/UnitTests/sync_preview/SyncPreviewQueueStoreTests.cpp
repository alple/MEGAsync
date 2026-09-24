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

TEST_CASE("Schema v2 round-trips snapshots and the completed flag")
{
    Queue queue;

    Pair pair;
    pair.id = QStringLiteral("pair-1");
    pair.localPath = QStringLiteral("/tmp/local");
    pair.remotePath = QStringLiteral("Cloud Drive/remote");
    pair.remoteHandle = QStringLiteral("h");
    pair.completed = true;

    RowDecision conflict;
    conflict.relativePath = QStringLiteral("old/b.txt");
    conflict.action = Action::RemoteToLocal;
    conflict.approved = true;
    conflict.kind = static_cast<int>(RowKind::Conflict);
    conflict.requiresApproval = true;

    RowDecision plain;
    plain.relativePath = QStringLiteral("docs/a.txt");
    plain.action = Action::LocalToRemote;
    plain.approved = false;
    plain.kind = static_cast<int>(RowKind::LocalOnly);
    plain.requiresApproval = false;

    pair.decisions = {conflict, plain};
    queue.pairs.append(pair);

    const Queue restored = QueueStore::deserialize(QueueStore::serialize(queue));

    REQUIRE(restored.pairs.size() == 1);
    CHECK(restored.pairs.first().completed);
    REQUIRE(restored.pairs.first().decisions.size() == 2);
    CHECK(restored.pairs.first().decisions.at(0).kind == static_cast<int>(RowKind::Conflict));
    CHECK(restored.pairs.first().decisions.at(0).requiresApproval);
    CHECK(restored.pairs.first().decisions.at(1).kind == static_cast<int>(RowKind::LocalOnly));
    CHECK_FALSE(restored.pairs.first().decisions.at(1).requiresApproval);
}

TEST_CASE("Schema v1 files deserialize with unknown snapshots")
{
    const Queue restored = QueueStore::deserialize(QByteArray(R"({
        "schemaVersion": 1,
        "pairs": [{
            "id": "pair-1",
            "localPath": "/tmp/local",
            "remotePath": "Cloud Drive/remote",
            "remoteHandle": "h",
            "decisions": [{"path": "docs/a.txt", "action": "local-to-remote", "approved": true}]
        }]
    })"));

    REQUIRE(restored.pairs.size() == 1);
    CHECK_FALSE(restored.pairs.first().completed);
    REQUIRE(restored.pairs.first().decisions.size() == 1);
    CHECK(restored.pairs.first().decisions.first().kind == RowDecision::UNKNOWN_KIND);
    CHECK_FALSE(restored.pairs.first().decisions.first().requiresApproval);
    CHECK(restored.pairs.first().decisions.first().action == Action::LocalToRemote);
}

TEST_CASE("Structural violations still reject in v2")
{
    // pairs is not an array.
    CHECK(QueueStore::deserialize(QByteArray(R"({"schemaVersion": 2, "pairs": {}})")).pairs.isEmpty());

    // A decision with a non-integer kind snapshot.
    CHECK(QueueStore::deserialize(QByteArray(R"({
        "schemaVersion": 2,
        "pairs": [{
            "id": "x", "localPath": "l", "remotePath": "r", "remoteHandle": "h",
            "decisions": [{"path": "f.txt", "action": "none", "approved": false, "kind": "blocker"}]
        }]
    })")).pairs.isEmpty());

    // A decision with a non-boolean requiresApproval snapshot.
    CHECK(QueueStore::deserialize(QByteArray(R"({
        "schemaVersion": 2,
        "pairs": [{
            "id": "x", "localPath": "l", "remotePath": "r", "remoteHandle": "h",
            "decisions": [{"path": "f.txt", "action": "none", "approved": false, "requiresApproval": "yes"}]
        }]
    })")).pairs.isEmpty());
}
