#ifndef SYNCPREVIEWROWWIDGET_H
#define SYNCPREVIEWROWWIDGET_H

#include "SyncPreviewPlanner.h"

#include <QWidget>

class QComboBox;
class QCheckBox;

namespace SyncPreview
{
    // The "action & approval" cell of one classification row: the action
    // combo (recommended action pre-marked and pre-selected, everything
    // overridable; disabled on blocker rows, which need rename/exclusion
    // rather than a transfer) and the explicit-approval checkbox on rows
    // that require one. The dual-pane columns (path, per-side size and
    // modified date, newer-side marker) are plain tree items owned by the
    // dialog.
    class SyncPreviewRowWidget : public QWidget
    {
        Q_OBJECT

    public:
        // approved: the pair's current decision on this row (approval only
        // lives in the controller, not in the planner's RowPlan).
        SyncPreviewRowWidget(const Row& row,
                             const RowPlan& plan,
                             bool approved,
                             bool reFlagged,
                             QWidget* parent = nullptr);

        // Current effective action (what the combo shows when untouched).
        Action effectiveAction() const { return mPlan.action; }

    signals:
        // Raw combo selection: emitted for files and directories alike. The
        // dialog applies file actions directly and routes directory actions
        // through the consequences popup first.
        void actionSelected(const QString& relativePath, Action action);
        void approvalToggled(const QString& relativePath, bool approved);

    private:
        QString actionLabel(Action action, bool recommended) const;

        const Row mRow;
        const RowPlan mPlan;
        QComboBox* mActionCombo = nullptr;
        QCheckBox* mApproveBox = nullptr;
    };
}
#endif // SYNCPREVIEWROWWIDGET_H