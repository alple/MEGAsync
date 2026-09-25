#include "SyncPreviewPairDetailDialog.h"
#include "ui_SyncPreviewPairDetailDialog.h"
#include "SyncPreviewGuiFormat.h"
#include "SyncPreviewGuiStyle.h"
#include "SyncPreviewPairController.h"
#include "SyncPreviewConsequencesDialog.h"

#include "ThemeManager.h"
#include "TokenParserWidgetManager.h"

#include <QBrush>
#include <QButtonGroup>
#include <QCheckBox>
#include <QCoreApplication>
#include <QFont>
#include <QFrame>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPalette>
#include <QPushButton>
#include <QScrollBar>
#include <QSet>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSplitter>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QVector>

#include <algorithm>
#include <functional>

namespace SyncPreview
{
    namespace
    {
        constexpr int kRowCap = 200;

        // Meld-dense rows: the MC-era 40px height was sized for per-row
        // widgets; plain tree rows read well at this height.
        constexpr int kRowHeight = 26;

        constexpr int kColPath = 0;
        constexpr int kColSize = 1;
        constexpr int kColModified = 2;

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

        // Meld mapping of our RowKind, per side: blockers read Blocked on
        // BOTH panes; a side without the entry reads Missing; per-side rows
        // (local-only, remote-only, conflict) read New on the side that has
        // the entry; paired content differs → Modified; identical → Same.
        // Folders paired on both sides read from the classifier's subtree
        // verdict (Identical vs BothDiffer), like meld's directory rows.
        PaneState stateFor(const Row& row, const std::optional<Entry>& entry)
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
        // for Same (meld semantics).
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

        // The action panel's pressable buttons, in a fixed order: the left
        // pane stays on the left and the right pane on the right, so the
        // buttons only flip the arrow (pane directions, not label words).
        const QVector<Action>& actionChoices()
        {
            static const QVector<Action> choices{Action::LocalToRemote, Action::RemoteToLocal,
                Action::BestEffort, Action::None};
            return choices;
        }

        // The button face: just the arrow for the pane-to-pane transfers.
        QString actionButtonText(Action action)
        {
            switch (action)
            {
                case Action::LocalToRemote:
                    return QCoreApplication::translate("SyncPreviewPairDetailDialog", "→");
                case Action::RemoteToLocal:
                    return QCoreApplication::translate("SyncPreviewPairDetailDialog", "←");
                case Action::BestEffort:
                    return QCoreApplication::translate("SyncPreviewPairDetailDialog", "Best-effort");
                case Action::None:
                    return QCoreApplication::translate("SyncPreviewPairDetailDialog", "Do nothing");
            }
            return QStringLiteral("?");
        }

        // The prose form, for hints and tooltips.
        QString actionName(Action action)
        {
            switch (action)
            {
                case Action::LocalToRemote:
                    return QCoreApplication::translate("SyncPreviewPairDetailDialog", "local → remote");
                case Action::RemoteToLocal:
                    return QCoreApplication::translate("SyncPreviewPairDetailDialog", "remote → local");
                case Action::BestEffort:
                    return QCoreApplication::translate("SyncPreviewPairDetailDialog", "best-effort");
                case Action::None:
                    return QCoreApplication::translate("SyncPreviewPairDetailDialog", "do nothing");
            }
            return QStringLiteral("?");
        }

        QString actionTooltip(Action action)
        {
            switch (action)
            {
                case Action::LocalToRemote:
                    return QCoreApplication::translate("SyncPreviewPairDetailDialog", "Transfer local → remote");
                case Action::RemoteToLocal:
                    return QCoreApplication::translate("SyncPreviewPairDetailDialog", "Transfer remote → local");
                case Action::BestEffort:
                    return QCoreApplication::translate("SyncPreviewPairDetailDialog", "Best-effort transfer");
                case Action::None:
                    return QCoreApplication::translate("SyncPreviewPairDetailDialog", "Leave this row unresolved (nothing transfers)");
            }
            return QString();
        }

        // The tree-displayed name: the last path segment.
        QString baseNameOf(const QString& relativePath)
        {
            const int slash = relativePath.lastIndexOf(QLatin1Char('/'));
            return slash < 0 ? relativePath : relativePath.mid(slash + 1);
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
        // The MEGA app icon for the window/taskbar entry (the app-level icon
        // does not reliably reach these non-modal windows under Wayland).
        setWindowIcon(QIcon(QStringLiteral(":/images/app_ico.ico")));

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
        // Row foregrounds, the panes' palettes and the action panel's colors
        // are token colors resolved at populate time: re-resolve when the
        // theme changes so both color schemas read well.
        connect(ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]()
        {
            applyPanesPalette();
            rebuild();
        });

