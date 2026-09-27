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
    // sit above the panes. Between the panes sits a narrow decision column
    // (MEGA-2.12): per-row compact arrow buttons (→, ←, ↔) that carry the
    // bottom action panel's decide+approve gesture onto every row, without
    // selecting it first. The selected row's action buttons and state
    // readout still live in the panel UNDER the trees (tester: both stay).
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

    private:
        void setupPanes();
        void buildActionPanel();
        void applyPanesPalette();
        void repopulate();
        void updateActionPanel();
        // The per-row decision column (MEGA-2.12): re-derives every row's
        // checked/enabled arrow state from the current decisions.
        void updateDecisionColumn();
        // The arrows decide AND approve in one gesture (MEGA-2.11 AC#8);
        // toggling the active arrow off records an explicit do-nothing.
        void onActionButtonClicked(const QString& relativePath, Action choice, bool checked);
        void onRowDecisionCleared(const QString& relativePath);
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
        // The decision column's row for one classification row: no text,
        // just the three compact arrow buttons (MEGA-2.12).
        QTreeWidgetItem* makeDecisionItem(const Row& row, QTreeWidgetItem* parent);
        // The tooltip shared by the bottom panel's and the column's arrow
        // buttons (action prose + recommendation/blocker notes).
        QString decisionButtonTooltip(const Row& row, Action choice, bool decidable) const;
        QTreeWidgetItem* loadMoreItem(int remaining);
        QString rowTooltip(const Row& row) const;
        bool rowVisible(const Row& row, const QString& filter) const;
        QHash<QString, RowDecision> decisionsFor() const;
        void updateSummary();
        // The directory action's consequences as a one-line footer note
        // (MEGA-2.12: replaces the popup): built from the preview plan at
        // click time, rendered in updateSummary, full breakdown in the
        // tooltip; the note's path invalidates it (un-decide / Apply).
        void setDirectoryNote(const QString& directoryPath, const RowPlan* directoryPlan);
        void renderDirectoryNote();

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
        // The decision column's rows keyed by path (MEGA-2.12), plus the
        // per-row arrow buttons so their checked state can be re-derived.
        QHash<QString, QTreeWidgetItem*> mMidItems;
        QHash<QString, QVector<QPushButton*>> mMidButtonsByPath;
        // Expansion and selection survive repopulation (filter edits,
        // decisions, themes). The left map is the reference; with
        // Synchronize view OFF the right pane keeps its own expansion map
        // and selection (MEGA-2.11 AC#6), so a rebuild never snaps a
        // freely-browsed pane back into lock-step.
        QHash<QString, bool> mExpandedByPath;
        QHash<QString, bool> mRightExpandedByPath;
        QString mLeftSelectedPath;
        QString mRightSelectedPath;
        // The path whose row the action panel shows; empty = no selection.
        QString mSelectedPath;
        bool mSyncingPanes = false;
        // Synchronize view is a toggle (MEGA-2.11 AC#6, default ON): ON
        // keeps the panes mirroring continuously (expansion, selection,
        // scroll); OFF lets each pane be browsed freely (expansion is still
        // recorded per path, so rebuilds never fight the free state).
        bool mPanesLocked = true;

        // The action panel under the trees (pressable buttons, no combo).
        QFrame* mActionPanel = nullptr;
        QLabel* mPanelPathLabel = nullptr;
        QLabel* mPanelStatesLabel = nullptr;
        QVector<QPushButton*> mActionButtons;
        QLabel* mPanelHintLabel = nullptr;
        QLabel* mPanelNotesLabel = nullptr;

        // The directory action's consequences, footer-line form
        // (MEGA-2.12). Empty path = no note; the label lives in the .ui
        // footer layout and is created in the constructor.
        QLabel* mDirectoryNoteLabel = nullptr;
        QString mDirectoryNotePath;
        QString mDirectoryNoteLine;
        QString mDirectoryNoteTooltip;

        int mShownCount = 0;
    };
}
#endif // SYNCPREVIEWPAIRDETAILDIALOG_H
