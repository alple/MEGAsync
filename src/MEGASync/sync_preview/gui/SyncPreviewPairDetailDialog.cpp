#include "SyncPreviewPairDetailDialog.h"
#include "ui_SyncPreviewPairDetailDialog.h"
#include "SyncPreviewGuiFormat.h"
#include "SyncPreviewPairController.h"
#include "SyncPreviewRowWidget.h"
#include "SyncPreviewConsequencesDialog.h"

#include "ThemeManager.h"
#include "TokenParserWidgetManager.h"

#include <QBrush>
#include <QCheckBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPalette>
#include <QPushButton>
#include <QScrollBar>
#include <QTreeWidgetItem>

namespace SyncPreview
{
    namespace
    {
        constexpr int kRowCap = 200;

        // Columns of the MC-style row: local pane | action | remote pane.
        constexpr int kColLocalPath = 0;
        constexpr int kColLocalSize = 1;
        constexpr int kColLocalModified = 2;
        constexpr int kColAction = 3;
        constexpr int kColRemotePath = 4;
        constexpr int kColRemoteSize = 5;
        constexpr int kColRemoteModified = 6;
        constexpr int kColCount = 7;
    }

    SyncPreviewPairDetailDialog::SyncPreviewPairDetailDialog(const QString& pairId,
                                                             PairController* controller,
                                                             QWidget* parent) :
        QDialog(parent),
        mPairId(pairId),
        mController(controller),
        mUi(new Ui::SyncPreviewPairDetailDialog)
    {
        mUi->setupUi(this);
        setAttribute(Qt::WA_DeleteOnClose);

        // Non-modal by construction (shown via show(), never exec()): the
        // pair list stays usable while detail windows are open.
        const Pair* pair = mController->pair(mPairId);
        const QString labelText = pair
            ? pair->localPath + QStringLiteral("  <->  ") + pair->remotePath
            : mPairId;
        mUi->pairLabel->setText(labelText);
        QFont boldFont = mUi->pairLabel->font();
        boldFont.setBold(true);
        mUi->pairLabel->setFont(boldFont);

        // Prod theming: standard-components stylesheet + per-theme token
        // replacement, re-applied on live theme changes.
        TokenParserWidgetManager::instance()->registerWidgetForTheming(this);

        connect(mUi->closeButton, &QPushButton::clicked, this, &QDialog::close);
        connect(mUi->pathFilterEdit, &QLineEdit::textChanged, this, [this]() { rebuild(); });
        connect(mUi->showInSyncToggle, &QCheckBox::toggled, this, [this](bool) { rebuild(); });

        connect(mController, &PairController::pairChanged, this, [this](const QString& pairId)
        {
            if (pairId == mPairId)
            {
                rebuild();
            }
        });
        connect(mController, &PairController::pairRemoved, this, [this](const QString& pairId)
        {
            if (pairId == mPairId)
            {
                close();
            }
        });
        // Per-row foregrounds and the tree palette are token colors resolved
        // at populate time: re-resolve when the theme changes so both color
        // schemas read well.
        connect(ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]()
        {
            applyTreePalette();
            rebuild();
        });

