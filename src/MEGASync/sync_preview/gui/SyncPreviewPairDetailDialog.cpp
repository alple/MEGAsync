#include "SyncPreviewPairDetailDialog.h"
#include "ui_SyncPreviewPairDetailDialog.h"
#include "SyncPreviewChangesDialog.h"
#include "SyncPreviewGuiFormat.h"
#include "SyncPreviewGuiStyle.h"
#include "SyncPreviewPairController.h"

#include "ThemeManager.h"
#include "TokenParserWidgetManager.h"

#include <QBrush>
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
#include <QTimer>
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

        // The decision column's pinned width: three compact arrows plus the
        // 2px cell margins, no slack.
        constexpr int kDecisionColumnWidth = 96;

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

        // The action panel's pressable buttons (MEGA-2.11 AC#8): the three
        // transfer arrows. "Do nothing" is no longer a button — none clicked
        // reads as do nothing — and approval is folded into the click.
        const QVector<Action>& actionChoices()
        {
            static const QVector<Action> choices{Action::LocalToRemote, Action::RemoteToLocal,
                Action::BestEffort};
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
                    return QCoreApplication::translate("SyncPreviewPairDetailDialog", "↔");
                case Action::None:
                    break;
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
                    return QCoreApplication::translate("SyncPreviewPairDetailDialog",
                        "Make remote like local (upload/overwrite; remote-only entries are removed from MEGA, recoverable). Clicking decides and approves in one step; click again to un-decide");
                case Action::RemoteToLocal:
                    return QCoreApplication::translate("SyncPreviewPairDetailDialog",
                        "Make local like remote (download/overwrite; local-only entries are removed locally, recoverable). Clicking decides and approves in one step; click again to un-decide");
                case Action::BestEffort:
                    return QCoreApplication::translate("SyncPreviewPairDetailDialog",
                        "Best-effort both-way merge (missing side gets the file; same-name-differ conflicts). Clicking decides and approves in one step; click again to un-decide");
                case Action::None:
                    break;
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

        // The state filters are prod checkboxes (MEGA-2.12): without the
        // type="mega" property the standard sheet's themed indicator images
        // never apply and the native fallback washes out on the page
        // background (barely visible in the light schema).
        for (QCheckBox* filterCheck :
             {mUi->sameFilterCheck, mUi->differentFilterCheck, mUi->newFilterCheck})
        {
            filterCheck->setProperty("type", QLatin1String("mega"));
        }

        // The directory action's consequences note (MEGA-2.12): one footer
        // line beside the summary, hidden until a directory action lands.
        mDirectoryNoteLabel = new QLabel(this);
        mDirectoryNoteLabel->setTextFormat(Qt::PlainText);
        mDirectoryNoteLabel->setVisible(false);
        mUi->footerLayout->insertWidget(0, mDirectoryNoteLabel, 1);

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
        // Review-loop surface (MEGA-2.9): the scheduled-changes list
        // (with its Apply step inside the popup, MEGA-2.11) and the
        // pane-view normalizer (MEGA-2.10), the latter now a checkable
        // toggle (MEGA-2.11 AC#6): ON = continuous panes lock-step,
        // OFF = each pane browses freely. Default ON.
        GuiStyle::styleOutlineButton(mUi->showChangesButton);
        GuiStyle::styleOutlineButton(mUi->synchronizeViewButton);
        mUi->synchronizeViewButton->setCheckable(true);
        mUi->synchronizeViewButton->setChecked(true);
        mUi->synchronizeViewButton->setToolTip(tr("Synchronize view — when checked, both panes expand, select and scroll in lock-step; uncheck to browse each pane independently"));
        connect(mUi->showChangesButton, &QPushButton::clicked, this, [this]() { showChanges(); });
        connect(mUi->synchronizeViewButton, &QPushButton::toggled, this, [this](bool locked)
        {
            mPanesLocked = locked;
            if (locked)
            {
                // Re-locking re-normalizes: the left pane's per-path
                // expansion is mirrored onto the right, and selection and
                // scroll re-align (the left pane is the reference) so the
                // panes read as one view again.
                synchronizeView();
                syncSelectionFrom(mUi->leftTree);
                syncScrollFrom(mUi->leftTree->verticalScrollBar());
            }
        });

        // Common per-tree setup (panes + decision column): the rows are
        // line-locked across all three trees, so zebra, row heights and the
        // scroll/expansion mirrors treat them equally.
        for (QTreeWidget* tree : {mUi->leftTree, mUi->decisionTree, mUi->rightTree})
        {
            tree->setMinimumHeight(160);
            tree->setUniformRowHeights(false);
            // Zebra striping (MEGA-2.11 AC#4): the trees are line-locked
            // (same rows, same order), so the alternating tint — the same
            // surface token on all — makes corresponding rows trackable
            // across the panes and the decision column.
            tree->setAlternatingRowColors(true);
            // EXPLICIT uniform row height on all three trees (MEGA-2.12
            // round 4): left to font metrics, the panes' rows and the
            // decision column's widget-pinned rows drift apart by a couple
            // of pixels and the stripes desynchronize. A stylesheet item
            // height makes every row in every tree exactly this tall.
            tree->setStyleSheet(
                QStringLiteral("QTreeWidget::item { height: %1px; }").arg(kRowHeight));
            // Pane lock-step, meld-style: scrolling or expanding/collapsing a
            // row moves the same row in the sibling views.
            connect(tree->verticalScrollBar(), &QScrollBar::valueChanged, this, [this, tree]()
            {
                syncScrollFrom(tree->verticalScrollBar());
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

        // The panes: single selection, and the current row drives the action
        // panel (the decision column never takes selection — its per-row
        // buttons work without one, MEGA-2.12).
        for (QTreeWidget* tree : {mUi->leftTree, mUi->rightTree})
        {
            tree->setSelectionMode(QAbstractItemView::SingleSelection);
            QHeaderView* header = tree->header();
            header->setSectionResizeMode(kColPath, QHeaderView::Stretch);
            header->setSectionResizeMode(kColSize, QHeaderView::Fixed);
            header->setSectionResizeMode(kColModified, QHeaderView::Fixed);
            tree->setColumnWidth(kColSize, 90);
            tree->setColumnWidth(kColModified, 130);
            connect(tree, &QTreeWidget::currentItemChanged, this,
                [this, tree](QTreeWidgetItem*, QTreeWidgetItem*)
            {
                if (!mSyncingPanes)
                {
                    syncSelectionFrom(tree);
                }
            });
        }

        // The decision column (MEGA-2.12): a narrow strip whose rows carry
        // the three compact arrow buttons; no selection, no expansion
        // chrome — its expansion follows the panes via the mirrors above.
        // No indentation (setIndentation(0)): the arrows start at the
        // cell's left edge. The width is pinned by the splitter below — a
        // plain setFixedWidth loses to the splitter's initial size
        // distribution (the column started wide and snapped lean only after
        // a manual resize, round-3 feedback).
        QTreeWidget* decisionTree = mUi->decisionTree;
        decisionTree->setFocusPolicy(Qt::NoFocus);
        decisionTree->header()->setSectionResizeMode(kColPath, QHeaderView::Stretch);
        decisionTree->setIndentation(0);
        decisionTree->setMinimumWidth(kDecisionColumnWidth);
        decisionTree->setMaximumWidth(kDecisionColumnWidth);
        // The blank header strip (MEGA-2.12 round 5): the panes show their
        // column headers, so their first row starts one header height below
        // the tree's top — with this header HIDDEN the column's rows and
        // stripes sat one header height (~19px) above the panes' rows and
        // never lined up. A blank, inert header keeps all three viewports'
        // rows starting at the same y by construction; its height is synced
        // to the panes' headers (syncDecisionHeaderHeight) after the first
        // layout pass and on theme changes.
        decisionTree->headerItem()->setText(kColPath, QString());
        QHeaderView* decisionHeader = decisionTree->header();
        decisionHeader->setSectionsClickable(false);
        decisionHeader->setSectionsMovable(false);

        QSplitter* splitter = mUi->panesSplitter;
        splitter->setStretchFactor(0, 1);
        splitter->setStretchFactor(1, 0);
        splitter->setStretchFactor(2, 1);
        // Pin the initial distribution too: stretch factors alone leave the
        // middle column at the tree's content-driven width on first show.
        // singleShot(0) runs after the dialog's first layout pass.
        QTimer::singleShot(0, this, [this]()
        {
            mUi->panesSplitter->setSizes({1, kDecisionColumnWidth, 1});
            // Header heights are final after the first layout pass: pin the
            // decision column's blank header to the panes' header height so
            // the three viewports' rows start at the same y (round 5).
            syncDecisionHeaderHeight();
        });
    }

    void SyncPreviewPairDetailDialog::syncDecisionHeaderHeight()
    {
        // The decision column's blank header must be exactly as tall as the
        // panes' headers, or its rows (and stripes) start offset from the
        // panes' rows (MEGA-2.12 round 5).
        mUi->decisionTree->header()->setFixedHeight(mUi->leftTree->header()->height());
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

        // The pressable buttons (MEGA-2.11 AC#8): three checkable arrows in
        // a fixed order — the left pane stays on the left and the right pane
        // on the right, so the buttons only flip the arrow (pane directions,
        // not label words). Clicking decides AND approves in one gesture;
        // clicking the active button again un-decides to an explicit
        // do-nothing.
        const QVector<Action>& choices = actionChoices();
        for (const Action choice : choices)
        {
            auto* button = new QPushButton(actionButtonText(choice), mActionPanel);
            button->setCheckable(true);
            // Prod widget design: the app's themed components are keyed on
            // the type="mega" property (same convention as MEGA-2.7's rows).
            button->setProperty("type", QLatin1String("mega"));
            mActionButtons.push_back(button);
            connect(button, &QPushButton::clicked, this, [this, choice](bool checked)
            {
                onActionButtonClicked(mSelectedPath, choice, checked);
            });
        }

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

        for (QTreeWidget* tree : {mUi->leftTree, mUi->decisionTree, mUi->rightTree})
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
        }

        // The decision column's per-row arrows (MEGA-2.12): the compact
        // variant of the same quiet sheet, re-resolved on theme change.
        for (auto it = mMidButtonsByPath.begin(); it != mMidButtonsByPath.end(); ++it)
        {
            for (QPushButton* button : it.value())
            {
                button->setStyleSheet(GuiStyle::actionButtonStyleSheet(true));
            }
        }

        // Filter-row and footer chrome (MEGA-2.10): the action panel's
        // token-resolved quiet sheet instead of the property-outline look,
        // which rendered unreadable (dark on dark) for the reviewer —
        // re-resolved on theme change like the rest of the window.
        const QString chromeSheet = GuiStyle::actionButtonStyleSheet();
        for (QPushButton* chrome : {mUi->closeButton, mUi->showChangesButton, mUi->synchronizeViewButton})
        {
            chrome->setStyleSheet(chromeSheet);
        }

        // Filter-row and footer text (MEGA-2.11 AC#5): the label/checkbox
        // colors the app stylesheet leaves dark-on-dark in this window are
        // pinned to the token colors, re-resolved on theme change like the
        // buttons above.
        const QString textPrimarySheet =
            QStringLiteral("color: %1;").arg(theme->getColor(QLatin1String("text-primary")).name());
        for (QWidget* widget : {mUi->pairLabel, mUi->filterLabel, mUi->summaryLabel})
        {
            widget->setStyleSheet(textPrimarySheet);
        }
        for (QCheckBox* filterCheck : {mUi->sameFilterCheck, mUi->differentFilterCheck, mUi->newFilterCheck})
        {
            filterCheck->setStyleSheet(textPrimarySheet);
        }

        // The state legend (MEGA-2.10), top-right beside the pair label:
        // colored dots over the same tokens the panes use, re-resolved on
        // every theme change.
        auto themeInstance = TokenParserWidgetManager::instance();
        const QColor info = themeInstance->getColor(QLatin1String("text-info"));
        const QColor success = themeInstance->getColor(QLatin1String("text-success"));
        const QColor error = themeInstance->getColor(QLatin1String("text-error"));
        const QColor secondary = themeInstance->getColor(QLatin1String("text-secondary"));
        mUi->legendLabel->setText(QStringLiteral(
            "<span style=\"color:%1;\">●</span> %5 &nbsp;&nbsp; "
            "<span style=\"color:%2;\">●</span> %6 &nbsp;&nbsp; "
            "<span style=\"color:%3;\">●</span> %7 &nbsp;&nbsp; "
            "<span style=\"color:%4;\">●</span> %8 &nbsp;&nbsp; "
            "<span style=\"color:%4; text-decoration: line-through;\">●</span> %9")
            .arg(info.name(), success.name(), error.name(), secondary.name(),
                 stateText(PaneState::Modified), stateText(PaneState::New),
                 stateText(PaneState::Blocked), stateText(PaneState::Same),
                 stateText(PaneState::Missing)));

        // Theme changes can re-size the headers (fonts differ per schema):
        // keep the decision column's blank header pinned to the panes'
        // header height so the stripes stay aligned (MEGA-2.12 round 5).
        syncDecisionHeaderHeight();
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
        updateDecisionColumn();
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
        QTreeWidget* midTree = mUi->decisionTree;

        // Scroll positions survive the repopulation: with Synchronize view
        // ON the panes follow the left pane's position (lock-step); with it
        // OFF each pane keeps its own (MEGA-2.11 AC#6). The decision column
        // has no free state — it always follows the left pane.
        const int leftScroll = leftTree->verticalScrollBar()->value();
        const int rightScroll = rightTree->verticalScrollBar()->value();
        leftTree->clear();
        rightTree->clear();
        midTree->clear();
        mLeftItems.clear();
        mRightItems.clear();
        mMidItems.clear();
        mMidButtonsByPath.clear();

        int displayed = 0;
        mSyncingPanes = true;

        // One depth-first walk builds all three views in lock-step: same
        // paths, same order, expansion state preserved across rebuilds.
        std::function<void(QTreeWidgetItem*, QTreeWidgetItem*, QTreeWidgetItem*, const QString&)> addLevel =
            [&](QTreeWidgetItem* leftParent, QTreeWidgetItem* rightParent,
                QTreeWidgetItem* midParent, const QString& parentKey)
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
                QTreeWidgetItem* midItem = makeDecisionItem(*row, midParent);
                mLeftItems.insert(row->relativePath, leftItem);
                mRightItems.insert(row->relativePath, rightItem);
                mMidItems.insert(row->relativePath, midItem);
                ++displayed;
                addLevel(leftItem, rightItem, midItem, row->relativePath);
            }
        };
        addLevel(nullptr, nullptr, nullptr, QString());

        // Load-more cap (MEGA-2.7): visible rows beyond the cap collapse
        // into a button row appended under BOTH panes, keeping the views
        // line-locked (the decision column gets a blank row).
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

            QTreeWidgetItem* midItem = loadMoreItem(remaining);
            midTree->addTopLevelItem(midItem);
            midTree->setFirstItemColumnSpanned(midItem, true);
        }
        mSyncingPanes = false;

        // Restore the per-pane selections and scroll positions. With the
        // Synchronize view ON both panes follow the left pane's position;
        // with it OFF each pane keeps its own (MEGA-2.11 AC#6).
        const auto restoreSelection = [this](QTreeWidget* tree, const QString& path)
        {
            if (path.isEmpty())
            {
                return;
            }
            const auto& items = (tree == mUi->leftTree) ? mLeftItems : mRightItems;
            const auto it = items.constFind(path);
            if (it != items.constEnd())
            {
                tree->setCurrentItem(it.value());
            }
        };

        if (!mSelectedPath.isEmpty() &&
            mLeftItems.constFind(mSelectedPath) == mLeftItems.constEnd() &&
            mRightItems.constFind(mSelectedPath) == mRightItems.constEnd())
        {
            // The selected row was filtered away; the panel resets.
            mSelectedPath.clear();
            mLeftSelectedPath.clear();
            mRightSelectedPath.clear();
        }
        // The restore's own currentItemChanged signals re-derive
        // mSelectedPath from the last-restored pane; keep the row the user
        // actually interacted with as the panel's row.
        const QString activeSelection = mSelectedPath;
        restoreSelection(mUi->leftTree, mLeftSelectedPath);
        restoreSelection(mUi->rightTree, mRightSelectedPath);
        mSelectedPath = activeSelection;
        leftTree->verticalScrollBar()->setValue(leftScroll);
        // The decision column has no free state: it always follows the left
        // pane's scroll (MEGA-2.12).
        midTree->verticalScrollBar()->setValue(leftScroll);
        if (mPanesLocked)
        {
            rightTree->verticalScrollBar()->setValue(leftScroll);
        }
        else
        {
            rightTree->verticalScrollBar()->setValue(rightScroll);
        }
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

        // Icons and the expansion affordance on BOTH panes, keyed on the
        // ROW's kind, not the side's entry (MEGA-2.11 AC#3): a folder row
        // reads as a folder left and right, so the panes mirror each other
        // even for a missing side (struck name) or a type mismatch (file on
        // one side, folder on the other — the mismatching side keeps its
        // struck/blocked read and the row tooltip explains). A local-only
        // folder shows as a struck folder name on the remote side, not as
        // a blank; the same row never reads file-icon on one pane and
        // folder-icon on the other.
        const bool rowIsFolder = (row.local && row.local->isFolder()) ||
            (row.remote && row.remote->isFolder());
        if (rowIsFolder)
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

        // Expansion on BOTH panes for every folder row (MEGA-2.10): a
        // folder missing on one side keeps its struck-through name but
        // still expands/collapses in lock-step — the reviewer sees the
        // same tree shape left and right. With Synchronize view OFF the
        // right pane reads its own expansion map (MEGA-2.11 AC#6).
        if (rowIsFolder)
        {
            const QHash<QString, bool>& expansionMap =
                (localSide || mPanesLocked) ? mExpandedByPath : mRightExpandedByPath;
            item->setExpanded(expansionMap.value(row.relativePath, true));
        }
        return item;
    }

    QTreeWidgetItem* SyncPreviewPairDetailDialog::makeDecisionItem(const Row& row,
                                                                   QTreeWidgetItem* parent)
    {
        // The decision column's row (MEGA-2.12): no text of its own — just
        // the three compact arrow buttons in a row widget, sharing the
        // bottom panel's decide+approve gesture per row. Root rows get the
        // same extra spacing as the panes so the views stay line-locked.
        auto* item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(mUi->decisionTree);
        item->setData(kColPath, Qt::UserRole, row.relativePath);

        auto* buttonsRow = new QWidget(mUi->decisionTree);
        buttonsRow->setStyleSheet(QStringLiteral("background: transparent;"));
        auto* layout = new QHBoxLayout(buttonsRow);
        layout->setContentsMargins(2, 2, 2, 2);
        layout->setSpacing(2);

        const Classification& classification = mController->classification(mPairId);
        const Row* liveRow = classification.find(row.relativePath);
        const bool isBlocker = liveRow && liveRow->kind == RowKind::Blocker;

        QVector<QPushButton*> buttons;
        const QVector<Action>& choices = actionChoices();
        for (const Action choice : choices)
        {
            auto* button = new QPushButton(actionButtonText(choice), buttonsRow);
            button->setCheckable(true);
            button->setFocusPolicy(Qt::NoFocus);
            button->setProperty("type", QLatin1String("mega"));
            button->setStyleSheet(GuiStyle::actionButtonStyleSheet(true));
            // Only the visually active button can toggle off on click; the
            // un-decide gesture is routed through the panel's shared path.
            connect(button, &QPushButton::clicked, this,
                [this, path = row.relativePath, choice](bool checked)
            {
                onActionButtonClicked(path, choice, checked);
            });
            layout->addWidget(button);
            buttons.push_back(button);
        }
        // Best-effort stays out on blocker rows (tester-approved exception).
        if (isBlocker)
        {
            buttons.last()->setEnabled(false);
        }
        mMidButtonsByPath.insert(row.relativePath, buttons);

        buttonsRow->setLayout(layout);
        // The row widget is pinned to the SAME enforced item height the
        // stylesheet gives every tree's rows (setupPanes): without the pin
        // the widget sizes itself and the middle tree's rows drift from the
        // panes', desynchronizing the stripes (MEGA-2.12 round 4).
        buttonsRow->setFixedHeight(kRowHeight);
        item->setSizeHint(kColPath, QSize(0, kRowHeight));
        mUi->decisionTree->setItemWidget(item, kColPath, buttonsRow);
        return item;
    }

    QString SyncPreviewPairDetailDialog::decisionButtonTooltip(const Row& row, Action choice,
                                                               bool decidable) const
    {
        QString tip = actionTooltip(choice);
        if (choice == row.recommendedAction)
        {
            tip += QStringLiteral(" — ") + tr("recommended");
        }
        if (row.kind == RowKind::Blocker &&
            (choice == Action::LocalToRemote || choice == Action::RemoteToLocal))
        {
            tip += QStringLiteral(" — ") + tr("displaces the conflicting entry with an automatic rename");
        }
        if (!decidable)
        {
            tip += QStringLiteral(" — ") + tr("a merge cannot resolve a blocked row");
        }
        return tip;
    }

    void SyncPreviewPairDetailDialog::syncScrollFrom(QScrollBar* source)
    {
        if (mSyncingPanes)
        {
            return;
        }

        // The decision column has no free state (MEGA-2.12): it always
        // follows the left pane's scroll. When the signal comes FROM the
        // decision column itself (transient, while it is being re-aligned),
        // treat it as the left pane's position.
        if (source == mUi->decisionTree->verticalScrollBar())
        {
            source = mUi->leftTree->verticalScrollBar();
        }

        mSyncingPanes = true;
        const int value = source->value();
        for (QTreeWidget* tree : {mUi->leftTree, mUi->decisionTree, mUi->rightTree})
        {
            QScrollBar* bar = tree->verticalScrollBar();
            if (bar == source)
            {
                continue;
            }
            // Panes follow each other only when locked; the decision column
            // follows the left pane unconditionally.
            if (mPanesLocked || tree == mUi->decisionTree || source == mUi->leftTree->verticalScrollBar())
            {
                bar->setValue(value);
            }
        }
        mSyncingPanes = false;
    }

    void SyncPreviewPairDetailDialog::syncSelectionFrom(QTreeWidget* source)
    {
        QTreeWidgetItem* current = source->currentItem();
        const QString path = current ? current->data(kColPath, Qt::UserRole).toString() : QString();
        mSelectedPath = path;
        if (source == mUi->leftTree)
        {
            mLeftSelectedPath = path;
        }
        else
        {
            mRightSelectedPath = path;
        }

        // The selection is mirrored between the PANES when the Synchronize
        // view toggle is ON (the panes read as one view); with it OFF the
        // sibling pane keeps its own selection (MEGA-2.11 AC#6). The
        // decision column takes no selection (MEGA-2.12).
        if (mPanesLocked)
        {
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
        }

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

        if (!mPanesLocked && source != mUi->decisionTree)
        {
            // Free browsing (MEGA-2.11 AC#6): record into the source pane's
            // own map so a rebuild restores exactly this pane's shape; the
            // sibling pane keeps its own. The decision column has no free
            // state (MEGA-2.12) — its expansion always follows the shared map.
            if (source == mUi->leftTree)
            {
                mExpandedByPath.insert(path, expanded);
            }
            else
            {
                mRightExpandedByPath.insert(path, expanded);
            }
            return;
        }

        // Locked: one shared reference map, mirrored onto the sibling views
        // (mSyncingPanes suppresses the mirror's own signals).
        mExpandedByPath.insert(path, expanded);

        mSyncingPanes = true;
        for (QTreeWidget* tree : {mUi->leftTree, mUi->decisionTree, mUi->rightTree})
        {
            if (tree != source)
            {
                const auto& items = (tree == mUi->leftTree) ? mLeftItems
                    : (tree == mUi->rightTree) ? mRightItems : mMidItems;
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
        if (!row.localTwinPath.isEmpty())
        {
            notes << tr("Local twin of the remote content: %1").arg(row.localTwinPath);
        }
        if (row.kind == RowKind::Conflict)
        {
            notes << tr("Same content, different name — needs approval");
            notes << tr("An arrow action transfers by renaming the identical twin to this name, without a duplicate");
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
            notes << tr("An arrow action transfers with an automatic rename; “do nothing” leaves it blocked");
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

        // Checked state (MEGA-2.11 AC#8): an own decision lights its button
        // (an explicit do-nothing lights none); a flagged row without an own
        // decision lights NONE — its approval must come from a click, even
        // when the plan would inherit or recommend an action; an unflagged
        // row lights its effective action (own decision aside: inherited or
        // the recommendation), because "clicked = what will happen".
        const Plan plan = mController->plan(mPairId);
        const RowPlan* rowPlan = plan.find(row->relativePath);
        const Action effective = rowPlan ? rowPlan->action : row->recommendedAction;
        const ActionSource effectiveSource = rowPlan ? rowPlan->actionSource : ActionSource::Recommended;
        const QString effectiveSourcePath = rowPlan ? rowPlan->decisionSourcePath : QString();

        const QHash<QString, RowDecision> decisions = decisionsFor();
        const bool hasOwnDecision = decisions.contains(row->relativePath);
        const Action ownAction = hasOwnDecision ? decisions.value(row->relativePath).action : Action::None;
        const Action checkedAction = hasOwnDecision ? ownAction
            : (row->requiresApproval ? Action::None : effective);

        // Rename-aware decidability (MEGA-2.9): the arrow actions decide on
        // conflict and blocker rows alike — they mean "transfer with an
        // automatic rename". On blocker rows only best-effort stays out
        // (a merge cannot fix a structural collision); "do nothing" is the
        // unclicked state.
        const QVector<Action>& choices = actionChoices();
        const bool isBlocker = row->kind == RowKind::Blocker;
        for (int i = 0; i < mActionButtons.size() && i < choices.size(); ++i)
        {
            const QSignalBlocker blocker(mActionButtons[i]);
            const Action choice = choices.at(i);
            const bool decidable = !isBlocker || choice != Action::BestEffort;
            mActionButtons[i]->setEnabled(decidable);
            mActionButtons[i]->setChecked(choice == checkedAction);
            mActionButtons[i]->setToolTip(decisionButtonTooltip(*row, choice, decidable));
        }

        // The description right of the buttons: the clicked action's prose;
        // when nothing is clicked, what the row still waits for.
        QString hintText;
        const char* hintToken = "text-secondary";
        if (hasOwnDecision && ownAction != Action::None)
        {
            hintText = actionName(ownAction);
            hintToken = "text-primary";
        }
        else if (hasOwnDecision)
        {
            hintText = tr("do nothing — nothing transfers");
        }
        else if (row->requiresApproval)
        {
            hintText = row->recommendedAction != Action::None
                ? tr("Needs your decision — recommended: %1").arg(actionName(row->recommendedAction))
                : tr("Needs your decision");
            hintToken = "text-warning";
        }
        else if (effectiveSource == ActionSource::InheritedFromDirectory)
        {
            hintText = tr("%1 — from directory %2").arg(actionName(effective), effectiveSourcePath);
        }
        else
        {
            hintText = tr("Recommended: %1").arg(actionName(effective));
        }
        mPanelHintLabel->setText(hintText);
        mPanelHintLabel->setStyleSheet(
            QStringLiteral("QLabel { color: %1; }")
                .arg(theme->getColor(QLatin1String(hintToken)).name()));

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
        // A decided blocker spells out its automatic rename in the plan.
        if (row->kind == RowKind::Blocker && rowPlan && !rowPlan->operations.isEmpty())
        {
            notes << rowPlan->warnings;
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
        renderDirectoryNote();
    }

    void SyncPreviewPairDetailDialog::updateDecisionColumn()
    {
        // Re-derive every decision-column row's checked/enabled state from
        // the current decisions (MEGA-2.12): the same state function the
        // bottom panel's buttons use, applied per row without selection.
        const QHash<QString, RowDecision> decisions = decisionsFor();
        const Plan plan = mController->plan(mPairId);

        for (auto it = mMidButtonsByPath.begin(); it != mMidButtonsByPath.end(); ++it)
        {
            const Row* row = mRowsByPath.value(it.key());
            if (!row)
            {
                continue;
            }

            const RowPlan* rowPlan = plan.find(row->relativePath);
            const Action effective = rowPlan ? rowPlan->action : row->recommendedAction;
            const bool hasOwnDecision = decisions.contains(row->relativePath);
            const Action ownAction =
                hasOwnDecision ? decisions.value(row->relativePath).action : Action::None;
            const Action checkedAction = hasOwnDecision ? ownAction
                : (row->requiresApproval ? Action::None : effective);

            const bool isBlocker = row->kind == RowKind::Blocker;
            const QVector<Action>& choices = actionChoices();
            const QVector<QPushButton*>& buttons = it.value();
            for (int i = 0; i < buttons.size() && i < choices.size(); ++i)
            {
                const Action choice = choices.at(i);
                const bool decidable = !isBlocker || choice != Action::BestEffort;
                const QSignalBlocker blocker(buttons[i]);
                buttons[i]->setEnabled(decidable);
                buttons[i]->setChecked(choice == checkedAction);
                buttons[i]->setToolTip(decisionButtonTooltip(*row, choice, decidable));
            }
        }
    }

    void SyncPreviewPairDetailDialog::setDirectoryNote(const QString& directoryPath,
                                                       const RowPlan* directoryPlan)
    {
        mDirectoryNotePath = directoryPath;
        mDirectoryNoteLine.clear();
        mDirectoryNoteTooltip.clear();

        if (!directoryPlan || directoryPath.isEmpty())
        {
            renderDirectoryNote();
            return;
        }

        // One line, the tester-approved shape (MEGA-2.12): the affected
        // counts; the full per-path breakdown rides in the tooltip. Warning
        // coloring when anything is removed or overwritten.
        QStringList pieces;
        QStringList tooltipLines;
        const auto addSection = [&pieces, &tooltipLines](const QString& label,
                                                         const QStringList& paths)
        {
            if (paths.isEmpty())
            {
                return;
            }
            pieces << tr("%1 %2").arg(paths.size()).arg(label);
            for (const QString& path : paths)
            {
                tooltipLines << QStringLiteral("%1: %2").arg(label, path);
            }
        };
        addSection(tr("removed locally"), directoryPlan->removedLocal);
        addSection(tr("overwritten locally"), directoryPlan->changedLocal);
        addSection(tr("created locally"), directoryPlan->createdLocal);
        addSection(tr("removed from MEGA"), directoryPlan->removedRemote);
        addSection(tr("overwritten on MEGA"), directoryPlan->changedRemote);
        addSection(tr("created on MEGA"), directoryPlan->createdRemote);
        for (const QString& warning : directoryPlan->warnings)
        {
            tooltipLines << warning;
        }

        if (pieces.isEmpty())
        {
            mDirectoryNoteLine = tr("Directory action on %1: nothing under it is affected.")
                                     .arg(directoryPath);
        }
        else
        {
            mDirectoryNoteLine = tr("Directory action on %1: %2.")
                                     .arg(directoryPath, pieces.join(QStringLiteral(", ")));
        }
        mDirectoryNoteTooltip = tooltipLines.join(QLatin1Char('\n'));
        renderDirectoryNote();
    }

    void SyncPreviewPairDetailDialog::renderDirectoryNote()
    {
        if (!mDirectoryNoteLabel)
        {
            return;
        }
        const bool hasNote = !mDirectoryNoteLine.isEmpty();
        mDirectoryNoteLabel->setVisible(hasNote);
        if (!hasNote)
        {
            return;
        }
        mDirectoryNoteLabel->setText(mDirectoryNoteLine);
        mDirectoryNoteLabel->setToolTip(mDirectoryNoteTooltip);
        // Warning color when the action removes or overwrites anything
        // (the same reads the old popup painted red); quiet info otherwise.
        const bool severe = mDirectoryNoteLine.contains(
            QCoreApplication::translate("SyncPreviewPairDetailDialog", "removed")) ||
            mDirectoryNoteLine.contains(
                QCoreApplication::translate("SyncPreviewPairDetailDialog", "overwritten"));
        mDirectoryNoteLabel->setStyleSheet(
            QStringLiteral("QLabel { color: %1; }")
                .arg(TokenParserWidgetManager::instance()
                         ->getColor(QLatin1String(severe ? "text-warning" : "text-secondary"))
                         .name()));
    }

    void SyncPreviewPairDetailDialog::onRowActionSelected(const QString& relativePath, Action action)
    {
        const Classification& classification = mController->classification(mPairId);
        const Row* row = classification.find(relativePath);
        const bool isDirectory = row && ((row->local && row->local->isFolder()) ||
                                         (row->remote && row->remote->isFolder()));

        if (isDirectory)
        {
            // Directory-level actions land immediately (MEGA-2.12): the
            // consequences become a one-line footer note (full breakdown in
            // its tooltip) instead of a modal popup — the tester found the
            // popup too much. Canceling no longer exists; un-clicking the
            // arrow clears the note with the decision.
            const Plan preview = mController->previewPlan(mPairId, relativePath, action);
            const RowPlan* directoryPlan = preview.find(relativePath);
            setDirectoryNote(relativePath, directoryPlan);
        }

        // Decide + approve in one gesture (MEGA-2.11 AC#8): the click IS
        // the approval; the commit gate counts flagged rows that were
        // never clicked.
        mController->setAction(mPairId, relativePath, action);
        mController->setApproved(mPairId, relativePath, true);
    }

    void SyncPreviewPairDetailDialog::onRowDecisionCleared(const QString& relativePath)
    {
        // Un-decide (the active button toggled off): an explicit do-nothing
        // decision the planner respects — no recommendation fallback, no
        // consequences note (nothing is applied), and the approval is
        // withdrawn with it.
        if (mDirectoryNotePath == relativePath)
        {
            setDirectoryNote(QString(), nullptr);
        }
        mController->setAction(mPairId, relativePath, Action::None);
        mController->setApproved(mPairId, relativePath, false);
    }

    void SyncPreviewPairDetailDialog::onActionButtonClicked(const QString& relativePath,
                                                            Action choice, bool checked)
    {
        if (relativePath.isEmpty() || !mRowsByPath.contains(relativePath))
        {
            return;
        }

        if (!checked)
        {
            // Only the visually active button can toggle off on click:
            // this is the un-decide gesture.
            onRowDecisionCleared(relativePath);
            return;
        }

        // A new pick: drop the previous active visual (a decision, an
        // inherited action or the recommendation's pre-clicked state).
        const Row* row = mRowsByPath.value(relativePath);
        const QHash<QString, RowDecision> decisions = decisionsFor();
        const bool hasOwnDecision = decisions.contains(relativePath);
        const Action active = hasOwnDecision ? decisions.value(relativePath).action
            : (row->requiresApproval ? Action::None : row->recommendedAction);
        if (active != Action::None && active != choice)
        {
            const QVector<Action>& choices = actionChoices();
            const auto deactivate = [choices, active](const QVector<QPushButton*>& buttons)
            {
                for (int i = 0; i < buttons.size() && i < choices.size(); ++i)
                {
                    if (choices.at(i) == active)
                    {
                        const QSignalBlocker blocker(buttons[i]);
                        buttons[i]->setChecked(false);
                    }
                }
            };
            deactivate(mActionButtons);
            deactivate(mMidButtonsByPath.value(relativePath));
        }

        onRowActionSelected(relativePath, choice);
    }

    void SyncPreviewPairDetailDialog::showChanges()
    {
        const Pair* pair = mController->pair(mPairId);
        if (!pair)
        {
            return;
        }

        const QString pairTitle = pair->localPath + QStringLiteral("  <->  ") + pair->remotePath;
        SyncPreviewChangesDialog changes(pairTitle, mController->plan(mPairId),
                                         mController->awaitingApprovalPaths(mPairId), this);
        // Apply moved into the popup (MEGA-2.11 AC#7): Accepted runs the
        // review loop's apply step here, exactly like the old filter-row
        // button; pairChanged rebuilds the panes.
        if (changes.exec() == QDialog::Accepted)
        {
            applyPlan();
        }
    }

    void SyncPreviewPairDetailDialog::applyPlan()
    {
        // The review loop's Apply step (fake data): the controller applies
        // the plan, re-scans and re-verifies; pairChanged rebuilds the
        // panes, so applied rows vanish or turn Same. No approval gate —
        // undecided flagged rows contribute no operations. No real API
        // calls happen here (the applier mutates the fake trees only).
        mController->applyPlan(mPairId);
    }

    void SyncPreviewPairDetailDialog::synchronizeView()
    {
        // The left pane is the reference: its per-path expansion state is
        // recorded and mirrored onto the right pane, so both panes show
        // the same shape. mSyncingPanes suppresses the mirror signals.
        mSyncingPanes = true;
        for (auto it = mLeftItems.constBegin(); it != mLeftItems.constEnd(); ++it)
        {
            QTreeWidgetItem* leftItem = it.value();
            if (!leftItem->childCount())
            {
                continue;
            }
            const bool expanded = leftItem->isExpanded();
            mExpandedByPath.insert(it.key(), expanded);
            const auto rightIt = mRightItems.constFind(it.key());
            if (rightIt != mRightItems.constEnd() && rightIt.value()
                && rightIt.value()->isExpanded() != expanded)
            {
                rightIt.value()->setExpanded(expanded);
            }
        }
        mSyncingPanes = false;

        // The right pane's free-expansion memory is replaced by the
        // normalized shape, so unlocking afterwards starts from it.
        mRightExpandedByPath = mExpandedByPath;
    }
}