        setupPanes();
        buildActionPanel();
        applyPanesPalette();
        mShownCount = kRowCap;
        rebuild();
    }

    SyncPreviewPairDetailDialog::~SyncPreviewPairDetailDialog() = default;

    void SyncPreviewPairDetailDialog::setupPanes()
    {
        // The panes absorb ALL spare vertical space: the top chrome (title +
        // filter row) and the footer stay at their natural height instead of
        // each grabbing a share of it. A horizontal QSplitter defaults to a
        // non-expanding vertical policy, which is why the chrome used to
        // swallow half the window.
        mUi->panesSplitter->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        if (auto* mainLayout = qobject_cast<QVBoxLayout*>(layout()))
        {
            mainLayout->setStretchFactor(mUi->panesSplitter, 1);
        }

        // The filter row shares the pair list's chrome conventions: themed
        // input (no bright native bar), capped width so the top bar stays
        // compact, and the window's outline close button.
        GuiStyle::styleLineEdit(mUi->pathFilterEdit);
        mUi->pathFilterEdit->setMaximumWidth(360);
        GuiStyle::styleOutlineButton(mUi->closeButton);

        for (QTreeWidget* tree : {mUi->leftTree, mUi->rightTree})
        {
            tree->setMinimumHeight(160);
            tree->setSelectionMode(QAbstractItemView::SingleSelection);
            QHeaderView* header = tree->header();
            header->setSectionResizeMode(kColPath, QHeaderView::Stretch);
            header->setSectionResizeMode(kColSize, QHeaderView::Fixed);
            header->setSectionResizeMode(kColModified, QHeaderView::Fixed);
            tree->setColumnWidth(kColSize, 90);
            tree->setColumnWidth(kColModified, 130);

            // Pane lock-step, meld-style: scrolling, selecting, expanding or
            // collapsing a row moves the same row in the sibling pane.
            connect(tree->verticalScrollBar(), &QScrollBar::valueChanged, this, [this, tree]()
            {
                syncScrollFrom(tree->verticalScrollBar());
            });
            connect(tree, &QTreeWidget::currentItemChanged, this,
                [this, tree](QTreeWidgetItem*, QTreeWidgetItem*)
            {
                if (!mSyncingPanes)
                {
                    syncSelectionFrom(tree);
                }
            });
            connect(tree, &QTreeWidget::itemExpanded, this, [this, tree](QTreeWidgetItem* item)
            {
                if (!mSyncingPanes)
                {
                    syncExpansionFrom(tree, item, true);
                }
            });
            connect(tree, &QTreeWidget::itemCollapsed, this, [this, tree](QTreeWidgetItem* item)
            {
                if (!mSyncingPanes)
                {
                    syncExpansionFrom(tree, item, false);
                }
            });
        }

        QSplitter* splitter = mUi->panesSplitter;
        splitter->setStretchFactor(0, 1);
        splitter->setStretchFactor(1, 1);
    }

