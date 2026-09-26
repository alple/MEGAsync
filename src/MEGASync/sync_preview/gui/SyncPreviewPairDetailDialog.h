#ifndef SYNCPREVIEWPAIRDETAILDIALOG_H
#define SYNCPREVIEWPAIRDETAILDIALOG_H

#include "SyncPreviewClassifier.h"
#include "SyncPreviewPlanner.h"
#include "SyncPreviewQueue.h"

#include <QDialog>
#include <QHash>
#include <QVector>

#include <memory>

class QButtonGroup;
class QFrame;
class QLabel;
class QPushButton;
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

    // Meld-style folder-diff review window for ONE queued pair (MEGA-2.8):
    // a local tree on the left, a remote tree on the right, both expandable
    // and row-locked — expansion, selection and scrolling move together by
    // path, like meld's folder comparison. Each pane reads its own side with
    // meld's states — Same / Modified / New / Missing / Blocked — colored
    // per SIDE, so one row can read New on the left and Missing on the
    // right; state filters (Same hidden by default, blockers always shown)
    // sit above the panes. There is no middle strip: the selected row's
    // action buttons and approval toggle live in a panel UNDER the trees.
    // Opened from the pair-first list; non-modal so several pairs can be
    // reviewed side by side. Shares the pair list's PairController: decisions
    // made here are persisted and re-verified exactly as before. The window
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
        void buildActionPanel();
        void applyPanesPalette();
        void repopulate();
        void updateActionPanel();
        // Review-loop surface (MEGA-2.9): the scheduled-changes list and
        // the fake-data Apply step (controller re-scans + re-verifies; the
        // pairChanged signal drives the rebuild).
        void showChanges();
        void applyPlan();
        // Pane-view normalizer (MEGA-2.10): mirrors the left pane's
        // per-path expansion onto the right pane.
        void synchronizeView();
        // Keeps the panes aligned: same path expanded, selected, scrolled.
        void syncScrollFrom(QScrollBar* source);
        void syncSelectionFrom(QTreeWidget* source);
        void syncExpansionFrom(QTreeWidget* source, QTreeWidgetItem* item, bool expanded);
        void addSubtree(QTreeWidget* leftTree,
                        QTreeWidget* rightTree,
                        QTreeWidgetItem* leftParent,
                        QTreeWidgetItem* rightParent,
                        const QString& parentKey);
        QTreeWidgetItem* makeSideItem(QTreeWidget* tree,
                                      QTreeWidgetItem* parent,
                                      const Row& row,
                                      bool localSide);
        QTreeWidgetItem* loadMoreItem(int remaining);
        QString rowTooltip(const Row& row) const;
        bool rowVisible(const Row& row, const QString& filter) const;
        QHash<QString, RowDecision> decisionsFor() const;
        void updateSummary();

        const QString mPairId;
        PairController* mController = nullptr;
        std::unique_ptr<Ui::SyncPreviewPairDetailDialog> mUi;

        // Rows of the current classification keyed by path (repopulation
        // scope); the tree views hold no decisions themselves.
        QHash<QString, const Row*> mRowsByPath;
        // Tree items of the current population keyed by path, per pane —
        // what keeps expansion/selection/scrolling in path lock-step.
        QHash<QString, QTreeWidgetItem*> mLeftItems;
        QHash<QString, QTreeWidgetItem*> mRightItems;
        // Expansion survives repopulation (filter edits, decisions, themes).
        QHash<QString, bool> mExpandedByPath;
        // The path whose row the action panel shows; empty = no selection.
        QString mSelectedPath;
        bool mSyncingPanes = false;

        // The action panel under the trees (pressable buttons, no combo).
        QFrame* mActionPanel = nullptr;
        QLabel* mPanelPathLabel = nullptr;
        QLabel* mPanelStatesLabel = nullptr;
        QVector<QPushButton*> mActionButtons;
        QButtonGroup* mActionGroup = nullptr;
        QPushButton* mApproveButton = nullptr;
        QLabel* mPanelHintLabel = nullptr;
        QLabel* mPanelNotesLabel = nullptr;

        int mShownCount = 0;
    };
}
#endif // SYNCPREVIEWPAIRDETAILDIALOG_H
