#include "SyncPreviewQueueStore.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

namespace SyncPreview
{
    namespace
    {
        QString actionToString(Action action)
        {
            switch (action)
            {
                case Action::LocalToRemote:
                    return QStringLiteral("local-to-remote");
                case Action::RemoteToLocal:
                    return QStringLiteral("remote-to-local");
                case Action::BestEffort:
                    return QStringLiteral("best-effort");
                case Action::None:
                    return QStringLiteral("none");
            }
            return QStringLiteral("none");
        }

        bool actionFromString(const QString& value, Action& action)
        {
            if (value == QLatin1String("local-to-remote"))
            {
                action = Action::LocalToRemote;
                return true;
            }
            if (value == QLatin1String("remote-to-local"))
            {
                action = Action::RemoteToLocal;
                return true;
            }
            if (value == QLatin1String("best-effort"))
            {
                action = Action::BestEffort;
                return true;
            }
            if (value == QLatin1String("none"))
            {
                action = Action::None;
                return true;
            }
            return false;
        }
    }

    QByteArray QueueStore::serialize(const Queue& queue)
    {
        QJsonObject root;
        root.insert(QStringLiteral("schemaVersion"), SCHEMA_VERSION);

        QJsonArray pairsJson;
        for (const Pair& pair : queue.pairs)
        {
            QJsonObject pairJson;
            pairJson.insert(QStringLiteral("id"), pair.id);
            pairJson.insert(QStringLiteral("localPath"), pair.localPath);
            pairJson.insert(QStringLiteral("remotePath"), pair.remotePath);
            pairJson.insert(QStringLiteral("remoteHandle"), pair.remoteHandle);
            pairJson.insert(QStringLiteral("completed"), pair.completed);

            QJsonArray decisionsJson;
            for (const RowDecision& decision : pair.decisions)
            {
                QJsonObject decisionJson;
                decisionJson.insert(QStringLiteral("path"), decision.relativePath);
                decisionJson.insert(QStringLiteral("action"), actionToString(decision.action));
                decisionJson.insert(QStringLiteral("approved"), decision.approved);
                decisionJson.insert(QStringLiteral("kind"), decision.kind);
                decisionJson.insert(QStringLiteral("requiresApproval"), decision.requiresApproval);
                decisionsJson.append(decisionJson);
            }
            pairJson.insert(QStringLiteral("decisions"), decisionsJson);

            pairsJson.append(pairJson);
        }
        root.insert(QStringLiteral("pairs"), pairsJson);

        return QJsonDocument(root).toJson(QJsonDocument::Compact);
    }

    Queue QueueStore::deserialize(const QByteArray& serialized)
    {
        Queue queue;

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(serialized, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject())
        {
            return queue;
        }

        const QJsonObject root = document.object();
        const int schemaVersion = root.value(QLatin1String("schemaVersion")).toInt(-1);
        // v1 predates the kind/requiresApproval snapshots and the completed
        // flag: those deserialize to their defaults and the re-verify pass
        // re-flags the decisions on first restore. Any other version is
        // unsupported (all-or-nothing → empty queue).
        if (schemaVersion != SCHEMA_VERSION && schemaVersion != SCHEMA_VERSION_V1)
        {
            return queue;
        }

        if (!root.contains(QLatin1String("pairs")) || !root.value(QLatin1String("pairs")).isArray())
        {
            return queue;
        }

        const QJsonArray pairsJson = root.value(QLatin1String("pairs")).toArray();
        for (const QJsonValue& pairValue : pairsJson)
        {
            if (!pairValue.isObject())
            {
                return Queue();
            }
            const QJsonObject pairJson = pairValue.toObject();

            Pair pair;
            pair.id = pairJson.value(QLatin1String("id")).toString();
            pair.localPath = pairJson.value(QLatin1String("localPath")).toString();
            pair.remotePath = pairJson.value(QLatin1String("remotePath")).toString();
            pair.remoteHandle = pairJson.value(QLatin1String("remoteHandle")).toString();
            // toString() on a non-string value yields a null QString; a
            // missing field does the same. Reject both.
            if (pair.id.isNull() || pair.localPath.isNull() || pair.remotePath.isNull() || pair.remoteHandle.isNull())
            {
                return Queue();
            }
            // Optional since v1 files lack it (also tolerated missing in v2).
            pair.completed = pairJson.contains(QLatin1String("completed")) &&
                pairJson.value(QLatin1String("completed")).toBool(false);

            if (pairJson.contains(QLatin1String("decisions")))
            {
                if (!pairJson.value(QLatin1String("decisions")).isArray())
                {
                    return Queue();
                }
                const QJsonArray decisionsJson = pairJson.value(QLatin1String("decisions")).toArray();
                for (const QJsonValue& decisionValue : decisionsJson)
                {
                    if (!decisionValue.isObject())
                    {
                        return Queue();
                    }
                    const QJsonObject decisionJson = decisionValue.toObject();

                    RowDecision decision;
                    decision.relativePath = decisionJson.value(QLatin1String("path")).toString();
                    if (decision.relativePath.isNull())
                    {
                        return Queue();
                    }

                    const QString actionValue = decisionJson.value(QLatin1String("action")).toString();
                    if (actionValue.isNull() || !actionFromString(actionValue, decision.action))
                    {
                        return Queue();
                    }

                    if (!decisionJson.contains(QLatin1String("approved")) || !decisionJson.value(QLatin1String("approved")).isBool())
                    {
                        return Queue();
                    }
                    decision.approved = decisionJson.value(QLatin1String("approved")).toBool(false);

                    // Snapshots are optional (v1 files predate them): absent
                    // kind keeps UNKNOWN_KIND so the re-verify pass re-flags
                    // the decision. Present-but-non-int is a structural
                    // violation.
                    if (decisionJson.contains(QLatin1String("kind")))
                    {
                        const QJsonValue kindValue = decisionJson.value(QLatin1String("kind"));
                        if (!kindValue.isDouble())
                        {
                            return Queue();
                        }
                        decision.kind = kindValue.toInt(RowDecision::UNKNOWN_KIND);
                    }
                    if (decisionJson.contains(QLatin1String("requiresApproval")))
                    {
                        if (!decisionJson.value(QLatin1String("requiresApproval")).isBool())
                        {
                            return Queue();
                        }
                        decision.requiresApproval =
                            decisionJson.value(QLatin1String("requiresApproval")).toBool(false);
                    }

                    pair.decisions.append(decision);
                }
            }

            queue.pairs.append(pair);
        }

        return queue;
    }
}
