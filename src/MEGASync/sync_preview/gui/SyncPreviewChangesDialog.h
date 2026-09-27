#ifndef SYNCPREVIEWCHANGESDIALOG_H
#define SYNCPREVIEWCHANGESDIALOG_H

#include "SyncPreviewPlanner.h"

#include <QDialog>
#include <QHash>
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
    // warning color. A row whose plan carries warnings shows the
    // exclamation-triangle icon on its path cell, that path's warnings in
    // the hover tooltip (MEGA-2.12); warnings of subtree-covered rows fold
    // into their covering directory row's tooltip, so nothing is lost.
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
        // Per-path warnings of the listed plan (keyed by the listed
        // operation path; covered rows fold under their covering path).
        QHash<QString, QStringList> mWarningsByPath;
    };
}
#endif // SYNCPREVIEWCHANGESDIALOG_H
