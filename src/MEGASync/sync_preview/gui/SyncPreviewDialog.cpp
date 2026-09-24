#include "SyncPreviewDialog.h"
#include "ui_SyncPreviewDialog.h"
#include "SyncPreviewFakePairPicker.h"
#include "SyncPreviewPairController.h"
#include "SyncPreviewQueueFileStore.h"
#include "SyncPreviewRowWidget.h"
#include "SyncPreviewConsequencesDialog.h"

#include "Preferences.h"

#include <QCheckBox>
#include <QDateTime>
#include <QDir>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QTreeWidgetItem>

namespace Ui
{
    class SyncPreviewDialog;
}

namespace SyncPreview
{
    namespace
    {
        constexpr int kRowCap = 200;

        QString pairLabel(const Pair& pair)
        {
            return pair.localPath + QStringLiteral("  <->  ") + pair.remotePath;
        }

        QString indentFor(const QString& relativePath)
        {
            const int depth = relativePath.count(QLatin1Char('/'));
            return QString(2 * depth, QLatin1Char(' '));
        }

        QString sizeText(const std::optional<Entry>& entry)
        {
            if (!entry || entry->isFolder())
            {
                return QStringLiteral("-");
            }
            const qint64 bytes = entry->size;
            if (bytes < 1024)
            {
                return QStringLiteral("%1 B").arg(bytes);
            }
            const double kb = static_cast<double>(bytes) / 1024.0;
            if (kb < 1024.0)
            {
                return QStringLiteral("%1 KB").arg(kb, 0, 'f', 1);
            }
            return QStringLiteral("%1 MB").arg(kb / 1024.0, 0, 'f', 1);
        }

        QString timeText(const std::optional<Entry>& entry)
        {
            if (!entry || entry->modifiedTime <= 0)
            {
                return QStringLiteral("-");
            }
            const QDateTime dateTime = QDateTime::fromSecsSinceEpoch(entry->modifiedTime);
            return dateTime.date().toString(QStringLiteral("yyyy-MM-dd")) + QLatin1Char(' ') +
                dateTime.time().toString(QStringLiteral("HH:mm"));
        }

        QString newerMarker(const Row& row)
        {
            if (!row.local || !row.remote || row.local->isFolder() || row.remote->isFolder())
            {
                return QStringLiteral("-");
            }
            if (row.local->modifiedTime > row.remote->modifiedTime)
            {
                return QStringLiteral("local");
            }
            if (row.local->modifiedTime < row.remote->modifiedTime)
            {
                return QStringLiteral("remote");
            }
            return QStringLiteral("same");
        }
    }

    SyncPreviewDialog::SyncPreviewDialog(QWidget* parent, const QString& queueFilePath) :
        QDialog(parent),
        mUi(new Ui::SyncPreviewDialog)
    {
        mUi->setupUi(this);
        setAttribute(Qt::WA_DeleteOnClose);

        QString path = queueFilePath;
        if (path.isEmpty())
        {
            // The fork-owned queue file in the app data directory; Stage 5
            // keeps this location for the real commit queue.
            path = Preferences::instance()->getDataPath() + QDir::separator() +
                QStringLiteral("sync-preview-queue.json");
        }

        mController = new PairController(QueueFileStore(path), this);
        // Stage 2 factory: the pair label identifies the fake scenario, so
        // pairs restored from the queue file re-scan to the same data.
        mController->setSideProviderFactory(
            [](const Pair& pair) -> std::optional<PairSideProviders>
            {
                const std::optional<FakeScenario> scenario =
                    SyncPreviewFakePairPicker::scenarioForLabel(pair.localPath);
                if (!scenario)
                {
                    return std::nullopt;
                }
                return PairSideProviders{std::make_shared<FakeSideProvider>(scenario->local),
                                         std::make_shared<FakeSideProvider>(scenario->remote)};
            });

        connect(mUi->addPairButton, &QPushButton::clicked, this, &SyncPreviewDialog::addPair);
        connect(mUi->closeButton, &QPushButton::clicked, this, &QDialog::close);
        connect(mUi->filterEdit, &QLineEdit::textChanged, this, [this]()
        {
            mShownCounts.clear();
            rebuild();
        });
        connect(mUi->groupByPairToggle, &QCheckBox::toggled, this, [this](bool) { rebuild(); });
        connect(mUi->showInSyncToggle, &QCheckBox::toggled, this, [this](bool) { rebuild(); });

        connect(mController, &PairController::pairAdded, this, [this](const QString&) { rebuild(); });
        connect(mController, &PairController::pairRemoved, this, [this](const QString&) { rebuild(); });
        connect(mController, &PairController::pairChanged, this, [this](const QString&) { rebuild(); });
        connect(mController, &PairController::restored, this, [this]() { rebuild(); });

        setupColumns();
        mController->restore();
        rebuild();
    }

