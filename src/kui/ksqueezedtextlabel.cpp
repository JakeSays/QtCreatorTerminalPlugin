/* This file is part of the KDE libraries
   Copyright (C) 2000 Ronny Standtke <Ronny.Standtke@gmx.de>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "ksqueezedtextlabel.h"
#include "qregularexpression.h"
#include <QAction>
#include <QContextMenuEvent>
#include <QMenu>
#include <QClipboard>
#include <QApplication>
#include <QTextDocument>

class KSqueezedTextLabelPrivate
{
public:

    void _k_copyFullText()
    {
        QApplication::clipboard()->setText(fullText);
    }

    QString fullText;
    Qt::TextElideMode elideMode;
};

KSqueezedTextLabel::KSqueezedTextLabel(const QString &text, QWidget *parent)
    : QLabel(parent),
      d(new KSqueezedTextLabelPrivate)
{
    setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed));
    d->fullText = text;
    d->elideMode = Qt::ElideMiddle;
    squeezeTextToLabel();
}

KSqueezedTextLabel::KSqueezedTextLabel(QWidget *parent)
    : QLabel(parent),
      d(new KSqueezedTextLabelPrivate)
{
    setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed));
    d->elideMode = Qt::ElideMiddle;
}

KSqueezedTextLabel::~KSqueezedTextLabel()
{
    delete d;
}

void KSqueezedTextLabel::resizeEvent(QResizeEvent *)
{
    squeezeTextToLabel();
}

QSize KSqueezedTextLabel::minimumSizeHint() const
{
    QSize sh = QLabel::minimumSizeHint();
    sh.setWidth(-1);
    return sh;
}

QSize KSqueezedTextLabel::sizeHint() const
{
    int maxWidth = QGuiApplication::primaryScreen()->geometry().width() * 3 / 4;
    QFontMetrics fm(fontMetrics());
    int textWidth = fm.boundingRect(d->fullText).width();
    if (textWidth > maxWidth) {
        textWidth = maxWidth;
    }
    const int chromeWidth = width() - contentsRect().width();
    return QSize(textWidth + chromeWidth, QLabel::sizeHint().height());
}

void KSqueezedTextLabel::setIndent(int indent)
{
    QLabel::setIndent(indent);
    squeezeTextToLabel();
}

void KSqueezedTextLabel::setMargin(int margin)
{
    QLabel::setMargin(margin);
    squeezeTextToLabel();
}

void KSqueezedTextLabel::setText(const QString &text)
{
    d->fullText = text;
    squeezeTextToLabel();
}

void KSqueezedTextLabel::clear()
{
    d->fullText.clear();
    QLabel::clear();
}

void KSqueezedTextLabel::squeezeTextToLabel()
{
    QFontMetrics fm(fontMetrics());
    const int labelWidth = contentsRect().width();
    QStringList squeezedLines;
    bool squeezed = false;
    const auto textLines = d->fullText.split(QLatin1Char('\n'));
    squeezedLines.reserve(textLines.size());
    for (const QString &line : textLines) {
        int lineWidth = fm.boundingRect(line).width();
        if (lineWidth > labelWidth) {
            squeezed = true;
            squeezedLines << fm.elidedText(line, d->elideMode, labelWidth);
        } else {
            squeezedLines << line;
        }
    }

    if (squeezed) {
        QLabel::setText(squeezedLines.join(QLatin1Char('\n')));
        setToolTip(d->fullText);
    } else {
        QLabel::setText(d->fullText);
        setToolTip(QString());
    }
}

QRect KSqueezedTextLabel::contentsRect() const
{
    // calculation according to API docs for QLabel::indent
    const int margin = this->margin();
    int indent = this->indent();
    if (indent < 0) {
        if (frameWidth() == 0) {
            indent = 0;
        } else {
            indent = fontMetrics().horizontalAdvance(QLatin1Char('x')) / 2 - margin;
        }
    }

    QRect result = QLabel::contentsRect();
    if (indent > 0) {
        const int alignment = this->alignment();
        if (alignment & Qt::AlignLeft) {
            result.setLeft(result.left() + indent);
        }
        if (alignment & Qt::AlignTop) {
            result.setTop(result.top() + indent);
        }
        if (alignment & Qt::AlignRight) {
            result.setRight(result.right() - indent);
        }
        if (alignment & Qt::AlignBottom) {
            result.setBottom(result.bottom() - indent);
        }
    }

    result.adjust(margin, margin, -margin, -margin);
    return result;
}

void KSqueezedTextLabel::setAlignment(Qt::Alignment alignment)
{
    // save fullText and restore it
    QString tmpFull(d->fullText);
    QLabel::setAlignment(alignment);
    d->fullText = tmpFull;
}

Qt::TextElideMode KSqueezedTextLabel::textElideMode() const
{
    return d->elideMode;
}

void KSqueezedTextLabel::setTextElideMode(Qt::TextElideMode mode)
{
    d->elideMode = mode;
    squeezeTextToLabel();
}

QString KSqueezedTextLabel::fullText() const
{
    return d->fullText;
}

bool KSqueezedTextLabel::isSqueezed() const
{
    return d->fullText != text();
}

void KSqueezedTextLabel::contextMenuEvent(QContextMenuEvent *ev)
{
    // We want to reimplement "Copy" to include the elided text.
    // But this means reimplementing the full popup menu, so no more
    // copy-link-address or copy-selection support anymore, since we
    // have no access to the QTextDocument.
    // Maybe we should have a boolean flag in KSqueezedTextLabel itself for
    // whether to show the "Copy Full Text" custom popup?
    // For now I chose to show it when the text is squeezed; when it's not, the
    // standard popup menu can do the job (select all, copy).

    if (isSqueezed()) {
        QMenu menu(this);

        QAction *act = new QAction(tr("&Copy Full Text"), &menu);
        connect(act, &QAction::triggered, this, [this]() { d->_k_copyFullText(); });
        menu.addAction(act);

        ev->accept();
        menu.exec(ev->globalPos());
    } else {
        QLabel::contextMenuEvent(ev);
    }
}

void KSqueezedTextLabel::mouseReleaseEvent(QMouseEvent *ev)
{
    if (QApplication::clipboard()->supportsSelection() &&
            textInteractionFlags() != Qt::NoTextInteraction &&
            ev->button() == Qt::LeftButton &&
            !d->fullText.isEmpty() &&
            hasSelectedText()) {
        // Expand "..." when selecting with the mouse
        QString txt = selectedText();
        const QChar ellipsisChar(0x2026); // from qtextengine.cpp
        const int dotsPos = txt.indexOf(ellipsisChar);
        if (dotsPos > -1) {
            // Ex: abcde...yz, selecting de...y  (selectionStart=3)
            // charsBeforeSelection = selectionStart = 2 (ab)
            // charsAfterSelection = 1 (z)
            // final selection length= 26 - 2 - 1 = 23
            const int start = selectionStart();
            int charsAfterSelection = text().length() - start - selectedText().length();
            txt = d->fullText;
            // Strip markup tags
            if (textFormat() == Qt::RichText
                    || (textFormat() == Qt::AutoText && Qt::mightBeRichText(txt))) {
                txt.remove(QRegularExpression(QStringLiteral("<[^>]*>")));
                // account for stripped characters
                charsAfterSelection -= d->fullText.length() - txt.length();
            }
            txt = txt.mid(selectionStart(), txt.length() - start - charsAfterSelection);
        }
        QApplication::clipboard()->setText(txt, QClipboard::Selection);
    } else {
        QLabel::mouseReleaseEvent(ev);
    }
}

#include "moc_ksqueezedtextlabel.cpp"