    void SyncPreviewPairDetailDialog::buildActionPanel()
    {
        // The decide/approve panel under the trees (MEGA-2.8 verdict: the
        // meld-true panes have no middle strip; the selected row's actions
        // are pressable buttons arranged on labeled lines here). Styled by
        // applyPanesPalette under the syncPreviewActionPanel objectName.
        mActionPanel = new QFrame(this);
        mActionPanel->setObjectName(QLatin1String("syncPreviewActionPanel"));
        mActionPanel->setFrameShape(QFrame::NoFrame);
        mActionPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

        mPanelPathLabel = new QLabel(mActionPanel);
        QFont boldFont = mPanelPathLabel->font();
        boldFont.setBold(true);
        mPanelPathLabel->setFont(boldFont);
        mPanelPathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

        mPanelStatesLabel = new QLabel(mActionPanel);
        mPanelStatesLabel->setTextFormat(Qt::RichText);

        // One pressable button per action, exclusive: the pressed button is
        // the row's effective action; the recommendation is a side hint.
        mActionGroup = new QButtonGroup(mActionPanel);
        mActionGroup->setExclusive(true);
        for (const Action choice : actionChoices())
        {
            auto* button = new QPushButton(actionButtonText(choice), mActionPanel);
            button->setCheckable(true);
            // Prod widget design: the app's themed components are keyed on
            // the type="mega" property (same convention as MEGA-2.7's rows).
            button->setProperty("type", QLatin1String("mega"));
            mActionGroup->addButton(button);
            mActionButtons.push_back(button);
            connect(button, &QPushButton::clicked, this, [this, choice](bool checked)
            {
                if (!checked || mSelectedPath.isEmpty())
                {
                    // Unchecking happens programmatically when another
                    // button takes over; only a real pick decides.
                    return;
                }
                const Row* row = mRowsByPath.value(mSelectedPath);
                const Plan plan = mController->plan(mPairId);
                const RowPlan* rowPlan = plan.find(mSelectedPath);
                const Action effective = rowPlan ? rowPlan->action : (row ? row->recommendedAction : Action::None);
                if (choice == effective)
                {
                    return; // re-click on the active action: no-op
                }
                onRowActionSelected(mSelectedPath, choice);
            });
        }

        mApproveButton = new QPushButton(tr("Approve"), mActionPanel);
        mApproveButton->setCheckable(true);
        mApproveButton->setProperty("type", QLatin1String("mega"));
        connect(mApproveButton, &QPushButton::clicked, this, [this](bool checked)
        {
            if (!mSelectedPath.isEmpty())
            {
                onRowApprovalToggled(mSelectedPath, checked);
            }
        });

        mPanelHintLabel = new QLabel(mActionPanel);

        mPanelNotesLabel = new QLabel(mActionPanel);
        mPanelNotesLabel->setWordWrap(true);

        auto* headerRow = new QHBoxLayout;
        headerRow->addWidget(mPanelPathLabel, 1);
        headerRow->addWidget(mPanelStatesLabel);

        auto* decisionsRow = new QHBoxLayout;
        decisionsRow->setSpacing(6);
        for (QPushButton* button : mActionButtons)
        {
            decisionsRow->addWidget(button);
        }
        decisionsRow->addWidget(mApproveButton);
        decisionsRow->addStretch(1);
        decisionsRow->addWidget(mPanelHintLabel);

        auto* panelLayout = new QVBoxLayout(mActionPanel);
        panelLayout->setContentsMargins(8, 6, 8, 6);
        panelLayout->setSpacing(4);
        panelLayout->addLayout(headerRow);
        panelLayout->addLayout(decisionsRow);
        panelLayout->addWidget(mPanelNotesLabel);

        // Between the panes and the footer, taking only its natural height.
        auto* mainLayout = qobject_cast<QVBoxLayout*>(layout());
        Q_ASSERT(mainLayout);
        mainLayout->insertWidget(mainLayout->indexOf(mUi->panesSplitter) + 1, mActionPanel);
    }

