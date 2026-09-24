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
#include <QSplitter>
#include <QTreeWidget>
#include <QTreeWidgetItem>

namespace SyncPreview
{
    namespace
    {
        constexpr int kRowCap = 200;

        // Uniform row height shared by all three panes: the row lock-step
        // (scroll/selection sync) relies on every view laying out the same
        // rows at the same heights.
        constexpr int kRowHeight = 40;

        // Fixed width of the action strip between the two panes.
        constexpr int kMidWidth = 380;

        constexpr int kColPath = 0;
        constexpr int kColSize = 1;
        constexpr int kColModified = 2;
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
        // Per-row foregrounds and the panes' palettes are token colors
        // resolved at populate time: re-resolve when the theme changes so
        // both color schemas read well.
        connect(ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]()
        {
            applyPanesPalette();
            rebuild();
        });

        setupPanes();
        applyPanesPalette();
        mShownCount = kRowCap;
        rebuild();
    }

    SyncPreviewPairDetailDialog::~SyncPreviewPairDetailDialog() = default;

    void SyncPreviewPairDetailDialog::setupPanes()
    {
        QTreeWidget* midTree = mUi->midTree;
        midTree->setSelectionMode(QAbstractItemView::NoSelection);
        midTree->setFocusPolicy(Qt::NoFocus);
        midTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
        midTree->setMinimumWidth(kMidWidth);
        midTree->setMaximumWidth(kMidWidth);

        for (QTreeWidget* tree : {mUi->leftTree, mUi->rightTree})
        {
            tree->setSelectionMode(QAbstractItemView::SingleSelection);
            QHeaderView* header = tree->header();
            header->setSectionResizeMode(kColPath, QHeaderView::Stretch);
            header->setSectionResizeMode(kColSize, QHeaderView::Fixed);
            header->setSectionResizeMode(kColModified, QHeaderView::Fixed);
            tree->setColumnWidth(kColSize, 90);
            tree->setColumnWidth(kColModified, 130);
        }

        QSplitter* splitter = mUi->panesSplitter;
        splitter->setStretchFactor(0, 1);
        splitter->setStretchFactor(1, 0);
        splitter->setStretchFactor(2, 1);

        // Row lock-step: scrolling one pane scrolls all three; selecting a
        // row in a pane selects the same row in the sibling pane.
        for (QTreeWidget* tree : {mUi->leftTree, mUi->midTree, mUi->rightTree})
        {
            connect(tree->verticalScrollBar(), &QScrollBar::valueChanged, this, [this, tree]()
            {
                syncScrollFrom(tree->verticalScrollBar());
            });
        }
        connect(mUi->leftTree, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem*, QTreeWidgetItem*)
        {
            if (!mSyncingPanes)
            {
                syncSelectionFrom(mUi->leftTree);
            }
        });
        connect(mUi->rightTree, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem*, QTreeWidgetItem*)
        {
            if (!mSyncingPanes)
            {
                syncSelectionFrom(mUi->rightTree);
            }
        });
    }

    void SyncPreviewPairDetailDialog::applyPanesPalette()
    {
        // Theme tokens instead of ad-hoc greys: rows and alternating bands
        // use the app's surface colors, selection uses the app's inverse
        // accent, so both color schemas keep sufficient contrast.
        auto theme = TokenParserWidgetManager::instance();
        for (QTreeWidget* tree : {mUi->leftTree, mUi->midTree, mUi->rightTree})
        {
            QPalette palette = tree->palette();
            palette.setColor(QPalette::Base, theme->getColor(QLatin1String("page-background")));
            palette.setColor(QPalette::AlternateBase, theme->getColor(QLatin1String("surface-1")));
            palette.setColor(QPalette::Text, theme->getColor(QLatin1String("text-primary")));
            palette.setColor(QPalette::WindowText, theme->getColor(QLatin1String("text-primary")));
            palette.setColor(QPalette::Highlight, theme->getColor(QLatin1String("surface-inverse-accent")));
            palette.setColor(QPalette::HighlightedText, theme->getColor(QLatin1String("text-inverse-accent")));
            tree->setPalette(palette);
        }
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

        repopulate();
        updateSummary();
    }

    void SyncPreviewPairDetailDialog::repopulate()
    {
        const Classification& classification = mController->classification(mPairId);
        const QHash<QString, RowDecision> decisions = decisionsFor();
        const QStringList reFlagged = mController->reFlaggedPaths(mPairId);
        const Plan plan = mController->plan(mPairId);

        const QString filter = mUi->pathFilterEdit->text();
        const bool showInSync = mUi->showInSyncToggle->isChecked();

        auto theme = TokenParserWidgetManager::instance();
        const QColor normalColor = theme->getColor(QLatin1String("text-primary"));
        const QColor secondaryColor = theme->getColor(QLatin1String("text-secondary"));
        const QColor errorColor = theme->getColor(QLatin1String("text-error"));
        const QColor warningColor = theme->getColor(QLatin1String("text-warning"));

        QTreeWidget* leftTree = mUi->leftTree;
        QTreeWidget* midTree = mUi->midTree;
        QTreeWidget* rightTree = mUi->rightTree;

        const int scrollPosition = leftTree->verticalScrollBar()->value();
        leftTree->clear();
        midTree->clear();
        rightTree->clear();

        static const RowPlan emptyPlan;

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
            const bool reFlaggedRow = reFlagged.contains(row.relativePath);

            const QString indent = GuiText::indentFor(row.relativePath);
            const QString badge = rowBadge(row, reFlaggedRow);

            // Each pane spells the entry as its own side holds it (conflict
            // rows may name it differently per side); a missing side reads
            // as an empty pane cell.
            QString localPathText = indent;
            if (!badge.isEmpty())
            {
                localPathText += QStringLiteral("[%1]  ").arg(badge);
            }
            localPathText += row.local ? row.local->relativePath : QStringLiteral("—");
            const QString remotePathText =
                indent + (row.remote ? row.remote->relativePath : QStringLiteral("—"));

            // One shared color per row across all panes: blockers must
            // stand out, re-flags warn, identical/covered rows de-emphasize.
            QColor color = normalColor;
            if (row.kind == RowKind::Blocker)
            {
                color = errorColor;
            }
            else if (reFlaggedRow)
            {
                color = warningColor;
            }
            else if (row.kind == RowKind::Identical ||
                     (rowPlan && !rowPlan->coveredByPath.isEmpty()))
            {
                color = secondaryColor;
            }

            const QString tooltip = rowTooltip(row);

            addRowToTree(leftTree, localPathText, GuiText::sizeText(row.local),
                         GuiText::timeText(row.local), tooltip, color);
            addRowToTree(rightTree, remotePathText, GuiText::sizeText(row.remote),
                         GuiText::timeText(row.remote), tooltip, color);

            // The action strip row: same index as both panes.
            auto* midItem = new QTreeWidgetItem();
            midItem->setSizeHint(0, QSize(kMidWidth, kRowHeight));
            midItem->setForeground(0, QBrush(color));
            midTree->addTopLevelItem(midItem);
            midTree->setItemWidget(midItem, 0,
                                   buildRowWidget(row, rowPlan ? *rowPlan : emptyPlan,
                                                  decisions.value(row.relativePath).approved,
                                                  reFlaggedRow, midTree));

            ++displayed;
        }

        if (remaining > 0)
        {
            QTreeWidgetItem* leftItem = loadMoreItem(remaining);
            leftTree->addTopLevelItem(leftItem);
            leftTree->setFirstItemColumnSpanned(leftItem, true);
            auto* loadMoreButton = new QPushButton(tr("Load more (%1 remaining)").arg(remaining), leftTree);
            connect(loadMoreButton, &QPushButton::clicked, this, [this]()
            {
                mShownCount += kRowCap;
                rebuild();
            });
            leftTree->setItemWidget(leftItem, kColPath, loadMoreButton);

            addRowToTree(midTree, tr("Load more (%1 remaining)").arg(remaining),
                         QString(), QString(), QString(), normalColor);
            addRowToTree(rightTree, tr("Load more (%1 remaining)").arg(remaining),
                         QString(), QString(), QString(), normalColor);
        }

        // Restore the scroll position across all panes.
        leftTree->verticalScrollBar()->setValue(scrollPosition);
    }

    void SyncPreviewPairDetailDialog::syncScrollFrom(QScrollBar* source)
    {
        if (mSyncingPanes)
        {
            return;
        }

        mSyncingPanes = true;
        const int value = source->value();
        for (QTreeWidget* tree : {mUi->leftTree, mUi->midTree, mUi->rightTree})
        {
            if (tree->verticalScrollBar() != source)
            {
                tree->verticalScrollBar()->setValue(value);
            }
        }
        mSyncingPanes = false;
    }

    void SyncPreviewPairDetailDialog::syncSelectionFrom(QTreeWidget* source)
    {
        QTreeWidgetItem* current = source->currentItem();
        if (!current)
        {
            return;
        }

        mSyncingPanes = true;
        const int row = source->indexOfTopLevelItem(current);
        for (QTreeWidget* tree : {mUi->leftTree, mUi->midTree, mUi->rightTree})
        {
            if (tree != source)
            {
                const QSignalBlocker blocker(tree);
                tree->setCurrentItem(tree->topLevelItem(row));
            }
        }
        mSyncingPanes = false;
    }

    QTreeWidgetItem* SyncPreviewPairDetailDialog::addRowToTree(QTreeWidget* tree,
                                                               const QString& pathText,
                                                               const QString& sizeText,
                                                               const QString& timeText,
                                                               const QString& tooltip,
                                                               const QColor& color)
    {
        auto* item = new QTreeWidgetItem();
        // Uniform row height: the panes lay out identical rows, which is
        // what keeps the three views line-locked.
        item->setSizeHint(kColPath, QSize(0, kRowHeight));
        item->setText(kColPath, pathText);
        item->setText(kColSize, sizeText);
        item->setText(kColModified, timeText);
        if (!tooltip.isEmpty())
        {
            item->setToolTip(kColPath, tooltip);
        }
        for (int column = 0; column < tree->columnCount(); ++column)
        {
            item->setForeground(column, QBrush(color));
        }
        tree->addTopLevelItem(item);
        return item;
    }

    QWidget* SyncPreviewPairDetailDialog::buildRowWidget(const Row& row,
                                                         const RowPlan& rowPlan,
                                                         bool approved,
                                                         bool reFlagged,
                                                         QWidget* parent)
    {
        auto* rowWidget = new SyncPreviewRowWidget(row, rowPlan, approved, reFlagged, parent);
        connect(rowWidget,
                &SyncPreviewRowWidget::actionSelected,
                this,
                &SyncPreviewPairDetailDialog::onRowActionSelected);
        connect(rowWidget,
                &SyncPreviewRowWidget::approvalToggled,
                this,
                &SyncPreviewPairDetailDialog::onRowApprovalToggled);
        return rowWidget;
    }

    QTreeWidgetItem* SyncPreviewPairDetailDialog::loadMoreItem(int remaining)
    {
        auto* item = new QTreeWidgetItem();
        item->setSizeHint(kColPath, QSize(0, kRowHeight));
        item->setText(kColPath, tr("Load more (%1 remaining)").arg(remaining));
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