    SyncPreviewDialog::~SyncPreviewDialog() = default;

    void SyncPreviewDialog::setupColumns()
    {
        QTreeWidget* tree = mUi->rowsTree;
        tree->setSortingEnabled(false);
        QHeaderView* header = tree->header();
        header->setSectionResizeMode(0, QHeaderView::Stretch);
        header->setSectionResizeMode(1, QHeaderView::Fixed);
        header->setSectionResizeMode(2, QHeaderView::Fixed);
        header->setSectionResizeMode(3, QHeaderView::Fixed);
        header->setSectionResizeMode(4, QHeaderView::Fixed);
        header->setSectionResizeMode(5, QHeaderView::Fixed);
        header->setSectionResizeMode(6, QHeaderView::Interactive);
        tree->setColumnWidth(1, 90);
        tree->setColumnWidth(2, 130);
        tree->setColumnWidth(3, 90);
        tree->setColumnWidth(4, 130);
        tree->setColumnWidth(5, 100);
        tree->setColumnWidth(6, 420);
    }

    void SyncPreviewDialog::addPair()
    {
        SyncPreviewFakePairPicker picker(this);
        if (picker.exec() != QDialog::Accepted)
        {
            return;
        }

        const PairCandidate candidate = picker.selectedCandidate();
        if (candidate.localPath.isEmpty())
        {
            return;
        }

        mController->addPair(candidate);
    }

    void SyncPreviewDialog::rebuild()
    {
        const int scrollPosition = mUi->rowsTree->verticalScrollBar()->value();
        repopulate();
        updateSummary();
        mUi->rowsTree->verticalScrollBar()->setValue(scrollPosition);
    }

    void SyncPreviewDialog::repopulate()
    {
        const bool grouped = mUi->groupByPairToggle->isChecked();

        QTreeWidget* tree = mUi->rowsTree;
        tree->clear();
        mShownCounts.reserve(mController->pairs().size());

        for (const Pair& pair : mController->pairs())
        {
            if (!mShownCounts.contains(pair.id))
            {
                mShownCounts.insert(pair.id, kRowCap);
            }
            const int shownCount = mShownCounts.value(pair.id, kRowCap);

            if (grouped)
            {
                auto* pairNode = new QTreeWidgetItem(tree);
                pairNode->setFirstColumnSpanned(true);

                auto* pairHeader = new QWidget(tree);
                auto* headerLayout = new QHBoxLayout(pairHeader);
                headerLayout->setContentsMargins(4, 2, 4, 2);

                auto* label = new QLabel(pairLabel(pair), pairHeader);
                QFont boldFont = label->font();
                boldFont.setBold(true);
                label->setFont(boldFont);
                headerLayout->addWidget(label);

                const int awaiting = mController->awaitingApprovalCount(pair.id);
                auto* countsLabel = new QLabel(awaiting > 0
                    ? tr("%1 item(s) awaiting approval").arg(awaiting)
                    : tr("all flagged items approved"), pairHeader);
                headerLayout->addWidget(countsLabel);
                headerLayout->addStretch(1);

                auto* commitButton = new QPushButton(tr("Commit pair"), pairHeader);
                commitButton->setToolTip(tr("Opens the pre-filled create-sync dialog (Stage 5)"));
                commitButton->setEnabled(mController->allApproved(pair.id));
                connect(commitButton, &QPushButton::clicked, this, [this]()
                {
                    QMessageBox::information(this,
                                             tr("Sync pre-commit review"),
                                             tr("The commit flow arrives in Stage 5; the reviewed decisions are already persisted."));
                });
                headerLayout->addWidget(commitButton);

                auto* removeButton = new QPushButton(tr("Remove"), pairHeader);
                connect(removeButton, &QPushButton::clicked, this, [this, pairId = pair.id]()
                {
                    if (QMessageBox::question(this,
                                              tr("Remove pair"),
                                              tr("Remove this pair from the review queue?")) == QMessageBox::Yes)
                    {
                        mController->removePair(pairId);
                    }
                });
                headerLayout->addWidget(removeButton);

                tree->setItemWidget(pairNode, 0, pairHeader);
                populatePairRows(pair, pairNode, shownCount);
            }
            else
            {
                populatePairRows(pair, nullptr, shownCount);
            }
        }
    }

