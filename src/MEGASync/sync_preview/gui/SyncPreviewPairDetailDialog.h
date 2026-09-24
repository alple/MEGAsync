#ifndef SYNCPREVIEWPAIRDETAILDIALOG_H
#define SYNCPREVIEWPAIRDETAILDIALOG_H

#include "SyncPreviewClassifier.h"
#include "SyncPreviewPlanner.h"
#include "SyncPreviewQueue.h"

#include <QDialog>
#include <QHash>

#include <memory>

class QColor;
class QScrollBar;
class QTreeWidget;
class QTreeWidgetItem;

namespace Ui
{
    class SyncPreviewPairDetailDialog;
}

namespace SyncPreview
{
    class PairController;

    // MC-style dual-pane review window for ONE queued pair (MEGA-2.7): a
    // local tree pane on the left, a remote tree pane on the right, and the
    // action choice on a fixed strip BETWEEN the panes. The panes are
    // row-locked: every classified path is one row on all three views (a
    // side that misses the entry shows an empty pane cell), scrolling and
    // selection move all panes together, like Midnight Commander. Opened
    // from the pair-first list; non-modal so several pairs can be reviewed
    // side by side. Shares the pair list's PairController: decisions made
    // here are persisted and re-verified exactly as before. The window
    // closes itself when its pair is removed from the queue.
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
        void setupPanes();
        void applyPanesPalette();
        void repopulate();
        // Keeps the panes aligned: same row index on every view.
        void syncScrollFrom(QScrollBar* source);
        void syncSelectionFrom(QTreeWidget* source);
        QTreeWidgetItem* addRowToTree(QTreeWidget* tree,
                                      const QString& pathText,
                                      const QString& sizeText,
                                      const QString& timeText,
                                      const QString& tooltip,
                                      const QColor& color);
        QWidget* buildRowWidget(const Row& row,
                                const RowPlan& rowPlan,
                                bool approved,
                                bool reFlagged,
                                QWidget* parent);
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
        bool mSyncingPanes = false;
    };
}
#endif // SYNCPREVIEWPAIRDETAILDIALOG_H