#include "SyncPreviewRowWidget.h"

#include "TokenParserWidgetManager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>

namespace SyncPreview
{
    SyncPreviewRowWidget::SyncPreviewRowWidget(const Row& row,
                                           const RowPlan& plan,
                                           bool approved,
                                           bool reFlagged,
                                           QWidget* parent) :
    QWidget(parent),
    mRow(row),
    mPlan(plan)
{
        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(4, 0, 4, 0);
        layout->setSpacing(6);

        // Prod's global sheet paints disabled combos with the low-contrast
        // text-disabled token; blocker rows (the only disabled ones) read
        // poorly with it. Scoped override to the more readable text-secondary
        // token — same de-emphasis, more contrast. Resolved at construction:
        // the dialogs re-populate on theme change (ThemeManager::themeChanged
        // hook), which recreates the row widgets with fresh token colors.
        const QColor disabledText =
            TokenParserWidgetManager::instance()->getColor(QLatin1String("text-secondary"));
        setStyleSheet(QStringLiteral("QComboBox:disabled { color: %1; }").arg(disabledText.name()));

        mActionCombo = new QComboBox(this);
        // Prod widget design: the app's themed combo/checkbox components are
        // keyed on the type="mega" property; without it the widgets fall
        // back to native Qt styling (part of the MEGA-2.7 readability
        // feedback).
        mActionCombo->setProperty("type", QLatin1String("mega"));
        const QVector<Action> choices{Action::LocalToRemote, Action::RemoteToLocal, Action::BestEffort, Action::None};
        int effectiveIndex = -1;
        for (int i = 0; i < choices.size(); ++i)
        {
            const Action choice = choices.at(i);
            mActionCombo->insertItem(i, actionLabel(choice, choice == mRow.recommendedAction));
            if (choice == mPlan.action)
            {
                effectiveIndex = i;
            }
        }
        if (effectiveIndex < 0)
        {
            // The effective action is outside the picker choices: add it as
            // a hidden-feel entry so the combo still shows something sane.
            mActionCombo->insertItem(choices.size(), actionLabel(mPlan.action, false));
            effectiveIndex = choices.size();
        }
        {
            // Programmatic selection must not re-enter the controller: the
            // combo's currentIndexChanged fires on programmatic changes too,
            // and the handler would trigger a rebuild loop.
            const QSignalBlocker blocker(mActionCombo);
            mActionCombo->setCurrentIndex(effectiveIndex);
        }
        mActionCombo->setEnabled(mRow.kind != RowKind::Blocker);
        connect(mActionCombo,
                QOverload<int>::of(&QComboBox::currentIndexChanged),
                this,
                [this](int index)
                {
                    static const QVector<Action> choices{Action::LocalToRemote, Action::RemoteToLocal, Action::BestEffort, Action::None};
                    if (index < 0 || index >= choices.size())
                    {
                        return;
                    }
                    emit actionSelected(mRow.relativePath, choices.at(index));
                });
        layout->addWidget(mActionCombo);

        if (mRow.requiresApproval)
        {
            mApproveBox = new QCheckBox(tr("Approved"), this);
            mApproveBox->setProperty("type", QLatin1String("mega"));
            connect(mApproveBox, &QCheckBox::toggled, this, [this](bool checked)
            {
                emit approvalToggled(mRow.relativePath, checked);
            });
            {
                const QSignalBlocker blocker(mApproveBox);
                mApproveBox->setChecked(approved);
            }
            layout->addWidget(mApproveBox);

            if (reFlagged)
            {
                auto* reFlagLabel = new QLabel(tr("(classification changed — re-approve)"), this);
                // Theme token, resolved at construction (dialogs re-populate
                // on theme change, see the disabled-combo note above).
                reFlagLabel->setStyleSheet(QStringLiteral("QLabel { color: %1; }")
                                               .arg(TokenParserWidgetManager::instance()
                                                        ->getColor(QLatin1String("text-warning"))
                                                        .name()));
                layout->addWidget(reFlagLabel);
            }
        }

        layout->addStretch(1);
    }

    QString SyncPreviewRowWidget::actionLabel(Action action, bool recommended) const
    {
        QString label;
        switch (action)
        {
            case Action::LocalToRemote:
                label = tr("L→R");
                break;
            case Action::RemoteToLocal:
                label = tr("R→L");
                break;
            case Action::BestEffort:
                label = tr("Best-effort");
                break;
            case Action::None:
                label = tr("Do nothing");
                break;
        }

        if (recommended)
        {
            label += tr(" — recommended");
        }
        return label;
    }
}