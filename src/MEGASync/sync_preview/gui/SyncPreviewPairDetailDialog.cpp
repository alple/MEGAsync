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
#include <QCoreApplication>
#include <QFont>
#include <QHeaderView>
#include <QIcon>
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

        // Fixed width of the per-pane meld-state column; it holds the state
        // word plus the compact "newer: <side>" suffix. The remaining width
        // goes to the stretching path column.
        constexpr int kStatusWidth = 140;

        constexpr int kColPath = 0;
        constexpr int kColStatus = 1;
        constexpr int kColSize = 2;
        constexpr int kColModified = 3;

        // Entry-type icons, existing app resources only, no new image assets
        // (MEGA-2.8 AC#4). The @2x folder variant is registered for HiDPI.
        const QLatin1String kFolderIcon("images/node_selector/search_filter/small_folder_default.png");
        const QLatin1String kFolderIcon2x("images/node_selector/search_filter/small_folder_default@2x.png");
        const QLatin1String kFileIcon("images/themed/common/MIME/generic_small.svg");

        // Meld folder-diff state, per pane per side (MEGA-2.8): one row can
        // read New in the local pane and Missing in the remote pane.
        enum class PaneState
        {
            Same,
            Modified,
            New,
            Missing,
            Blocked
        };

        // How one side of one classification row reads in its pane.
        struct PaneRender
        {
            PaneState state = PaneState::Same;
            QString pathText;
            QString statusText;
            bool hasEntry = false;
            bool isFolder = false;
        };

        const Entry* sideEntry(const Row& row, bool localSide)
        {
            if (localSide)
            {
                return row.local ? &*row.local : nullptr;
            }
            return row.remote ? &*row.remote : nullptr;
        }

        // Meld mapping of our RowKind, per side: blockers read Blocked on
        // BOTH panes; a side without the entry reads Missing; per-side rows
        // (local-only, remote-only, conflict) read New on the side that has
        // the entry; paired content differs → Modified; identical → Same.
        PaneState stateFor(const Row& row, const Entry* entry)
        {
            if (row.kind == RowKind::Blocker)
            {
                return PaneState::Blocked;
            }
            if (!entry)
            {
                return PaneState::Missing;
            }
            switch (row.kind)
            {
                case RowKind::Identical:
                    return PaneState::Same;
                case RowKind::BothDiffer:
                    return PaneState::Modified;
                case RowKind::LocalOnly:
                case RowKind::RemoteOnly:
                case RowKind::Conflict:
                    return PaneState::New;
                case RowKind::Blocker:
                    break; // handled above
            }
            return PaneState::Same;
        }

        // Theme tokens only (MEGA-2.8 AC#2): Modified = text-info (blue),
        // New = text-success (green), Blocked = text-error (bright red),
        // Same/Missing = text-secondary (de-emphasized/gray).
        QColor stateColor(PaneState state, TokenParserWidgetManager* theme)
        {
            switch (state)
            {
                case PaneState::Modified:
                    return theme->getColor(QLatin1String("text-info"));
                case PaneState::New:
                    return theme->getColor(QLatin1String("text-success"));
                case PaneState::Blocked:
                    return theme->getColor(QLatin1String("text-error"));
                case PaneState::Same:
                case PaneState::Missing:
                    return theme->getColor(QLatin1String("text-secondary"));
            }
            return theme->getColor(QLatin1String("text-primary"));
        }

        // Bold for New/Modified/Blocked, strikethrough for Missing, plain
        // for Same (meld semantics). Applied to the path and status columns;
        // size and modified take the state color but keep a plain font.
        QFont stateFont(PaneState state, const QFont& base)
        {
            QFont font = base;
            switch (state)
            {
                case PaneState::New:
                case PaneState::Modified:
                case PaneState::Blocked:
                    font.setBold(true);
                    break;
                case PaneState::Missing:
                    font.setStrikeOut(true);
                    break;
                case PaneState::Same:
                    break;
            }
            return font;
        }

        QString stateText(PaneState state)
        {
            switch (state)
            {
                case PaneState::Same:
                    return QCoreApplication::translate("SyncPreviewPairDetailDialog", "Same");
                case PaneState::Modified:
                    return QCoreApplication::translate("SyncPreviewPairDetailDialog", "Modified");
                case PaneState::New:
                    return QCoreApplication::translate("SyncPreviewPairDetailDialog", "New");
                case PaneState::Missing:
                    return QCoreApplication::translate("SyncPreviewPairDetailDialog", "Missing");
                case PaneState::Blocked:
                    return QCoreApplication::translate("SyncPreviewPairDetailDialog", "Blocked");
            }
            return QString();
        }

        // Newer-side marker as a compact status suffix (MEGA-2.8): rows with
        // a file on both sides only; equal timestamps say it themselves.
        QString newerSuffix(const Row& row)
        {
            if (!(row.local && row.remote) || row.local->isFolder() || row.remote->isFolder())
            {
                return QString();
            }
            if (row.local->modifiedTime > row.remote->modifiedTime)
            {
                return QCoreApplication::translate("SyncPreviewPairDetailDialog", "newer: local");
            }
            if (row.local->modifiedTime < row.remote->modifiedTime)
            {
                return QCoreApplication::translate("SyncPreviewPairDetailDialog", "newer: remote");
            }
            return QString();
        }

        // Meld-style state filters (MEGA-2.8 AC#3): a row is visible when
        // its bucket is checked — Same = identical, Different = both-differ,
        // New = the entry exists on one side only (conflict rows included:
        // each side's name exists on one side only, exactly what meld's New
        // filter matches). Blocker rows stay visible regardless.
        bool visibleUnderFilters(const Row& row, bool showSame, bool showDifferent, bool showNew)
        {
            if (row.kind == RowKind::Blocker)
            {
                return true;
            }
            switch (row.kind)
            {
                case RowKind::Identical:
                    return showSame;
                case RowKind::BothDiffer:
                    return showDifferent;
                case RowKind::LocalOnly:
                case RowKind::RemoteOnly:
                case RowKind::Conflict:
                    return showNew;
                case RowKind::Blocker:
                    return true; // handled above
            }
            return true;
        }

        PaneRender renderSide(const Row& row, bool localSide, const QString& indent, const QString& newer)
        {
            const Entry* entry = sideEntry(row, localSide);
            PaneRender render;
            render.state = stateFor(row, entry);
            render.hasEntry = entry != nullptr;
            render.isFolder = entry && entry->isFolder();
            render.pathText = indent + (entry ? entry->relativePath : QStringLiteral("—"));
            render.statusText = stateText(render.state);
            if (!newer.isEmpty())
            {
                render.statusText += QStringLiteral(" · ") + newer;
            }
            return render;
        }

        // One side pane's row for a classified path: state color on every
        // column, bold/strike font on path + status, entry-type icon from
        // existing app resources (MEGA-2.8 AC#2/#4).
        QTreeWidgetItem* addSideRow(QTreeWidget* tree,
                                    const PaneRender& render,
                                    const QString& sizeText,
                                    const QString& timeText,
                                    const QString& tooltip,
                                    TokenParserWidgetManager* theme)
        {
            auto* item = new QTreeWidgetItem();
            // Uniform row height: the panes lay out identical rows, which is
            // what keeps the three views line-locked.
            item->setSizeHint(kColPath, QSize(0, kRowHeight));
            item->setText(kColPath, render.pathText);
            item->setText(kColStatus, render.statusText);
            item->setText(kColSize, sizeText);
            item->setText(kColModified, timeText);
            if (!tooltip.isEmpty())
            {
                item->setToolTip(kColPath, tooltip);
                item->setToolTip(kColStatus, tooltip);
            }
            const QColor color = stateColor(render.state, theme);
            const QFont stateFontFor = stateFont(render.state, tree->font());
            for (int column = 0; column < tree->columnCount(); ++column)
            {
                item->setForeground(column, QBrush(color));
                if (column == kColPath || column == kColStatus)
                {
                    item->setFont(column, stateFontFor);
                }
            }
            if (render.hasEntry)
            {
                QIcon icon;
                if (render.isFolder)
                {
                    icon.addFile(QStringLiteral(":/") + kFolderIcon);
                    icon.addFile(QStringLiteral(":/") + kFolderIcon2x);
                }
                else
                {
                    icon.addFile(QStringLiteral(":/") + kFileIcon);
                }
                item->setIcon(kColPath, icon);
            }
            tree->addTopLevelItem(item);
            return item;
        }
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
        // Meld-style state filters (MEGA-2.8 AC#3): Same unchecked by
        // default, so identical rows stay hidden unless asked for.
        connect(mUi->sameFilterCheck, &QCheckBox::toggled, this, [this](bool) { rebuild(); });
        connect(mUi->differentFilterCheck, &QCheckBox::toggled, this, [this](bool) { rebuild(); });
        connect(mUi->newFilterCheck, &QCheckBox::toggled, this, [this](bool) { rebuild(); });

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
            header->setSectionResizeMode(kColStatus, QHeaderView::Fixed);
            header->setSectionResizeMode(kColSize, QHeaderView::Fixed);
            header->setSectionResizeMode(kColModified, QHeaderView::Fixed);
            tree->setColumnWidth(kColStatus, kStatusWidth);
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
        // Meld-style state filters (MEGA-2.8 AC#3): Same hidden by default.
        const bool showSame = mUi->sameFilterCheck->isChecked();
        const bool showDifferent = mUi->differentFilterCheck->isChecked();
        const bool showNew = mUi->newFilterCheck->isChecked();

        auto theme = TokenParserWidgetManager::instance();
        const QColor primaryColor = theme->getColor(QLatin1String("text-primary"));

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
            if (!rowVisible(row, filter) ||
                !visibleUnderFilters(row, showSame, showDifferent, showNew))
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
            const QString newer = newerSuffix(row);

            // Per-side meld states (MEGA-2.8 AC#1): one row reads New on the
            // side that holds the entry and Missing on the other, blockers
            // read Blocked on both. The state explains the row; the conflict
            // and blocker details live in the tooltips, the re-approve
            // warning rides the action strip's label.
            const PaneRender localRender = renderSide(row, true, indent, newer);
            const PaneRender remoteRender = renderSide(row, false, indent, newer);

            const QString tooltip = rowTooltip(row);

            addSideRow(leftTree, localRender, GuiText::sizeText(row.local),
                       GuiText::timeText(row.local), tooltip, theme.get());
            addSideRow(rightTree, remoteRender, GuiText::sizeText(row.remote),
                       GuiText::timeText(row.remote), tooltip, theme.get());

            // The action strip row: same index as both panes.
            auto* midItem = new QTreeWidgetItem();
            midItem->setSizeHint(0, QSize(kMidWidth, kRowHeight));
            midItem->setForeground(0, QBrush(primaryColor));
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

            PaneRender loadMoreRender;
            loadMoreRender.pathText = tr("Load more (%1 remaining)").arg(remaining);
            addSideRow(midTree, loadMoreRender, QString(), QString(), QString(), theme.get());
            addSideRow(rightTree, loadMoreRender, QString(), QString(), QString(), theme.get());
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

    QTreeWidgetItem* SyncPreviewPairDetailDialog::loadMoreItem(int remaining)
    {
        auto* item = new QTreeWidgetItem();
        item->setSizeHint(kColPath, QSize(0, kRowHeight));
        item->setText(kColPath, tr("Load more (%1 remaining)").arg(remaining));
        return item;
    }

    QString SyncPreviewPairDetailDialog::rowTooltip(const Row& row) const
    {
        QStringList notes;
        if (row.hasIdenticalTwin && !row.twinPath.isEmpty())
        {
            notes << tr("Identical twin: %1").arg(row.twinPath);
        }
        if (row.kind == RowKind::Conflict)
        {
            // Was the badge text (MEGA-2.7); the meld rework spells the
            // state in the status column and keeps the detail here.
            notes << tr("Same content, different name — needs approval");
        }
        if (row.underBlockedPath)
        {
            notes << tr("Under a blocked path: resolve the blocker above first");
        }
        if (row.kind == RowKind::Blocker)
        {
            notes << (row.blockerReason == BlockerReason::TypeMismatch
                ? tr("Blocker: file vs folder at the same path")
                : tr("Blocker: case-insensitive name collision"));
            notes << tr("Not resolvable by a transfer: needs rename/exclusion (later stage)");
        }
        return notes.join(QLatin1Char('\n'));
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