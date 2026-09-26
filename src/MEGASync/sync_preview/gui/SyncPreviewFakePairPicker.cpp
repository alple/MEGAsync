#include "SyncPreviewFakePairPicker.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace SyncPreview
{
    namespace
    {
        const QLatin1String kitchenSinkLabel("Demo: kitchen sink (all edge cases)");
        const QLatin1String renameSwapLabel("Demo: rename swap (content moved to a new name)");
        const QLatin1String renameChainLabel("Demo: rename chain (double rename, both directions)");
        const QLatin1String renameEditLabel("Demo: rename + edit (no same-content counterpart)");
        const QLatin1String renameCrossFolderLabel("Demo: rename across folders (moved copy)");
        const QLatin1String longListLabel("Demo: long list (600 rows, load-more)");
        const QLatin1String emptyPairLabel("Demo: empty pair (no differences)");
        constexpr int longListRows = 600;
    }

    SyncPreviewFakePairPicker::SyncPreviewFakePairPicker(QWidget* parent) :
        QDialog(parent)
    {
        setWindowTitle(tr("Add a demo pair (fake data)"));
        setMinimumWidth(420);

        auto* layout = new QVBoxLayout(this);
        layout->addWidget(new QLabel(tr("These are canned fake pairs standing in for real local/remote folders until Stage 5. Pick one to add it to the review queue."), this));
        layout->addWidget(new QLabel(tr("The pair label doubles as the fake-data source, so restored pairs re-scan to the same scenario."), this));

        mScenarioList = new QListWidget(this);
        mScenarioList->addItems(scenarioLabels());
        mScenarioList->setCurrentRow(0);
        layout->addWidget(mScenarioList);

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        buttons->button(QDialogButtonBox::Ok)->setText(tr("Add pair"));
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        layout->addWidget(buttons);
    }

    PairCandidate SyncPreviewFakePairPicker::selectedCandidate() const
    {
        PairCandidate candidate;
        auto* item = mScenarioList->currentItem();
        if (!item)
        {
            return candidate;
        }

        candidate.localPath = item->text();
        candidate.remotePath = item->text() + QStringLiteral(" (remote)");
        return candidate;
    }

    QStringList SyncPreviewFakePairPicker::scenarioLabels()
    {
        return {QString(kitchenSinkLabel),
                QString(renameSwapLabel),
                QString(renameChainLabel),
                QString(renameEditLabel),
                QString(renameCrossFolderLabel),
                QString(longListLabel),
                QString(emptyPairLabel)};
    }

    std::optional<FakeScenario> SyncPreviewFakePairPicker::scenarioForLabel(const QString& label)
    {
        if (label == kitchenSinkLabel)
        {
            return FakeScenarios::edgeCaseKitchenSink();
        }
        if (label == renameSwapLabel)
        {
            return FakeScenarios::renameSwap();
        }
        if (label == renameChainLabel)
        {
            return FakeScenarios::renameChain();
        }
        if (label == renameEditLabel)
        {
            return FakeScenarios::renameEdit();
        }
        if (label == renameCrossFolderLabel)
        {
            return FakeScenarios::renameCrossFolder();
        }
        if (label == longListLabel)
        {
            return FakeScenarios::manyLocalOnlyFiles(longListRows);
        }
        if (label == emptyPairLabel)
        {
            return FakeScenarios::emptySides();
        }
        return std::nullopt;
    }
}