    void SyncPreviewDialog::populatePairRows(const Pair& pair, QTreeWidgetItem* pairNode, int shownCount)
    {
        const Classification& classification = mController->classification(pair.id);
        const QHash<QString, RowDecision> decisions = decisionsFor(pair.id);
        const QStringList reFlagged = mController->reFlaggedPaths(pair.id);
        const Plan plan = mController->plan(pair.id);

        const QString filter = mUi->filterEdit->text();
        const bool showInSync = mUi->showInSyncToggle->isChecked();

        QTreeWidget* tree = mUi->rowsTree;
        int displayed = 0;
        int remaining = 0;

        for (const Row& row : classification.rows)
        {
            if (!rowVisible(row, filter) || (!showInSync && row.kind == RowKind::Identical))
            {
                continue;
            }

            if (displayed >= shownCount)
            {
                ++remaining;
                continue;
            }

const RowPlan* rowPlan = plan.find(row.relativePath);
            static const RowPlan emptyPlan;
            const bool reFlaggedRow = reFlagged.contains(row.relativePath);

            const QString indent = indentFor(row.relativePath);
            const QString badge = rowBadge(row, reFlaggedRow);
            // Grouped mode: path only. Flat mode: the pair label prefixes
            // the path so rows from several pairs stay identifiable.
            const QString displayPath = pairNode
                ? row.relativePath
                : QStringLiteral("%1 — %2").arg(pair.localPath, row.relativePath);

            auto* rowItem = new QTreeWidgetItem();
            QString pathText = indent;
            if (!badge.isEmpty())
            {
                pathText += QStringLiteral("[%1]  ").arg(badge);
            }
            pathText += displayPath;
            rowItem->setText(0, pathText);
            rowItem->setText(1, sizeText(row.local));
            rowItem->setText(2, timeText(row.local));
            rowItem->setText(3, sizeText(row.remote));
            rowItem->setText(4, timeText(row.remote));
            rowItem->setText(5, newerMarker(row));
            rowItem->setToolTip(0, rowTooltip(row));

            auto* rowWidget = new SyncPreviewRowWidget(row,
                                                       rowPlan ? *rowPlan : emptyPlan,
                                                       decisions.value(row.relativePath).approved,
                                                       reFlaggedRow,
                                                       tree);
            const QString pairId = pair.id;
            connect(rowWidget,
                    &SyncPreviewRowWidget::actionSelected,
                    this,
                    [this, pairId](const QString& relativePath, Action action)
                    {
                        onRowActionSelected(pairId, relativePath, action);
                    });
            connect(rowWidget,
                    &SyncPreviewRowWidget::approvalToggled,
                    this,
                    [this, pairId](const QString& relativePath, bool approved)
                    {
                        onRowApprovalToggled(pairId, relativePath, approved);
                    });

            if (pairNode)
            {
                pairNode->addChild(rowItem);
            }
            else
            {
                tree->addTopLevelItem(rowItem);
            }
            tree->setItemWidget(rowItem, 6, rowWidget);

            ++displayed;
        }

        if (remaining > 0)
        {
            QTreeWidgetItem* loadMore = loadMoreItem(remaining);
            if (pairNode)
            {
                pairNode->addChild(loadMore);
            }
            else
            {
                tree->addTopLevelItem(loadMore);
            }
            auto* loadMoreButton = new QPushButton(tr("Load more (%1 remaining)").arg(remaining), tree);
            const QString loadPairId = pair.id;
            connect(loadMoreButton, &QPushButton::clicked, this, [this, loadPairId]()
            {
                mShownCounts[loadPairId] += kRowCap;
                rebuild();
            });
            tree->setItemWidget(loadMore, 0, loadMoreButton);
        }
    }

