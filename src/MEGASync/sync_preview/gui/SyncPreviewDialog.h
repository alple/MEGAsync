#ifndef SYNCPREVIEWDIALOG_H
#define SYNCPREVIEWDIALOG_H

#include "SyncPreviewClassifier.h"
#include "SyncPreviewQueue.h"

#include <QDialog>
#include <QHash>

#include <memory>

class QTreeWidgetItem;

namespace Ui
{
    class SyncPreviewDialog;
}

namespace SyncPreview
{
    class PairController;

    // Pre-commit review dialog (Stage 2: runs entirely on the fake provider;
    // the Stage 5 wiring swaps the pair source and the commit flow). Lists
    // the queued candidate pairs and their classification rows, lets the
    // user choose per-row/per-directory actions with the consequences popup,
    // requires explicit approval on flagged rows, and gates the commit until
    // everything flagged is approved. The queue and the decisions persist
    // across closes and restarts.
    class SyncPreviewDialog : public QDialog
    {
        Q_OBJECT

    public:
        // queueFilePath: explicit override for tests/scaffolding; empty
        // resolves to the fork-owned queue file in the app data directory.
        explicit SyncPreviewDialog(QWidget* parent = nullptr, const QString& queueFilePath = QString());
        ~SyncPreviewDialog();

    private slots:
        void addPair();
        void rebuild();
        void onRowActionSelected(const QString& pairId, const QString& relativePath, Action action);
        void onRowApprovalToggled(const QString& pairId, const QString& relativePath, bool approved);

    private:
        void setupColumns();
        void repopulate();
        void populatePairRows(const Pair& pair, QTreeWidgetItem* pairNode, int shownCount);
        QTreeWidgetItem* loadMoreItem(int remaining);
        QString rowBadge(const Row& row, bool reFlagged) const;
        QString rowTooltip(const Row& row) const;
        bool rowVisible(const Row& row, const QString& filter) const;
        QHash<QString, RowDecision> decisionsFor(const QString& pairId) const;
        void updateSummary();

        std::unique_ptr<Ui::SyncPreviewDialog> mUi;
        PairController* mController = nullptr;
        QHash<QString, int> mShownCounts;  // per pair: displayed row count
    };
}
#endif // SYNCPREVIEWDIALOG_H