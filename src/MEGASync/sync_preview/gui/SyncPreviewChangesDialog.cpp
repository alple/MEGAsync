#include "SyncPreviewChangesDialog.h"

#include "SyncPreviewGuiStyle.h"

#include "ThemeManager.h"
#include "TokenParserWidgetManager.h"

#include <QBrush>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSet>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace SyncPreview
{
    namespace
    {
        QString changeKindText(OperationType type, bool isFolder, bool isSubtree)
        {
            QString text;
            switch (type)
            {
                case OperationType::Upload:
                    text = SyncPreviewChangesDialog::tr("created on MEGA (upload)");
                    break;
                case OperationType::UploadReplace:
                    text = SyncPreviewChangesDialog::tr("overwritten on MEGA (previous copy recoverable)");
                    break;
                case OperationType::Download:
                    text = SyncPreviewChangesDialog::tr("created locally (download)");
                    break;
                case OperationType::DownloadReplace:
                    text = SyncPreviewChangesDialog::tr("overwritten locally (previous copy recoverable)");
                    break;
                case OperationType::DeleteRemoteToRubbish:
                    text = SyncPreviewChangesDialog::tr("moved to MEGA Rubbish");
                    break;
                case OperationType::DeleteLocalToTrash:
                    text = SyncPreviewChangesDialog::tr("moved to the OS trash");
                    break;
                case OperationType::RenameRemote:
                case OperationType::RenameLocal:
                    break; // spelled out by the caller with its from → to paths
                case OperationType::None:
                    break;
            }
            if (isFolder && isSubtree)
            {
                text += SyncPreviewChangesDialog::tr(" (folder, includes contents)");
            }
            return text;
        }
    }

    SyncPreviewChangesDialog::SyncPreviewChangesDialog(const QString& pairTitle,
                                                       const Plan& plan,
                                                       const QStringList& awaitingPaths,
                                                       QWidget* parent) :
        QDialog(parent)
    {
        setWindowTitle(tr("Scheduled changes"));
        setMinimumSize(560, 400);
        // No WA_DeleteOnClose: this dialog is stack-allocated and exec()'d
        // (lifetime scoped by exec); with the attribute set Qt schedules a
        // deferred delete of a stack object and the app aborts on close
        // with "free(): invalid size" (MEGA-2.11 AC#1).

        // Prod theming: the popup is exec()'d (never tracked by
        // DialogOpener), so it must register itself to receive the app's
        // standard-components styling and live theme changes.
        TokenParserWidgetManager::instance()->registerWidgetForTheming(this);

        auto* layout = new QVBoxLayout(this);

        mHeaderLabel = new QLabel(tr("Scheduled changes for <b>%1</b>:").arg(pairTitle.toHtmlEscaped()), this);
        mHeaderLabel->setTextFormat(Qt::RichText);
        mHeaderLabel->setWordWrap(true);
        layout->addWidget(mHeaderLabel);

        mChangesTree = new QTreeWidget(this);
        mChangesTree->setRootIsDecorated(false);
        mChangesTree->setUniformRowHeights(true);
        mChangesTree->setAllColumnsShowFocus(true);
        mChangesTree->setAlternatingRowColors(true);
        mChangesTree->setColumnCount(2);
        mChangesTree->setHeaderLabels({tr("Path"), tr("Scheduled change")});
        mChangesTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        mChangesTree->header()->setStretchLastSection(true);
        layout->addWidget(mChangesTree, 1);

        const QSet<QString> awaiting{awaitingPaths.cbegin(), awaitingPaths.cend()};

        int entries = 0;
        for (const RowPlan& rowPlan : plan.rows)
        {
            // Per-row warnings (MEGA-2.12): the triangle icon goes on each
            // affected row; a row covered by a directory subtree op folds
            // its warnings into the covering directory row's tooltip, so
            // the aggregated bottom list can go.
            QStringList rowWarnings = rowPlan.warnings;
            if (!rowPlan.coveredByPath.isEmpty() && !rowWarnings.isEmpty())
            {
                mWarningsByPath[rowPlan.coveredByPath] += rowWarnings;
                rowWarnings.clear();
            }
            else if (!rowWarnings.isEmpty())
            {
                mWarningsByPath[rowPlan.relativePath] += rowWarnings;
            }

            if (!rowPlan.coveredByPath.isEmpty())
            {
                continue;  // inside a directory's subtree operation; listed there
            }
            for (const PlannedOperation& operation : rowPlan.operations)
            {
                QString change;
                switch (operation.type)
                {
                    case OperationType::RenameRemote:
                    case OperationType::RenameLocal:
                        change = tr("%1: %2 → %3")
                                      .arg(operation.type == OperationType::RenameRemote ? tr("renamed on MEGA") : tr("renamed locally"),
                                           operation.fromPath, operation.toPath);
                        break;
                    case OperationType::None:
                        continue;
                    default:
                        change = changeKindText(operation.type, operation.isFolder, operation.isSubtree);
                        break;
                }

                auto* item = new QTreeWidgetItem(mChangesTree);
                item->setText(0, operation.path);
                item->setText(1, change);
                // The exclamation triangle on the path cell when this row's
                // plan carries warnings; the tooltip spells them out.
                if (mWarningsByPath.contains(operation.path))
                {
                    QIcon warningIcon;
                    warningIcon.addFile(QStringLiteral(":/images/alert-triangle-small.png"));
                    warningIcon.addFile(QStringLiteral(":/images/alert-triangle-small@2x.png"));
                    item->setIcon(0, warningIcon);
                    item->setToolTip(0, mWarningsByPath.value(operation.path).join(QLatin1Char('\n')));
                }
                if (awaiting.contains(rowPlan.relativePath))
                {
                    item->setData(0, Qt::UserRole, QLatin1String("awaiting"));
                    item->setData(1, Qt::UserRole, QLatin1String("awaiting"));
                }
                ++entries;
            }
        }

        if (entries == 0)
        {
            mEmptyLabel = new QLabel(tr("No scheduled changes under the current decisions."), this);
            layout->addWidget(mEmptyLabel);
        }

        // The review loop's Apply step lives here (MEGA-2.11 AC#7): Apply
        // executes exactly the listed plan (the caller runs it on Accepted)
        // and closes the popup; Close just closes. The Apply standard
        // button emits only clicked(), never accepted(), so it is wired
        // directly.
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Apply | QDialogButtonBox::Close, this);
        buttons->button(QDialogButtonBox::Apply)
            ->setToolTip(tr("Executes the scheduled changes on the fake data and re-resolves the panes (review loop, no real transfer)"));
        connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        layout->addWidget(buttons);

        applyPalette();

        // Rows whose colors are token-resolved re-resolve on live theme
        // changes, like the rest of the sync_preview windows.
        connect(ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]()
        {
            applyPalette();
        });
    }

    void SyncPreviewChangesDialog::applyPalette()
    {
        auto theme = TokenParserWidgetManager::instance();

        // Same read as the pair-detail window: the window (and the labels
        // the app stylesheet leaves dark-on-dark) pinned to token colors,
        // the tree on the page background with zebra rows, the buttons in
        // the proven quiet chrome sheet. Everything re-resolves on theme
        // change.
        GuiStyle::applyWindowPalette(this);
        GuiStyle::applyViewPalette(mChangesTree);

        mHeaderLabel->setStyleSheet(
            QStringLiteral("QLabel { color: %1; }")
                .arg(theme->getColor(QLatin1String("text-primary")).name()));
        if (mEmptyLabel)
        {
            mEmptyLabel->setStyleSheet(
                QStringLiteral("QLabel { color: %1; }")
                    .arg(theme->getColor(QLatin1String("text-secondary")).name()));
        }

        // Row text: primary everywhere except the awaiting-approval rows
        // (warning color), re-resolved over the stored marker. The warning
        // triangles (MEGA-2.12) need no re-resolution — they are a themed
        // icon on the row, not text.
        for (int row = 0; row < mChangesTree->topLevelItemCount(); ++row)
        {
            QTreeWidgetItem* item = mChangesTree->topLevelItem(row);
            const QColor color = item->data(0, Qt::UserRole) == QLatin1String("awaiting")
                ? theme->getColor(QLatin1String("text-warning"))
                : theme->getColor(QLatin1String("text-primary"));
            item->setForeground(0, QBrush(color));
            item->setForeground(1, QBrush(color));
        }

        for (QPushButton* button : findChildren<QPushButton*>())
        {
            button->setStyleSheet(GuiStyle::actionButtonStyleSheet());
        }
    }
}
