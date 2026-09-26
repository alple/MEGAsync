#ifndef SYNCPREVIEWCHANGESDIALOG_H
#define SYNCPREVIEWCHANGESDIALOG_H

#include "SyncPreviewPlanner.h"

#include <QDialog>
#include <QStringList>

class QLabel;
class QTreeWidget;

namespace SyncPreview
{
    // Scheduled-changes list for one pair (MEGA-2.9 review loop): every
    // planned operation under the current decisions, one row per operation
    // — path, the change it performs (transfer direction, overwrite,
    // rename with its from → to paths, removal destination), and folder
    // subtree notes. Rows still awaiting approval are marked in the
    // warning color; the planner's aggregated warnings close the list.
    // The Apply step lives here (MEGA-2.11 AC#7): Apply accepts the dialog
    // and the caller executes the listed plan (fake data); Close rejects.
    // Stack-allocated and exec()'d by its callers — no WA_DeleteOnClose
    // (deleting a stack object aborts the app, MEGA-2.11 AC#1).
    class SyncPreviewChangesDialog : public QDialog
    {
        Q_OBJECT

    public:
        explicit SyncPreviewChangesDialog(const QString& pairTitle,
                                          const Plan& plan,
                                          const QStringList& awaitingPaths,
                                          QWidget* parent = nullptr);

    private:
        // Token colors for the window, labels, tree rows and the button
        // box; re-resolved on live theme changes.
        void applyPalette();

        QTreeWidget* mChangesTree = nullptr;
        QLabel* mHeaderLabel = nullptr;
        QLabel* mEmptyLabel = nullptr;
        QLabel* mNotesLabel = nullptr;
        QStringList mWarningLines;
    };
}
#endif // SYNCPREVIEWCHANGESDIALOG_H
