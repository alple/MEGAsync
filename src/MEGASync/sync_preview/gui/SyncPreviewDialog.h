#ifndef SYNCPREVIEWDIALOG_H
#define SYNCPREVIEWDIALOG_H

#include "SyncPreviewQueue.h"

#include <QDialog>
#include <QHash>
#include <QPointer>

#include <memory>

class QListWidgetItem;

namespace Ui
{
    class SyncPreviewDialog;
}

namespace SyncPreview
{
    class PairController;
    class SyncPreviewPairDetailDialog;

    // Pre-commit review dialog (Stage 2: runs entirely on the fake provider;
    // the Stage 5 wiring swaps the pair source and the commit flow). Pair-
    // first flow (MEGA-2.7): the dialog lists the queued candidate pairs with
    // per-pair summary details; opening a pair raises its own MC-style
    // detail window, where per-row actions, the consequences popup, the
    // approval gate and the filters live. The queue and the decisions
    // persist across closes and restarts.
    class SyncPreviewDialog : public QDialog
    {
        Q_OBJECT

    public:
        // queueFilePath: explicit override for tests/scaffolding; empty
        // resolves to the fork-owned queue file in the app data directory.
        explicit SyncPreviewDialog(QWidget* parent = nullptr, const QString& queueFilePath = QString());
        ~SyncPreviewDialog();

    private slots:
        void addPair();
        void rebuild();
        void openPair(const QString& pairId);

    private:
        void repopulate();
        QString pairStatsText(const Pair& pair) const;
        QString pairPendingText(const Pair& pair) const;
        bool pairVisible(const Pair& pair, const QString& filter) const;
        void updateSummary();
        void applyListPalette();

        std::unique_ptr<Ui::SyncPreviewDialog> mUi;
        PairController* mController = nullptr;
        // One detail window per pair; QPointer nulls itself on destroy.
        QHash<QString, QPointer<QDialog>> mDetailWindows;
    };
}
#endif // SYNCPREVIEWDIALOG_H