#pragma once

#include <QFont>
#include <QLabel>
#include <QPalette>

namespace aur::theme
{

    inline void dimLabel(QLabel *label)
    {
        label->setForegroundRole(QPalette::PlaceholderText);
    }

    inline void hintLabel(QLabel *label)
    {
        dimLabel(label);
        label->setMargin(3);
    }

    inline void sectionTitle(QLabel *label)
    {
        QFont font = label->font();
        font.setBold(true);
        font.setLetterSpacing(QFont::AbsoluteSpacing, 1.0);
        label->setFont(font);
    }

}
