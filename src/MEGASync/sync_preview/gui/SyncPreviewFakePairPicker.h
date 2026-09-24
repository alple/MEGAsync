#ifndef SYNCPREVIEWFAKEPAIRPICKER_H
#define SYNCPREVIEWFAKEPAIRPICKER_H

#include "FakeSyncPreviewProvider.h"
#include "SyncPreviewPairController.h"

#include <QDialog>

class QListWidget;

#include <optional>

namespace SyncPreview
{
    // Stage-2 scaffolding: the stand-in "add pair" source while the review
    // dialog runs on fake data. Lists the canned fake scenarios instead of
    // real local/remote folders; Stage 5 replaces this with the real pair
    // picker (Add-Sync wizard flow). Also owns the label <-> scenario
    // mapping the dialog's side-provider factory uses to rebuild providers
    // for pairs restored from the queue file.
    class SyncPreviewFakePairPicker : public QDialog
    {
        Q_OBJECT

    public:
        explicit SyncPreviewFakePairPicker(QWidget* parent = nullptr);

        // The candidate the user picked; empty localPath when cancelled.
        PairCandidate selectedCandidate() const;

        static QStringList scenarioLabels();
        static std::optional<FakeScenario> scenarioForLabel(const QString& label);

    private:
        QListWidget* mScenarioList = nullptr;
    };
}
#endif // SYNCPREVIEWFAKEPAIRPICKER_H