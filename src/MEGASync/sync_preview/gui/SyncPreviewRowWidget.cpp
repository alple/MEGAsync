#include "SyncPreviewRowWidget.h"

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

        mActionCombo = new QComboBox(this);
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