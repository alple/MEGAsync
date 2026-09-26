#ifndef SYNCPREVIEWCHANGESDIALOG_H
#define SYNCPREVIEWCHANGESDIALOG_H

#include "SyncPreviewPlanner.h"

#include <QDialog>
#include <QStringList>

class QTreeWidget;

namespace SyncPreview
{
    // Scheduled-changes list for one pair (MEGA-2.9 review loop): every
    // planned operation under the current decisions, one row per operation
    // — path, the change it performs (transfer direction, overwrite,
    // rename with its from → to paths, removal destination), and folder
    // subtree notes. Rows still awaiting approval are marked in the
    // warning color; the planner's aggregated warnings close the list.
    // Read-only companion to the Apply step: it always reflects the plan
    // Apply would execute on the fake data.
    class SyncPreviewChangesDialog : public QDialog
    {
        Q_OBJECT

    public:
        explicit SyncPreviewChangesDialog(const QString& pairTitle,
                                          const Plan& plan,
                                          const QStringList& awaitingPaths,
                                          QWidget* parent = nullptr);

    private:
        QTreeWidget* mChangesTree = nullptr;
    };
}

#endif // SYNCPREVIEWCHANGESDIALOG_H