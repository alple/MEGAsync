#include "SyncPreviewDialog.h"
#include "ui_SyncPreviewDialog.h"
#include "SyncPreviewChangesDialog.h"
#include "SyncPreviewFakeApplier.h"
#include "SyncPreviewFakePairPicker.h"
#include "SyncPreviewGuiStyle.h"
#include "SyncPreviewPairController.h"
#include "SyncPreviewPairDetailDialog.h"
#include "SyncPreviewGuiFormat.h"
#include "SyncPreviewQueueFileStore.h"

#include "DialogOpener.h"
#include "Preferences.h"
#include "ThemeManager.h"
#include "TokenParserWidgetManager.h"

#include <QColor>
#include <QDir>
#include <QFont>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPalette>
#include <QPushButton>
#include <QVBoxLayout>

namespace SyncPreview
{
    SyncPreviewDialog::SyncPreviewDialog(QWidget* parent, const QString& queueFilePath) :
        QDialog(parent),
        mUi(new Ui::SyncPreviewDialog)
    {
        mUi->setupUi(this);
        setAttribute(Qt::WA_DeleteOnClose);
        // The MEGA app icon for the window/taskbar entry (the app-level icon
        // does not reliably reach these non-modal windows under Wayland).
        setWindowIcon(QIcon(QStringLiteral(":/images/app_ico.ico")));

        QString path = queueFilePath;
        if (path.isEmpty())
        {
            // The fork-owned queue file in the app data directory; Stage 5
            // keeps this location for the real commit queue.
            path = Preferences::instance()->getDataPath() + QDir::separator() +
                QStringLiteral("sync-preview-queue.json");
        }

        mController = new PairController(QueueFileStore(path), this);
        // Stage 2 factory: the pair label identifies the fake scenario, so
        // pairs restored from the queue file re-scan to the same data. The
        // scenario store seeds on first use and keeps the Apply-step
        // mutations (MEGA-2.9 review loop) for the session.
        mController->setSideProviderFactory(
            [this](const Pair& pair) -> std::optional<PairSideProviders>
            {
                FakeScenario& scenario = fakeScenarioFor(pair.localPath);
                return PairSideProviders{std::make_shared<FakeSideProvider>(scenario.local),
                                         std::make_shared<FakeSideProvider>(scenario.remote)};
            });
        // Apply step of the review loop: executes the plan on the pair's
        // fake trees; applyPlan() then re-scans and re-verifies the pair.
        mController->setPlanApplier(
            [this](const QString& pairId, const Plan& plan) -> bool
            {
                const Pair* pair = mController->pair(pairId);
                if (!pair)
                {
                    return false;
                }
                FakeScenario& scenario = fakeScenarioFor(pair->localPath);
                SyncPreview::applyPlan(scenario, plan);
                return true;
            });

        connect(mUi->addPairButton, &QPushButton::clicked, this, &SyncPreviewDialog::addPair);
        connect(mUi->closeButton, &QPushButton::clicked, this, &QDialog::close);
        connect(mUi->filterEdit, &QLineEdit::textChanged, this, [this]() { rebuild(); });
        connect(mUi->pairsList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item)
        {
            openPair(item->data(Qt::UserRole).toString());
        });

        connect(mController, &PairController::pairAdded, this, [this](const QString&) { rebuild(); });
        connect(mController, &PairController::pairRemoved, this, [this](const QString&) { rebuild(); });
        connect(mController, &PairController::pairChanged, this, [this](const QString&) { rebuild(); });
        connect(mController, &PairController::restored, this, [this]() { rebuild(); });