    QTreeWidgetItem* SyncPreviewDialog::loadMoreItem(int remaining)
    {
        auto* item = new QTreeWidgetItem();
        item->setText(0, tr("Load more (%1 remaining)").arg(remaining));
        return item;
    }

    QString SyncPreviewDialog::rowBadge(const Row& row, bool reFlagged) const
    {
        QString badge;
        switch (row.kind)
        {
            case RowKind::Identical:
                break;
            case RowKind::LocalOnly:
                break;
            case RowKind::RemoteOnly:
                break;
            case RowKind::BothDiffer:
                badge = QStringLiteral("CONFLICT: both differ");
                break;
            case RowKind::Conflict:
                badge = QStringLiteral("CONFLICT: same content, different name");
                break;
            case RowKind::Blocker:
                badge = (row.blockerReason == BlockerReason::TypeMismatch)
                    ? QStringLiteral("BLOCKER: file vs folder")
                    : QStringLiteral("BLOCKER: case-insensitive name collision");
                break;
        }

        if (reFlagged)
        {
            badge += badge.isEmpty() ? QString() : QStringLiteral(" · ");
            badge += QStringLiteral("changed — re-approve");
        }
        return badge;
    }

    QString SyncPreviewDialog::rowTooltip(const Row& row) const
    {
        QStringList notes;
        if (row.hasIdenticalTwin && !row.twinPath.isEmpty())
        {
            notes << tr("Identical twin: %1").arg(row.twinPath);
        }
        if (row.underBlockedPath)
        {
            notes << tr("Under a blocked path: resolve the blocker above first");
        }
        if (row.kind == RowKind::Blocker)
        {
            notes << tr("Not resolvable by a transfer: needs rename/exclusion (later stage)");
        }
        return notes.join(QLatin1Char('\n'));
    }

    bool SyncPreviewDialog::rowVisible(const Row& row, const QString& filter) const
    {
        if (!filter.isEmpty() && !row.relativePath.contains(filter, Qt::CaseInsensitive))
        {
            return false;
        }
        return true;
    }

    QHash<QString, RowDecision> SyncPreviewDialog::decisionsFor(const QString& pairId) const
    {
        QHash<QString, RowDecision> decisions;
        if (const Pair* pair = mController->pair(pairId))
        {
            for (const RowDecision& decision : pair->decisions)
            {
                decisions.insert(decision.relativePath, decision);
            }
        }
        return decisions;
    }

    void SyncPreviewDialog::updateSummary()
    {
        int awaiting = 0;
        for (const Pair& pair : mController->pairs())
        {
            awaiting += mController->awaitingApprovalCount(pair.id);
        }

        if (mController->pairs().isEmpty())
        {
            mUi->summaryLabel->setText(tr("No pairs queued yet — add a demo pair."));
        }
        else if (awaiting == 0)
        {
            mUi->summaryLabel->setText(tr("%1 pair(s) queued — everything flagged is approved.")
                                           .arg(mController->pairs().size()));
        }
        else
        {
            mUi->summaryLabel->setText(tr("%1 pair(s) queued — %2 item(s) awaiting approval.")
                                           .arg(mController->pairs().size())
                                           .arg(awaiting));
        }
    }

    void SyncPreviewDialog::onRowActionSelected(const QString& pairId, const QString& relativePath, Action action)
    {
        const Classification& classification = mController->classification(pairId);
        const Row* row = classification.find(relativePath);
        const bool isDirectory = row && ((row->local && row->local->isFolder()) ||
                                         (row->remote && row->remote->isFolder()));

        if (isDirectory)
        {
            // Directory-level actions trigger the consequences popup before
            // anything is applied; canceling restores the previous state.
            const Plan preview = mController->previewPlan(pairId, relativePath, action);
            const RowPlan* directoryPlan = preview.find(relativePath);
            static const RowPlan emptyPlan;
            SyncPreviewConsequencesDialog popup(relativePath, directoryPlan ? *directoryPlan : emptyPlan, this);
            if (popup.exec() != QDialog::Accepted)
            {
                rebuild();
                return;
            }
        }

        mController->setAction(pairId, relativePath, action);
    }

    void SyncPreviewDialog::onRowApprovalToggled(const QString& pairId, const QString& relativePath, bool approved)
    {
        mController->setApproved(pairId, relativePath, approved);
    }
}