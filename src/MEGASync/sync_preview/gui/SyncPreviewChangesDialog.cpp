#include "SyncPreviewChangesDialog.h"

#include "SyncPreviewGuiStyle.h"

#include "TokenParserWidgetManager.h"

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
        setAttribute(Qt::WA_DeleteOnClose);

        // Prod theming: the popup is exec()'d (never tracked by
        // DialogOpener), so it must register itself to receive the app's
        // standard-components styling and live theme changes.
        TokenParserWidgetManager::instance()->registerWidgetForTheming(this);

        auto* layout = new QVBoxLayout(this);

        auto* header = new QLabel(tr("Scheduled changes for <b>%1</b>:").arg(pairTitle.toHtmlEscaped()), this);
        header->setTextFormat(Qt::RichText);
        header->setWordWrap(true);
        layout->addWidget(header);

        mChangesTree = new QTreeWidget(this);
        mChangesTree->setRootIsDecorated(false);
        mChangesTree->setUniformRowHeights(true);
        mChangesTree->setAllColumnsShowFocus(true);
        mChangesTree->setColumnCount(2);
        mChangesTree->setHeaderLabels({tr("Path"), tr("Scheduled change")});
        mChangesTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        mChangesTree->header()->setStretchLastSection(true);
        GuiStyle::applyViewPalette(mChangesTree);
        layout->addWidget(mChangesTree, 1);

        const QSet<QString> awaiting{awaitingPaths.cbegin(), awaitingPaths.cend()};

        int entries = 0;
        QSet<QString> warnings;
        for (const RowPlan& rowPlan : plan.rows)
        {
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
                if (awaiting.contains(rowPlan.relativePath))
                {
                    item->setForeground(0, GuiStyle::token(QLatin1String("text-warning")));
                    item->setForeground(1, GuiStyle::token(QLatin1String("text-warning")));
                }
                ++entries;
            }

            for (const QString& warning : rowPlan.warnings)
            {
                warnings.insert(warning);
            }
        }

        if (entries == 0)
        {
            layout->addWidget(new QLabel(tr("No scheduled changes under the current decisions."), this));
        }

        if (!warnings.isEmpty())
        {
            auto* notes = new QLabel(this);
            notes->setTextFormat(Qt::RichText);
            notes->setWordWrap(true);
            QStringList lines;
            for (const QString& warning : warnings)
            {
                lines << QStringLiteral("• %1").arg(warning.toHtmlEscaped());
            }
            notes->setText(QStringLiteral("<span style=\"color:%2;\">%1</span>")
                               .arg(lines.join(QStringLiteral("<br/>")),
                                    GuiStyle::token(QLatin1String("text-warning")).name()));
            layout->addWidget(notes);
        }

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        layout->addWidget(buttons);
    }
}