    void SyncPreviewPairDetailDialog::applyPanesPalette()
    {
        // Theme tokens instead of ad-hoc greys: the whole window sits on the
        // page background (a lighter default surface read as a grey band
        // above the panes), trees and their column headers use the same
        // background, selection uses the app's inverse accent, and the
        // action panel is a quiet surface card — so both color schemas keep
        // a calm, uniform dark/light read.
        auto theme = TokenParserWidgetManager::instance();
        const QColor surface1 = theme->getColor(QLatin1String("surface-1"));
        const QColor borderStrong = theme->getColor(QLatin1String("border-strong"));

        GuiStyle::applyWindowPalette(this);

        for (QTreeWidget* tree : {mUi->leftTree, mUi->rightTree})
        {
            GuiStyle::applyViewPalette(tree);
        }

        if (mActionPanel)
        {
            // The decide/approve panel: one quiet surface card, no frames.
            mActionPanel->setStyleSheet(QStringLiteral(
                "QFrame#syncPreviewActionPanel {"
                " background: %1;"
                " border: 1px solid %2;"
                " border-radius: 6px;"
                " }")
                .arg(surface1.name(), borderStrong.name()));

            for (QPushButton* button : mActionButtons)
            {
                button->setStyleSheet(GuiStyle::actionButtonStyleSheet());
            }
            mApproveButton->setStyleSheet(GuiStyle::approveButtonStyleSheet());
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
        updateActionPanel();
        updateSummary();
    }

    void SyncPreviewPairDetailDialog::repopulate()
    {
        const Classification& classification = mController->classification(mPairId);

        const QString filter = mUi->pathFilterEdit->text();
        // Meld-style state filters (MEGA-2.8 AC#3): Same hidden by default.
        const bool showSame = mUi->sameFilterCheck->isChecked();
        const bool showDifferent = mUi->differentFilterCheck->isChecked();
        const bool showNew = mUi->newFilterCheck->isChecked();

        // Path-keyed row index and parent→children grouping for the tree
        // walk; the classification holds every path (folders included) with
        // its own row, so the tree and the rows agree one to one.
        mRowsByPath.clear();
        QHash<QString, QVector<const Row*>> childrenOf;
        for (const Row& row : classification.rows)
        {
            mRowsByPath.insert(row.relativePath, &row);
            childrenOf[parentPath(row.relativePath)].push_back(&row);
        }
        for (auto it = childrenOf.begin(); it != childrenOf.end(); ++it)
        {
            std::sort(it->begin(), it->end(),
                [](const Row* a, const Row* b)
                {
                    return baseNameOf(a->relativePath).compare(baseNameOf(b->relativePath),
                               Qt::CaseInsensitive) < 0;
                });
        }

        // Row visibility: path filter + state bucket; a row the state
        // filters would hide still shows as a structural ancestor when
        // visible descendants hang under it (meld keeps such folders on
        // screen so the tree stays connected).
        QSet<QString> visiblePaths;
        for (const Row& row : classification.rows)
        {
            if (rowVisible(row, filter) && visibleUnderFilters(row, showSame, showDifferent, showNew))
            {
                for (QString path = row.relativePath; !path.isEmpty(); path = parentPath(path))
                {
                    if (visiblePaths.contains(path))
                    {
                        break; // this ancestor chain is already marked
                    }
                    visiblePaths.insert(path);
                }
            }
        }

        QTreeWidget* leftTree = mUi->leftTree;
        QTreeWidget* rightTree = mUi->rightTree;

        const int scrollPosition = leftTree->verticalScrollBar()->value();
        leftTree->clear();
        rightTree->clear();
        mLeftItems.clear();
        mRightItems.clear();

        int displayed = 0;
        mSyncingPanes = true;

        // One depth-first walk builds both panes in lock-step: same paths,
        // same order, expansion state preserved across rebuilds.
        std::function<void(QTreeWidgetItem*, QTreeWidgetItem*, const QString&)> addLevel =
            [&](QTreeWidgetItem* leftParent, QTreeWidgetItem* rightParent, const QString& parentKey)
        {
            const auto siblings = childrenOf.constFind(parentKey);
            if (siblings == childrenOf.constEnd())
            {
                return;
            }
            for (const Row* row : siblings.value())
            {
                if (!visiblePaths.contains(row->relativePath))
                {
                    continue;
                }
                if (displayed >= mShownCount)
                {
                    continue; // counted as remaining below
                }
                QTreeWidgetItem* leftItem = makeSideItem(leftTree, leftParent, *row, true);
                QTreeWidgetItem* rightItem = makeSideItem(rightTree, rightParent, *row, false);
                mLeftItems.insert(row->relativePath, leftItem);
                mRightItems.insert(row->relativePath, rightItem);
                ++displayed;
                addLevel(leftItem, rightItem, row->relativePath);
            }
        };
        addLevel(nullptr, nullptr, QString());

        // Load-more cap (MEGA-2.7): visible rows beyond the cap collapse
        // into a button row appended under BOTH panes, keeping the panes
        // line-locked.
        const int remaining = visiblePaths.size() - displayed;
        if (remaining > 0)
        {
            QTreeWidgetItem* leftItem = loadMoreItem(remaining);
            leftTree->addTopLevelItem(leftItem);
            leftTree->setFirstItemColumnSpanned(leftItem, true);
            auto* loadMoreButton =
                new QPushButton(tr("Load more (%1 remaining)").arg(remaining), leftTree);
            connect(loadMoreButton, &QPushButton::clicked, this, [this]()
            {
                mShownCount += kRowCap;
                rebuild();
            });
            leftTree->setItemWidget(leftItem, kColPath, loadMoreButton);

            QTreeWidgetItem* rightItem = loadMoreItem(remaining);
            rightTree->addTopLevelItem(rightItem);
        }
        mSyncingPanes = false;

        // Restore selection and scroll position across both panes.
        if (!mSelectedPath.isEmpty())
        {
            const auto leftIt = mLeftItems.constFind(mSelectedPath);
            const auto rightIt = mRightItems.constFind(mSelectedPath);
            if (leftIt != mLeftItems.constEnd() && rightIt != mRightItems.constEnd())
            {
                mUi->leftTree->setCurrentItem(leftIt.value());
                mUi->rightTree->setCurrentItem(rightIt.value());
            }
            else
            {
                // The selected row was filtered away; the panel resets.
                mSelectedPath.clear();
            }
        }
        leftTree->verticalScrollBar()->setValue(scrollPosition);
    }

    QTreeWidgetItem* SyncPreviewPairDetailDialog::makeSideItem(QTreeWidget* tree,
                                                               QTreeWidgetItem* parent,
                                                               const Row& row,
                                                               bool localSide)
    {
        const std::optional<Entry>& side = localSide ? row.local : row.remote;
        const PaneState state = stateFor(row, side);

        auto* item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(tree);
        item->setData(kColPath, Qt::UserRole, row.relativePath);
        item->setSizeHint(kColPath, QSize(0, kRowHeight));

        // Each pane spells the entry as its own side holds it (the two
        // spellings of a case-collision blocker differ); a missing side
        // keeps the canonical spelling, struck through.
        const QString name = side ? baseNameOf(side->relativePath) : baseNameOf(row.relativePath);
        item->setText(kColPath, name);
        item->setText(kColSize, GuiText::sizeText(side));
        item->setText(kColModified, GuiText::timeText(side));

        const QString tooltip = rowTooltip(row);
        if (!tooltip.isEmpty())
        {
            item->setToolTip(kColPath, tooltip);
        }

        const QColor color = stateColor(state, TokenParserWidgetManager::instance().get());
        for (int column = 0; column < tree->columnCount(); ++column)
        {
            item->setForeground(column, QBrush(color));
        }
        item->setFont(kColPath, stateFont(state, tree->font()));

        // Icons on BOTH panes, including a missing side: the row's entry
        // type (whichever side holds it) is what the other side would sync,
        // so both panes read as mirrored file trees — a local-only folder
        // shows as a struck folder name on the remote side, not as a blank.
        bool isFolder = false;
        if (side)
        {
            isFolder = side->isFolder();
        }
        else if (row.local)
        {
            isFolder = row.local->isFolder();
        }
        else if (row.remote)
        {
            isFolder = row.remote->isFolder();
        }
        if (isFolder)
        {
            QIcon icon;
            icon.addFile(QStringLiteral(":/") + kFolderIcon);
            icon.addFile(QStringLiteral(":/") + kFolderIcon2x);
            item->setIcon(kColPath, icon);
        }
        else if (side)
        {
            item->setIcon(kColPath, QIcon(QStringLiteral(":/") + kFileIcon));
        }

        if (side && side->isFolder())
        {
            item->setExpanded(mExpandedByPath.value(row.relativePath, true));
        }
        return item;
    }

    void SyncPreviewPairDetailDialog::syncScrollFrom(QScrollBar* source)
    {
        if (mSyncingPanes)
        {
            return;
        }

        mSyncingPanes = true;
        const int value = source->value();
        for (QTreeWidget* tree : {mUi->leftTree, mUi->rightTree})
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
        mSelectedPath = current ? current->data(kColPath, Qt::UserRole).toString() : QString();

        mSyncingPanes = true;
        for (QTreeWidget* tree : {mUi->leftTree, mUi->rightTree})
        {
            if (tree != source)
            {
                QTreeWidgetItem* mirror = nullptr;
                if (!mSelectedPath.isEmpty())
                {
                    const auto& items = (tree == mUi->leftTree) ? mLeftItems : mRightItems;
                    const auto it = items.constFind(mSelectedPath);
                    if (it != items.constEnd())
                    {
                        mirror = it.value();
                    }
                }
                const QSignalBlocker blocker(tree);
                tree->setCurrentItem(mirror);
            }
        }
        mSyncingPanes = false;

        updateActionPanel();
    }

    void SyncPreviewPairDetailDialog::syncExpansionFrom(QTreeWidget* source,
                                                        QTreeWidgetItem* item,
                                                        bool expanded)
    {
        const QString path = item->data(kColPath, Qt::UserRole).toString();
        if (path.isEmpty())
        {
            return;
        }
        mExpandedByPath.insert(path, expanded);

        mSyncingPanes = true;
        for (QTreeWidget* tree : {mUi->leftTree, mUi->rightTree})
        {
            if (tree != source)
            {
                const auto& items = (tree == mUi->leftTree) ? mLeftItems : mRightItems;
                const auto it = items.constFind(path);
                if (it != items.constEnd())
                {
                    it.value()->setExpanded(expanded);
                }
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

    void SyncPreviewPairDetailDialog::updateActionPanel()
    {
        auto theme = TokenParserWidgetManager::instance();
        const Row* row = mRowsByPath.value(mSelectedPath);
        if (!row)
        {
            mPanelPathLabel->setText(tr("Select a row to decide"));
            mPanelPathLabel->setToolTip(QString());
            mPanelStatesLabel->clear();
            for (QPushButton* button : mActionButtons)
            {
                const QSignalBlocker blocker(button);
                button->setChecked(false);
                button->setEnabled(false);
            }
            mApproveButton->setVisible(false);
            mPanelHintLabel->clear();
            mPanelNotesLabel->clear();
            mPanelNotesLabel->setVisible(false);
            return;
        }

        mPanelPathLabel->setText(row->relativePath);
        mPanelPathLabel->setToolTip(row->relativePath);

        // The two panes' states, spelled out with their state colors.
        const PaneState localState = stateFor(*row, row->local);
        const PaneState remoteState = stateFor(*row, row->remote);
        mPanelStatesLabel->setText(QStringLiteral(
            "<span style=\"color:%2;\">local: %1</span> · <span style=\"color:%4;\">remote: %3</span>")
            .arg(stateText(localState), stateColor(localState, theme.get()).name(),
                 stateText(remoteState), stateColor(remoteState, theme.get()).name()));

        // The effective action (decisions already folded in by the planner)
        // is the pressed button; the recommendation is a side hint.
        const Plan plan = mController->plan(mPairId);
        const RowPlan* rowPlan = plan.find(row->relativePath);
        const Action effective = rowPlan ? rowPlan->action : row->recommendedAction;
        const QVector<Action>& choices = actionChoices();
        // Blocker rows cannot transfer (a transfer would stall on them):
        // only "Do nothing" stays clickable there, with the rename/exclusion
        // resolution spelled out in the notes. Transfer buttons stay visible
        // but disabled — readable, with the later-stage explanation.
        const bool transferable = row->kind != RowKind::Blocker;
        for (int i = 0; i < mActionButtons.size() && i < choices.size(); ++i)
        {
            const QSignalBlocker blocker(mActionButtons[i]);
            const Action choice = choices.at(i);
            mActionButtons[i]->setEnabled(transferable || choice == Action::None);
            mActionButtons[i]->setChecked(choice == effective);
            QString tip = actionTooltip(choice);
            if (choice == row->recommendedAction)
            {
                tip += QStringLiteral(" — ") + tr("recommended");
            }
            if (!transferable && choice != Action::None)
            {
                tip += QStringLiteral(" — ") + tr("arrives in a later stage");
            }
            mActionButtons[i]->setToolTip(tip);
        }
        mPanelHintLabel->setText(tr("Recommended: %1").arg(actionName(row->recommendedAction)));
        mPanelHintLabel->setStyleSheet(
            QStringLiteral("QLabel { color: %1; }")
                .arg(theme->getColor(QLatin1String("text-secondary")).name()));

        // Approval only on rows the classifier flagged for it, and only when
        // something actually transfers: approving a "do nothing" row is a
        // no-op and would just read as noise.
        const QHash<QString, RowDecision> decisions = decisionsFor();
        mApproveButton->setVisible(row->requiresApproval && effective != Action::None);
        {
            const QSignalBlocker blocker(mApproveButton);
            mApproveButton->setEnabled(row->kind != RowKind::Blocker || row->requiresApproval);
            mApproveButton->setChecked(decisions.value(row->relativePath).approved);
        }

        // Notes: conflict/blocker reasons, twin, blocked path, re-flag.
        QStringList notes;
        const QStringList reFlagged = mController->reFlaggedPaths(mPairId);
        if (reFlagged.contains(row->relativePath))
        {
            notes << tr("classification changed — re-approve");
        }
        const QString tooltipNotes = rowTooltip(*row);
        if (!tooltipNotes.isEmpty())
        {
            notes << tooltipNotes;
        }
        const bool severe = row->kind == RowKind::Blocker || row->kind == RowKind::Conflict ||
            row->underBlockedPath;
        mPanelNotesLabel->setText(notes.join(QStringLiteral("  ·  ")));
        mPanelNotesLabel->setStyleSheet(
            QStringLiteral("QLabel { color: %1; }")
                .arg(theme->getColor(QLatin1String(severe ? "text-error" : "text-warning")).name()));
        mPanelNotesLabel->setVisible(!notes.isEmpty());
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