        // Per-row widget and palette colors are token colors resolved at
        // populate time: re-resolve when the theme changes so both color
        // schemas read well.
        connect(ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]()
        {
            applyListPalette();
            rebuild();
        });

        // Prod theming: standard-components stylesheet + per-theme token
        // replacement, re-applied on live theme changes. (DialogOpener also
        // themes on show; registering keeps this dialog covered when it is
        // shown outside DialogOpener, e.g. from tests/scaffolding.)
        TokenParserWidgetManager::instance()->registerWidgetForTheming(this);

        // Chrome shares the pair detail window's conventions: themed input,
        // compact filter bar, quiet outline buttons (property-set, so the
        // standard sheet re-themes them on theme change by itself).
        GuiStyle::styleLineEdit(mUi->filterEdit);
        mUi->filterEdit->setMaximumWidth(360);
        GuiStyle::styleOutlineButton(mUi->addPairButton);
        GuiStyle::styleOutlineButton(mUi->closeButton);

        applyListPalette();
        mController->restore();
        rebuild();
    }

    SyncPreviewDialog::~SyncPreviewDialog() = default;

    void SyncPreviewDialog::applyListPalette()
    {
        // Theme tokens instead of ad-hoc greys: the whole window (header,
        // filter row, footer) and the list sit on the page background, rows
        // alternate with the surface color, selection uses the app's inverse
        // accent — same read as the pair detail window.
        GuiStyle::applyWindowPalette(this);
        GuiStyle::applyViewPalette(mUi->pairsList);
    }

    void SyncPreviewDialog::addPair()
    {
        SyncPreviewFakePairPicker picker(this);
        if (picker.exec() != QDialog::Accepted)
        {
            return;
        }

        const PairCandidate candidate = picker.selectedCandidate();
        if (candidate.localPath.isEmpty())
        {
            return;
        }

        mController->addPair(candidate);
    }

    FakeScenario& SyncPreviewDialog::fakeScenarioFor(const QString& scenarioLabel)
    {
        auto it = mFakeScenarios.find(scenarioLabel);
        if (it == mFakeScenarios.end())
        {
            it = mFakeScenarios.insert(scenarioLabel,
                                       SyncPreviewFakePairPicker::scenarioForLabel(scenarioLabel).value_or(FakeScenario{}));
        }
        return it.value();
    }

    void SyncPreviewDialog::showChanges(const QString& pairId)
    {
        const Pair* pair = mController->pair(pairId);
        if (!pair)
        {
            return;
        }

        const QString pairTitle = pair->localPath + QStringLiteral("  <->  ") + pair->remotePath;
        SyncPreviewChangesDialog changes(pairTitle, mController->plan(pairId),
                                         mController->awaitingApprovalPaths(pairId), this);
        // Apply moved into the popup (MEGA-2.11 AC#7): Accepted runs the
        // review loop's apply step for this pair.
        if (changes.exec() == QDialog::Accepted)
        {
            mController->applyPlan(pairId);
        }
    }

    void SyncPreviewDialog::rebuild()
    {
        repopulate();
        updateSummary();
    }

    void SyncPreviewDialog::repopulate()
    {
        const QString filter = mUi->filterEdit->text();

        QListWidget* list = mUi->pairsList;
        list->clear();

        for (const Pair& pair : mController->pairs())
        {
            if (!pairVisible(pair, filter))
            {
                continue;
            }

            auto* item = new QListWidgetItem(list);
            item->setData(Qt::UserRole, pair.id);

            auto* rowWidget = new QWidget(list);
            auto* rowLayout = new QVBoxLayout(rowWidget);
            rowLayout->setContentsMargins(6, 4, 6, 4);
            rowLayout->setSpacing(2);

            auto* headerLayout = new QHBoxLayout();
            headerLayout->setSpacing(6);

            auto* label = new QLabel(pair.localPath + QStringLiteral("  <->  ") + pair.remotePath, rowWidget);
            QFont boldFont = label->font();
            boldFont.setBold(true);
            label->setFont(boldFont);
            headerLayout->addWidget(label);
            headerLayout->addStretch(1);

            auto* reviewButton = new QPushButton(tr("Review…"), rowWidget);
            reviewButton->setToolTip(tr("Opens the dual-pane detail window for this pair"));
            GuiStyle::styleOutlineButton(reviewButton);
            const QString pairId = pair.id;
            connect(reviewButton, &QPushButton::clicked, this, [this, pairId]() { openPair(pairId); });
            headerLayout->addWidget(reviewButton);

            auto* changesButton = new QPushButton(tr("Show changes"), rowWidget);
            changesButton->setToolTip(tr("Lists the changes scheduled under the current decisions (renames included)"));
            GuiStyle::styleOutlineButton(changesButton);
            connect(changesButton, &QPushButton::clicked, this, [this, pairId]() { showChanges(pairId); });
            headerLayout->addWidget(changesButton);

            auto* commitButton = new QPushButton(tr("Commit pair"), rowWidget);
            commitButton->setToolTip(tr("Applies the approved plan to this pair's fake data and drops the pair (mocked sync creation)"));
            GuiStyle::styleOutlineButton(commitButton);
            commitButton->setEnabled(mController->allApproved(pair.id));
            connect(commitButton, &QPushButton::clicked, this, [this, pairId]()
            {
                if (QMessageBox::question(this,
                                          tr("Commit pair"),
                                          tr("Apply the approved plan to this pair's fake data and drop it from the review queue (mocked sync creation)?"))
                    == QMessageBox::Yes)
                {
                    mController->commitPair(pairId);
                }
            });
            headerLayout->addWidget(commitButton);

            auto* removeButton = new QPushButton(tr("Remove"), rowWidget);
            GuiStyle::styleOutlineButton(removeButton);
            connect(removeButton, &QPushButton::clicked, this, [this, pairId]()
            {
                if (QMessageBox::question(this,
                                          tr("Remove pair"),
                                          tr("Remove this pair from the review queue?")) == QMessageBox::Yes)
                {
                    mController->removePair(pairId);
                }
            });
            headerLayout->addWidget(removeButton);

            rowLayout->addLayout(headerLayout);

            auto* statsLabel = new QLabel(pairStatsText(pair), rowWidget);
            statsLabel->setStyleSheet(QStringLiteral("QLabel { color: %1; }")
                                          .arg(TokenParserWidgetManager::instance()
                                                   ->getColor(QLatin1String("text-secondary"))
                                                   .name()));
            rowLayout->addWidget(statsLabel);

            auto* footerLayout = new QHBoxLayout();
            footerLayout->setSpacing(6);
            auto* pendingLabel = new QLabel(pairPendingText(pair), rowWidget);
            footerLayout->addWidget(pendingLabel);
            footerLayout->addStretch(1);
            const int awaiting = mController->awaitingApprovalCount(pair.id);
            auto* awaitingLabel = new QLabel(awaiting > 0
                ? tr("%1 item(s) awaiting approval").arg(awaiting)
                : tr("all flagged items approved"), rowWidget);
            awaitingLabel->setStyleSheet(QStringLiteral("QLabel { color: %1; }")
                                             .arg(awaiting > 0
                                                 ? TokenParserWidgetManager::instance()
                                                       ->getColor(QLatin1String("text-warning"))
                                                       .name()
                                                 : TokenParserWidgetManager::instance()
                                                       ->getColor(QLatin1String("text-success"))
                                                       .name()));
            footerLayout->addWidget(awaitingLabel);
            rowLayout->addLayout(footerLayout);

            item->setSizeHint(rowWidget->sizeHint());
            list->setItemWidget(item, rowWidget);
        }
    }

    QString SyncPreviewDialog::pairStatsText(const Pair& pair) const
    {
        const PairSummary summary = mController->summary(pair.id);
        return tr("local: %1 file(s), %2 dir(s), %3 — remote: %4 file(s), %5 dir(s), %6")
            .arg(summary.localFiles)
            .arg(summary.localDirs)
            .arg(GuiText::sizeBytesText(summary.localBytes))
            .arg(summary.remoteFiles)
            .arg(summary.remoteDirs)
            .arg(GuiText::sizeBytesText(summary.remoteBytes));
    }

    QString SyncPreviewDialog::pairPendingText(const Pair& pair) const
    {
        const PairSummary summary = mController->summary(pair.id);
        QStringList pieces;
        if (summary.pendingLocalFiles > 0)
        {
            pieces << tr("%1 file(s) to local (%2)")
                          .arg(summary.pendingLocalFiles)
                          .arg(GuiText::sizeBytesText(summary.pendingLocalBytes));
        }
        if (summary.pendingLocalRemoved > 0)
        {
            pieces << tr("%1 to local trash").arg(summary.pendingLocalRemoved);
        }
        if (summary.pendingRemoteFiles > 0)
        {
            pieces << tr("%1 file(s) to remote (%2)")
                          .arg(summary.pendingRemoteFiles)
                          .arg(GuiText::sizeBytesText(summary.pendingRemoteBytes));
        }
        if (summary.pendingRemoteRemoved > 0)
        {
            pieces << tr("%1 to MEGA Rubbish").arg(summary.pendingRemoteRemoved);
        }

        if (pieces.isEmpty())
        {
            return tr("no pending transfers");
        }
        return tr("pending: %1").arg(pieces.join(QStringLiteral(" · ")));
    }

    bool SyncPreviewDialog::pairVisible(const Pair& pair, const QString& filter) const
    {
        if (!filter.isEmpty() && !pair.localPath.contains(filter, Qt::CaseInsensitive) &&
            !pair.remotePath.contains(filter, Qt::CaseInsensitive))
        {
            return false;
        }
        return true;
    }

    void SyncPreviewDialog::openPair(const QString& pairId)
    {
        if (!mController->pair(pairId))
        {
            return;
        }

        if (auto* existing = mDetailWindows.value(pairId).data())
        {
            // One window per pair: raise the open one instead of duplicating.
            existing->raise();
            activateWidgetWaylandSafe(existing);
            return;
        }

        auto* detail = new SyncPreviewPairDetailDialog(pairId, mController, this);
        mDetailWindows.insert(pairId, detail);
        connect(detail, &QObject::destroyed, this, [this, pairId]()
        {
            mDetailWindows.remove(pairId);
        });
        detail->show();
    }

    void SyncPreviewDialog::updateSummary()
    {
        int awaiting = 0;
        for (const Pair& pair : mController->pairs())
        {
            awaiting += mController->awaitingApprovalCount(pair.id);
        }

        if (mController->pairs().isEmpty())
        {
            mUi->summaryLabel->setText(tr("No pairs queued yet — add a demo pair."));
        }
        else if (awaiting == 0)
        {
            mUi->summaryLabel->setText(tr("%1 pair(s) queued — everything flagged is approved.")
                                           .arg(mController->pairs().size()));
        }
        else
        {
            mUi->summaryLabel->setText(tr("%1 pair(s) queued — %2 item(s) awaiting approval.")
                                           .arg(mController->pairs().size())
                                           .arg(awaiting));
        }
    }
}