        setupColumns();
        applyTreePalette();
        mShownCount = kRowCap;
        rebuild();
    }

    SyncPreviewPairDetailDialog::~SyncPreviewPairDetailDialog() = default;

    void SyncPreviewPairDetailDialog::setupColumns()
    {
        QTreeWidget* tree = mUi->rowsTree;
        tree->setSortingEnabled(false);
        QHeaderView* header = tree->header();
        header->setSectionResizeMode(kColLocalPath, QHeaderView::Stretch);
        header->setSectionResizeMode(kColLocalSize, QHeaderView::Fixed);
        header->setSectionResizeMode(kColLocalModified, QHeaderView::Fixed);
        header->setSectionResizeMode(kColAction, QHeaderView::Interactive);
        header->setSectionResizeMode(kColRemotePath, QHeaderView::Stretch);
        header->setSectionResizeMode(kColRemoteSize, QHeaderView::Fixed);
        header->setSectionResizeMode(kColRemoteModified, QHeaderView::Fixed);
        tree->setColumnWidth(kColLocalSize, 90);
        tree->setColumnWidth(kColLocalModified, 130);
        tree->setColumnWidth(kColAction, 380);
        tree->setColumnWidth(kColRemoteSize, 90);
        tree->setColumnWidth(kColRemoteModified, 130);
    }

    void SyncPreviewPairDetailDialog::applyTreePalette()
    {
        // Theme tokens instead of ad-hoc greys: rows and alternating bands
        // use the app's surface colors, selection uses the app's inverse
        // accent, so both color schemas keep sufficient contrast.
        QPalette palette = mUi->rowsTree->palette();
        auto theme = TokenParserWidgetManager::instance();
        palette.setColor(QPalette::Base, theme->getColor(QLatin1String("page-background")));
        palette.setColor(QPalette::AlternateBase, theme->getColor(QLatin1String("surface-1")));
        palette.setColor(QPalette::Text, theme->getColor(QLatin1String("text-primary")));
        palette.setColor(QPalette::WindowText, theme->getColor(QLatin1String("text-primary")));
        palette.setColor(QPalette::Highlight, theme->getColor(QLatin1String("surface-inverse-accent")));
        palette.setColor(QPalette::HighlightedText, theme->getColor(QLatin1String("text-inverse-accent")));
        mUi->rowsTree->setPalette(palette);
    }

    void SyncPreviewPairDetailDialog::rebuild()
    {
        if (!mController->pair(mPairId))
        {
            // The pair vanished from the queue (also handled via pairRemoved;
            // this guards the race between the signal and the rebuild).
            close();
            return;
        }

        const int scrollPosition = mUi->rowsTree->verticalScrollBar()->value();
        repopulate();
        updateSummary();
        mUi->rowsTree->verticalScrollBar()->setValue(scrollPosition);
    }

    void SyncPreviewPairDetailDialog::repopulate()
    {
        const Classification& classification = mController->classification(mPairId);
        const QHash<QString, RowDecision> decisions = decisionsFor();
        const QStringList reFlagged = mController->reFlaggedPaths(mPairId);
        const Plan plan = mController->plan(mPairId);

        const QString filter = mUi->pathFilterEdit->text();
        const bool showInSync = mUi->showInSyncToggle->isChecked();

        QTreeWidget* tree = mUi->rowsTree;
        tree->clear();

        int displayed = 0;
        int remaining = 0;

        for (const Row& row : classification.rows)
        {
            if (!rowVisible(row, filter) || (!showInSync && row.kind == RowKind::Identical))
            {
                continue;
            }

            if (displayed >= mShownCount)
            {
                ++remaining;
                continue;
            }

            const RowPlan* rowPlan = plan.find(row.relativePath);
            static const RowPlan emptyPlan;
            const bool reFlaggedRow = reFlagged.contains(row.relativePath);

            const QString indent = GuiText::indentFor(row.relativePath);
            const QString badge = rowBadge(row, reFlaggedRow);

            // MC-style: each pane spells the entry as its own side holds it
            // (conflict rows may name it differently per side); a missing
            // side reads as an empty pane.
            QString localPathText = indent;
            if (!badge.isEmpty())
            {
                localPathText += QStringLiteral("[%1]  ").arg(badge);
            }
            localPathText += row.local ? row.local->relativePath : QStringLiteral("—");

            auto* rowItem = new QTreeWidgetItem();
            rowItem->setText(kColLocalPath, localPathText);
            rowItem->setText(kColLocalSize, GuiText::sizeText(row.local));
            rowItem->setText(kColLocalModified, GuiText::timeText(row.local));
            rowItem->setText(kColRemotePath,
                             indent + (row.remote ? row.remote->relativePath : QStringLiteral("—")));
            rowItem->setText(kColRemoteSize, GuiText::sizeText(row.remote));
            rowItem->setText(kColRemoteModified, GuiText::timeText(row.remote));
            rowItem->setToolTip(kColLocalPath, rowTooltip(row));

            applyRowForeground(rowItem, row, rowPlan ? *rowPlan : emptyPlan, reFlaggedRow);

            auto* rowWidget = new SyncPreviewRowWidget(row,
                                                       rowPlan ? *rowPlan : emptyPlan,
                                                       decisions.value(row.relativePath).approved,
                                                       reFlaggedRow,
                                                       tree);
            connect(rowWidget,
                    &SyncPreviewRowWidget::actionSelected,
                    this,
                    &SyncPreviewPairDetailDialog::onRowActionSelected);
            connect(rowWidget,
                    &SyncPreviewRowWidget::approvalToggled,
                    this,
                    &SyncPreviewPairDetailDialog::onRowApprovalToggled);

            tree->addTopLevelItem(rowItem);
            tree->setItemWidget(rowItem, kColAction, rowWidget);

            ++displayed;
        }

        if (remaining > 0)
        {
            QTreeWidgetItem* loadMore = loadMoreItem(remaining);
            tree->addTopLevelItem(loadMore);
            auto* loadMoreButton = new QPushButton(tr("Load more (%1 remaining)").arg(remaining), tree);
            connect(loadMoreButton, &QPushButton::clicked, this, [this]()
            {
                mShownCount += kRowCap;
                rebuild();
            });
            tree->setItemWidget(loadMore, kColLocalPath, loadMoreButton);
        }
    }

    void SyncPreviewPairDetailDialog::applyRowForeground(QTreeWidgetItem* item,
                                                         const Row& row,
                                                         const RowPlan& rowPlan,
                                                         bool reFlagged)
    {
        auto theme = TokenParserWidgetManager::instance();
        QColor color = theme->getColor(QLatin1String("text-primary"));
        if (row.kind == RowKind::Blocker)
        {
            // Blockers cannot be transferred; they must stand out.
            color = theme->getColor(QLatin1String("text-error"));
        }
        else if (reFlagged)
        {
            // Classification changed and approval was cleared: warn.
            color = theme->getColor(QLatin1String("text-warning"));
        }
        else if (row.kind == RowKind::Identical || !rowPlan.coveredByPath.isEmpty())
        {
            // De-emphasize rows that need no attention: identical content
            // and rows whose effective action is inherited from a directory
            // decision above them.
            color = theme->getColor(QLatin1String("text-secondary"));
        }

        for (int column = 0; column < kColCount; ++column)
        {
            item->setForeground(column, QBrush(color));
        }
    }

    QTreeWidgetItem* SyncPreviewPairDetailDialog::loadMoreItem(int remaining)
    {
        auto* item = new QTreeWidgetItem();
        item->setText(kColLocalPath, tr("Load more (%1 remaining)").arg(remaining));
        return item;
    }

    QString SyncPreviewPairDetailDialog::rowBadge(const Row& row, bool reFlagged) const
    {
        QString badge;
        switch (row.kind)
        {
            case RowKind::Identical:
                break;
            case RowKind::LocalOnly:
                break;
            case RowKind::RemoteOnly:
                break;
            case RowKind::BothDiffer:
                badge = QStringLiteral("CONFLICT: both differ");
                break;
            case RowKind::Conflict:
                badge = QStringLiteral("CONFLICT: same content, different name");
                break;
            case RowKind::Blocker:
                badge = (row.blockerReason == BlockerReason::TypeMismatch)
                    ? QStringLiteral("BLOCKER: file vs folder")
                    : QStringLiteral("BLOCKER: case-insensitive name collision");
                break;
        }

        // Newer-side marker: previously a dedicated column, folded into the
        // badge area by the MC-style rework (both modified dates sit next to
        // each other across the middle).
        if (row.local && row.remote && !row.local->isFolder() && !row.remote->isFolder())
        {
            QString newer;
            if (row.local->modifiedTime > row.remote->modifiedTime)
            {
                newer = QStringLiteral("newer: local");
            }
            else if (row.local->modifiedTime < row.remote->modifiedTime)
            {
                newer = QStringLiteral("newer: remote");
            }
            else
            {
                newer = QStringLiteral("same");
            }
            badge += badge.isEmpty() ? QString() : QStringLiteral(" · ");
            badge += newer;
        }

        if (reFlagged)
        {
            badge += badge.isEmpty() ? QString() : QStringLiteral(" · ");
            badge += QStringLiteral("changed — re-approve");
        }
        return badge;
    }

    QString SyncPreviewPairDetailDialog::rowTooltip(const Row& row) const
    {
        QStringList notes;
        if (row.hasIdenticalTwin && !row.twinPath.isEmpty())
        {
            notes << tr("Identical twin: %1").arg(row.twinPath);
        }
        if (row.underBlockedPath)
        {
            notes << tr("Under a blocked path: resolve the blocker above first");
        }
        if (row.kind == RowKind::Blocker)
        {
            notes << tr("Not resolvable by a transfer: needs rename/exclusion (later stage)");
        }
        return notes.join(QLatin1Char('\n'));
    }

    bool SyncPreviewPairDetailDialog::rowVisible(const Row& row, const QString& filter) const
    {
        if (!filter.isEmpty() && !row.relativePath.contains(filter, Qt::CaseInsensitive))
        {
            return false;
        }
        return true;
    }

    QHash<QString, RowDecision> SyncPreviewPairDetailDialog::decisionsFor() const
    {
        QHash<QString, RowDecision> decisions;
        if (const Pair* pair = mController->pair(mPairId))
        {
            for (const RowDecision& decision : pair->decisions)
            {
                decisions.insert(decision.relativePath, decision);
            }
        }
        return decisions;
    }

    void SyncPreviewPairDetailDialog::updateSummary()
    {
        const int awaiting = mController->awaitingApprovalCount(mPairId);
        if (awaiting > 0)
        {
            mUi->summaryLabel->setText(tr("%1 item(s) awaiting approval").arg(awaiting));
        }
        else
        {
            mUi->summaryLabel->setText(tr("all flagged items approved"));
        }
    }

    void SyncPreviewPairDetailDialog::onRowActionSelected(const QString& relativePath, Action action)
    {
        const Classification& classification = mController->classification(mPairId);
        const Row* row = classification.find(relativePath);
        const bool isDirectory = row && ((row->local && row->local->isFolder()) ||
                                         (row->remote && row->remote->isFolder()));

        if (isDirectory)
        {
            // Directory-level actions trigger the consequences popup before
            // anything is applied; canceling restores the previous state.
            const Plan preview = mController->previewPlan(mPairId, relativePath, action);
            const RowPlan* directoryPlan = preview.find(relativePath);
            static const RowPlan emptyPlan;
            SyncPreviewConsequencesDialog popup(relativePath, directoryPlan ? *directoryPlan : emptyPlan, this);
            if (popup.exec() != QDialog::Accepted)
            {
                rebuild();
                return;
            }
        }

        mController->setAction(mPairId, relativePath, action);
    }

    void SyncPreviewPairDetailDialog::onRowApprovalToggled(const QString& relativePath, bool approved)
    {
        mController->setApproved(mPairId, relativePath, approved);
    }
}