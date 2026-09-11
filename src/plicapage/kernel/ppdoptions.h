/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL2+
 *
 * Copyright: 2026 Matthew Daines <spuddermax@gmail.com>
 * Authors:
 *   Matthew Daines <spuddermax@gmail.com>
 *
 * This program or library is free software; you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 *
 * END_COMMON_COPYRIGHT_HEADER */

#ifndef PPDOPTIONS_H
#define PPDOPTIONS_H

#include <QList>
#include <QSizeF>
#include <QString>

struct PpdChoice
{
    QString keyword;    // What lpr is given: -o <option>=<keyword>
    QString text;       // What the PPD calls it in the UI
};

struct PpdPaperSize
{
    QString keyword;    // PPD PageSize choice, e.g. "Letter"
    QString text;
    QSizeF  size;       // points
    qreal left, right, top, bottom;   // the PPD's imageable-area margins, points
};

struct PpdOption
{
    QString keyword;
    QString text;
    QString group;          // The PPD's UI group text, e.g. "Output Control Common"
    QString defaultChoice;  // Keyword of the choice the queue uses when nothing is passed
    QList<PpdChoice> choices;

    QString defaultText() const;
};

/**
 * The options a printer's PPD lets a job choose, in the PPD's own order and
 * groups, so the settings dialog can offer them without knowing anything about
 * the driver. Options PlicaPage manages itself (paper size, colour, duplex) and
 * ones that can't be chosen per job (installable hardware) are left out.
 */
class PpdOptions
{
public:
    explicit PpdOptions(const QString &printerName);

    bool isValid() const { return mValid; }
    const QList<PpdOption> &options() const { return mOptions; }

    /// The option that behaves as "print quality" on this driver, or an empty
    /// string if there is nothing that looks like one.
    QString qualityKeyword() const { return mQualityKeyword; }

    /// Every paper size the PPD defines, in PPD order, and the one the queue
    /// uses when a job names none.
    const QList<PpdPaperSize> &paperSizes() const { return mPaperSizes; }
    QString defaultPaperSize() const { return mDefaultPaperSize; }

private:
    QList<PpdPaperSize> mPaperSizes;
    QString mDefaultPaperSize;
    bool mValid;
    QList<PpdOption> mOptions;
    QString mQualityKeyword;
};

#endif // PPDOPTIONS_H
