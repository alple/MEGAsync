#ifndef SYNCPREVIEWCONSEQUENCESDIALOG_H
#define SYNCPREVIEWCONSEQUENCESDIALOG_H

#include "SyncPreviewPlanner.h"

#include <QDialog>

namespace SyncPreview
{
    // Consequences popup shown when the user explicitly chooses an action on
    // a directory row: lists which files are created, overwritten (changed)
    // and removed per side under the directory, plus the planner's warnings.
    // Confirm applies the choice, cancel leaves the previous action in place.
    // Stack-allocated and exec()'d by its caller — no WA_DeleteOnClose
    // (deleting a stack object aborts the app, MEGA-2.11 AC#1).
    class SyncPreviewConsequencesDialog : public QDialog
    {
        Q_OBJECT

    public:
        explicit SyncPreviewConsequencesDialog(const QString& directoryPath,
                                               const RowPlan& directoryPlan,
                                               QWidget* parent = nullptr);

    private:
        // Token colors for the window, labels and the button box;
        // re-resolved on live theme changes.
        void applyPalette();
    };
}
#endif // SYNCPREVIEWCONSEQUENCESDIALOG_H