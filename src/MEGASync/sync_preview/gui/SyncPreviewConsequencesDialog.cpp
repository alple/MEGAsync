#include "SyncPreviewConsequencesDialog.h"

#include "SyncPreviewGuiStyle.h"

#include "ThemeManager.h"
#include "TokenParserWidgetManager.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace SyncPreview
{
    namespace
    {
        void addSection(QVBoxLayout* layout, const QString& title, const QStringList& paths)
        {
            if (paths.isEmpty())
            {
                return;
            }

            auto* sectionTitle = new QLabel(QStringLiteral("<b>%1 (%2)</b>").arg(title.toHtmlEscaped()).arg(paths.size()));
            sectionTitle->setTextFormat(Qt::RichText);
            layout->addWidget(sectionTitle);

            for (const QString& path : paths)
            {
                auto* pathLabel = new QLabel(QStringLiteral("• %1").arg(path.toHtmlEscaped()));
                pathLabel->setTextFormat(Qt::RichText);
                pathLabel->setContentsMargins(12, 0, 0, 0);
                layout->addWidget(pathLabel);
            }
        }
    }

    SyncPreviewConsequencesDialog::SyncPreviewConsequencesDialog(const QString& directoryPath,
                                                                 const RowPlan& directoryPlan,
                                                                 QWidget* parent) :
        QDialog(parent)
    {
        setWindowTitle(tr("Consequences of the directory action"));
        setMinimumWidth(520);
        // No WA_DeleteOnClose: stack-allocated + exec()'d (see
        // SyncPreviewChangesDialog) — the attribute would delete a stack
        // object on close and abort the app (MEGA-2.11 AC#1).

        // Prod theming: the popup is exec()'d (never tracked by
        // DialogOpener), so it must register itself to receive the app's
        // standard-components styling and live theme changes.
        TokenParserWidgetManager::instance()->registerWidgetForTheming(this);

        auto* layout = new QVBoxLayout(this);

        auto* header = new QLabel(tr("Applying the chosen action under <b>%1</b> will:").arg(directoryPath.toHtmlEscaped()), this);
        header->setTextFormat(Qt::RichText);
        header->setWordWrap(true);
        layout->addWidget(header);

        // The directory row aggregates its whole cascaded subtree, so these
        // lists are exactly "which files are removed and which changed".
        addSection(layout, tr("Created on local disk"), directoryPlan.createdLocal);
        addSection(layout, tr("Overwritten locally (previous copy recoverable)"), directoryPlan.changedLocal);
        addSection(layout, tr("Removed locally (moved to the OS trash)"), directoryPlan.removedLocal);
        addSection(layout, tr("Created on MEGA"), directoryPlan.createdRemote);
        addSection(layout, tr("Overwritten remotely (previous copy to Rubbish)"), directoryPlan.changedRemote);
        addSection(layout, tr("Removed remotely (moved to MEGA Rubbish)"), directoryPlan.removedRemote);
        addSection(layout, tr("Warnings"), directoryPlan.warnings);

        const bool noEffects = directoryPlan.createdLocal.isEmpty() &&
            directoryPlan.changedLocal.isEmpty() &&
            directoryPlan.removedLocal.isEmpty() &&
            directoryPlan.createdRemote.isEmpty() &&
            directoryPlan.changedRemote.isEmpty() &&
            directoryPlan.removedRemote.isEmpty();
        if (noEffects)
        {
            layout->addWidget(new QLabel(tr("No files under this directory are affected."), this));
        }

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        buttons->button(QDialogButtonBox::Ok)->setText(tr("Apply"));
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        layout->addWidget(buttons);

        applyPalette();

        // Labels and buttons re-resolve their token colors on live theme
        // changes, like the rest of the sync_preview windows.
        connect(ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]()
        {
            applyPalette();
        });
    }

    void SyncPreviewConsequencesDialog::applyPalette()
    {
        auto theme = TokenParserWidgetManager::instance();

        // Same read as the pair-detail window: the window (and the labels
        // the app stylesheet leaves dark-on-dark) pinned to token colors,
        // the buttons in the proven quiet chrome sheet.
        GuiStyle::applyWindowPalette(this);

        for (QLabel* label : findChildren<QLabel*>())
        {
            label->setStyleSheet(
                QStringLiteral("QLabel { color: %1; }")
                    .arg(theme->getColor(QLatin1String("text-primary")).name()));
        }

        for (QPushButton* button : findChildren<QPushButton*>())
        {
            button->setStyleSheet(GuiStyle::actionButtonStyleSheet());
        }
    }
}
