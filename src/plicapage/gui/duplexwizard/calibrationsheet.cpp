/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL2+
 *
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


#include "calibrationsheet.h"
#include "kernel/printer.h"
#include "plicapagetypes.h"

#include <QCoreApplication>
#include <QFont>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QRectF>


/************************************************
 * Height of the edge bar, and how far the big glyph sits below it.
 ************************************************/
#define BAR_HEIGHT_PT   54.0
#define GLYPH_SIZE_PT   190.0


/************************************************

 ************************************************/
static void drawSheet(QPainter *painter, const QRectF &pageRect,
                      const QString &glyph, const QString &sideLabel,
                      const QString &caption)
{
    painter->save();

    // The bar, flush against the top of the printable area. Everything the user
    // is asked to compare is this bar's position on the paper.
    const QRectF bar(pageRect.left(), pageRect.top(), pageRect.width(), BAR_HEIGHT_PT);
    painter->fillRect(bar, Qt::black);

    QFont barFont = painter->font();
    barFont.setPointSizeF(20);
    barFont.setBold(true);
    painter->setFont(barFont);
    painter->setPen(Qt::white);
    painter->drawText(bar, Qt::AlignCenter, sideLabel);

    // The identifying glyph, large enough to read across a room.
    QFont glyphFont = painter->font();
    glyphFont.setPointSizeF(GLYPH_SIZE_PT);
    glyphFont.setBold(true);
    painter->setFont(glyphFont);
    painter->setPen(Qt::black);

    QRectF glyphRect = pageRect;
    glyphRect.setTop(bar.bottom());
    painter->drawText(glyphRect, Qt::AlignCenter, glyph);

    // Caption under the glyph.
    QFont capFont = painter->font();
    capFont.setPointSizeF(13);
    capFont.setBold(false);
    painter->setFont(capFont);

    QRectF capRect = pageRect;
    capRect.setTop(pageRect.bottom() - 90);
    painter->drawText(capRect, Qt::AlignHCenter | Qt::AlignTop, caption);

    // A hairline round the printable area, so a sheet fed skew or cropped by an
    // unexpected margin is obvious rather than silently mismeasured.
    painter->setPen(QPen(Qt::black, 1, Qt::DashLine));
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(pageRect);

    painter->restore();
}


/************************************************

 ************************************************/
QString writeCalibrationPdf(const Printer *printer, int pass)
{
    if (!printer)
        return QString();

    const QString fileName = genTmpFileName(QString("-calibration%1.pdf").arg(pass));

    const QSizeF paper = printer->paperSize(UnitPoint);
    if (paper.isEmpty())
        return QString();

    QPdfWriter pdf(fileName);
    pdf.setCreator("PlicaPage");
    pdf.setTitle(QObject::tr("Double-sided calibration", "Title of the printed calibration test document"));

    // One painter unit is one PDF point, matching how the rest of the kernel
    // measures things.
    pdf.setResolution(72);
    pdf.setPageSize(QPageSize(paper, QPageSize::Point, QString(), QPageSize::ExactMatch));
    pdf.setPageMargins(QMarginsF(0, 0, 0, 0), QPageLayout::Point);

    QPainter painter;
    if (!painter.begin(&pdf))
        return QString();

    // Marks go inside the printable area, or a printer with generous hardware
    // margins would clip the very bar we are asking the user to look at.
    const QRectF pageRect = printer->pageRect();

    const bool second = (pass != 1);
    const QString sideLabel = second
            ? QObject::tr("SIDE 2", "Printed on the calibration sheet's second side")
            : QObject::tr("SIDE 1", "Printed on the calibration sheet's first side");

    const QString glyphs = second ? QStringLiteral("AB") : QStringLiteral("12");

    for (int i = 0; i < 2; ++i)
    {
        if (i > 0)
            pdf.newPage();

        const QString caption = second
                ? QObject::tr("PlicaPage calibration - second side",
                              "Caption printed on the calibration sheet")
                : QObject::tr("PlicaPage calibration - first side",
                              "Caption printed on the calibration sheet");

        drawSheet(&painter, pageRect, glyphs.mid(i, 1), sideLabel, caption);
    }

    painter.end();
    return fileName;
}
