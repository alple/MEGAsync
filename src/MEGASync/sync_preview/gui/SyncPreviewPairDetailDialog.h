#ifndef SYNCPREVIEWPAIRDETAILDIALOG_H
#define SYNCPREVIEWPAIRDETAILDIALOG_H

#include "SyncPreviewClassifier.h"
#include "SyncPreviewPlanner.h"
#include "SyncPreviewQueue.h"

#include <QDialog>
#include <QHash>

#include <memory>

class QTreeWidgetItem;

namespace Ui
{
    class SyncPreviewPairDetailDialog;
}

namespace SyncPreview
{
    class PairController;

    // MC-style dual-pane review window for ONE queued pair (MEGA-2.7): local
    // pane on the left, remote pane on the right, the action choice between
    // the panes. Opened from the pair-first list; non-modal so several pairs
    // can be reviewed side by side. Shares the pair list's PairController:
    // decisions made here are persisted and re-verified exactly as before.
    // The window closes itself when its pair is removed from the queue.
    class SyncPreviewPairDetailDialog : public QDialog
    {
        Q_OBJECT

    public:
        explicit SyncPreviewPairDetailDialog(const QString& pairId,
                                             PairController* controller,
                                             QWidget* parent = nullptr);
        ~SyncPreviewPairDetailDialog();

        QString pairId() const { return mPairId; }

    private slots:
        void rebuild();
        void onRowActionSelected(const QString& relativePath, Action action);
        void onRowApprovalToggled(const QString& relativePath, bool approved);

    private:
        void setupColumns();
        void applyTreePalette();
        void repopulate();
        void applyRowForeground(QTreeWidgetItem* item, const Row& row, const RowPlan& rowPlan, bool reFlagged);
        QTreeWidgetItem* loadMoreItem(int remaining);
        QString rowBadge(const Row& row, bool reFlagged) const;
        QString rowTooltip(const Row& row) const;
        bool rowVisible(const Row& row, const QString& filter) const;
        QHash<QString, RowDecision> decisionsFor() const;
        void updateSummary();

        const QString mPairId;
        PairController* mController = nullptr;
        std::unique_ptr<Ui::SyncPreviewPairDetailDialog> mUi;
        int mShownCount = 0;
    };
}
#endif // SYNCPREVIEWPAIRDETAILDIALOG_H