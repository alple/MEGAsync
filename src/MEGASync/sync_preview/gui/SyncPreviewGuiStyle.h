#ifndef SYNCPREVIEWGUISTYLE_H
#define SYNCPREVIEWGUISTYLE_H

#include "TokenParserWidgetManager.h"

#include <QAbstractItemView>
#include <QColor>
#include <QHeaderView>
#include <QLineEdit>
#include <QLatin1String>
#include <QPalette>
#include <QPushButton>
#include <QWidget>

namespace SyncPreview
{
    // Shared token styling for the sync_preview windows (pair list + pair
    // detail), so both read congruently in both color schemas. Property-set
    // chrome (type/dimension) is themed by the app's standard-components
    // sheet and re-tokenizes on theme change by itself; the checkable action
    // buttons have no prod counterpart, so they use a custom quiet sheet
    // that the owning dialogs re-resolve on ThemeManager::themeChanged.
    namespace GuiStyle
    {
        inline QColor token(const QLatin1String& name)
        {
            return TokenParserWidgetManager::instance()->getColor(name);
        }

        // The whole window sits on the page background with primary text.
        inline void applyWindowPalette(QWidget* window)
        {
            QPalette palette = window->palette();
            palette.setColor(QPalette::Window, token(QLatin1String("page-background")));
            palette.setColor(QPalette::WindowText, token(QLatin1String("text-primary")));
            window->setPalette(palette);
        }

        // A list/tree view on the page background; selection is a subtle
        // translucent grey (the inverse accent read as glaringly bright over
        // the dark panes), with primary text on top of it.
        inline void applyViewPalette(QAbstractItemView* view)
        {
            QPalette palette = view->palette();
            palette.setColor(QPalette::Base, token(QLatin1String("page-background")));
            palette.setColor(QPalette::AlternateBase, token(QLatin1String("surface-1")));
            palette.setColor(QPalette::Text, token(QLatin1String("text-primary")));
            palette.setColor(QPalette::WindowText, token(QLatin1String("text-primary")));
            palette.setColor(QPalette::Button, token(QLatin1String("page-background")));
            palette.setColor(QPalette::ButtonText, token(QLatin1String("text-secondary")));
            palette.setColor(QPalette::Highlight, token(QLatin1String("neutral-container-hover")));
            palette.setColor(QPalette::HighlightedText, token(QLatin1String("text-primary")));
            view->setPalette(palette);
            if (auto* header = qobject_cast<QHeaderView*>(view->findChild<QHeaderView*>()))
            {
                header->setPalette(palette);
            }
        }

        // The app's quiet chrome button: transparent with a hairline
        // border (type="outline" in the standard sheet), small dimension —
        // themed entirely by the standard sheet, re-themes automatically.
        inline void styleOutlineButton(QPushButton* button)
        {
            button->setProperty("type", QLatin1String("outline"));
            button->setProperty("dimension", QLatin1String("small"));
        }

        // The app's themed single-line input (dark surface + hairline).
        inline void styleLineEdit(QLineEdit* edit)
        {
            edit->setProperty("type", QLatin1String("mega"));
            edit->setProperty("dimension", QLatin1String("small"));
        }

        // Custom quiet style for the detail window's exclusive action
        // buttons: readable outline in the normal state, inverse accent when
        // checked, and a readable (not near-invisible) disabled state.
        // Re-resolve on theme change (the dialog does in applyPanesPalette).
        inline QString actionButtonStyleSheet()
        {
            return QStringLiteral(
                "QPushButton {"
                " background: transparent;"
                " border: 1px solid %1;"
                " border-radius: 4px;"
                " padding: 4px 14px;"
                " color: %2;"
                " }"
                "QPushButton:hover { border-color: %3; }"
                "QPushButton:checked { background: %3; color: %4; border-color: %3; }"
                "QPushButton:disabled { color: %5; border-color: %1; }")
                .arg(token(QLatin1String("border-strong")).name(),
                     token(QLatin1String("text-primary")).name(),
                     token(QLatin1String("surface-inverse-accent")).name(),
                     token(QLatin1String("text-inverse-accent")).name(),
                     token(QLatin1String("text-secondary")).name());
        }

        // The approval toggle lights up in the success color when checked.
        inline QString approveButtonStyleSheet()
        {
            return QStringLiteral(
                "QPushButton {"
                " background: transparent;"
                " border: 1px solid %1;"
                " border-radius: 4px;"
                " padding: 4px 14px;"
                " color: %2;"
                " }"
                "QPushButton:hover { border-color: %3; }"
                "QPushButton:checked { background: %3; color: %4; border-color: %3; }"
                "QPushButton:disabled { color: %5; border-color: %1; }")
                .arg(token(QLatin1String("border-strong")).name(),
                     token(QLatin1String("text-primary")).name(),
                     token(QLatin1String("text-success")).name(),
                     token(QLatin1String("page-background")).name(),
                     token(QLatin1String("text-secondary")).name());
        }
    }
}
#endif // SYNCPREVIEWGUISTYLE_H
