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
#include <QPolygonF>
#include <QStringList>


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


/************************************************
 * The rulers are labelled with the distance the PDF puts each mark from the
 * sheet edge. Measuring where the marks really land tells the user how far the
 * printer is off, and in which direction.
 ************************************************/
QString writeCenteringPdf(const Printer *printer, qreal offsetX, qreal offsetY)
{
    if (!printer)
        return QString();

    const QString fileName = genTmpFileName("-centering.pdf");
    const QSizeF paper = printer->paperSize(UnitPoint);
    if (paper.isEmpty())
        return QString();

    QPdfWriter pdf(fileName);
    pdf.setCreator("PlicaPage");
    pdf.setTitle(QObject::tr("Centring test page", "Title of the printed centring test document"));
    pdf.setResolution(72);
    pdf.setPageSize(QPageSize(paper, QPageSize::Point, QString(), QPageSize::ExactMatch));
    pdf.setPageMargins(QMarginsF(0, 0, 0, 0), QPageLayout::Point);

    QPainter painter;
    if (!painter.begin(&pdf))
        return QString();

    // QPainter's y runs down the page; the offset's runs up.
    painter.translate(offsetX, -offsetY);

    const qreal mm = 72.0 / 25.4;
    const qreal w = paper.width(), h = paper.height();
    const QPointF c(w / 2, h / 2);

    painter.setPen(QPen(Qt::black, 0.5));
    painter.drawLine(QPointF(c.x() - 40, c.y()), QPointF(c.x() + 40, c.y()));
    painter.drawLine(QPointF(c.x(), c.y() - 40), QPointF(c.x(), c.y() + 40));
    painter.setPen(QPen(Qt::black, 0.4));
    foreach (qreal r, QList<qreal>() << 5 * mm << 10 * mm)
    {
        QPolygonF d; d << QPointF(c.x() - r, c.y()) << QPointF(c.x(), c.y() - r)
                       << QPointF(c.x() + r, c.y()) << QPointF(c.x(), c.y() + r);
        painter.drawPolygon(d);
    }

    QFont small; small.setPointSizeF(6); painter.setFont(small);
    // Ticks every millimetre from 2 to 30 mm in from each edge, labelled every 5.
    for (int d = 2; d <= 30; ++d)
    {
        const qreal p = d * mm; const bool major = (d % 5 == 0); const qreal len = major ? 6 : 3;
        const QString lbl = QString::number(d);
        painter.drawLine(QPointF(p, c.y() - len), QPointF(p, c.y() + len));                  // left
        painter.drawLine(QPointF(w - p, c.y() - len), QPointF(w - p, c.y() + len));          // right
        painter.drawLine(QPointF(c.x() - len, p), QPointF(c.x() + len, p));                  // top
        painter.drawLine(QPointF(c.x() - len, h - p), QPointF(c.x() + len, h - p));          // bottom
        if (major)
        {
            painter.drawText(QPointF(p - 3, c.y() - 9), lbl);
            painter.drawText(QPointF(w - p - 3, c.y() - 9), lbl);
            painter.drawText(QPointF(c.x() + 9, p + 2), lbl);
            painter.drawText(QPointF(c.x() + 9, h - p + 2), lbl);
        }
    }
    painter.setPen(QPen(Qt::black, 0.3));
    painter.drawLine(QPointF(2 * mm, c.y()), QPointF(30 * mm, c.y()));
    painter.drawLine(QPointF(w - 30 * mm, c.y()), QPointF(w - 2 * mm, c.y()));
    painter.drawLine(QPointF(c.x(), 2 * mm), QPointF(c.x(), 30 * mm));
    painter.drawLine(QPointF(c.x(), h - 30 * mm), QPointF(c.x(), h - 2 * mm));

    QFont text; text.setPointSizeF(9); painter.setFont(text);
    const QStringList lines = QStringList()
        << QObject::tr("PlicaPage centring test page", "Caption on the centring test sheet")
        << QObject::tr("Each ruler is labelled with its distance in mm from the sheet edge as the PDF defines it.")
        << QObject::tr("Measure the real distance from the paper edge to the 10 mm mark on all four sides.")
        << QObject::tr("Horizontal offset = (right - left) / 2.   Vertical offset = (top - bottom) / 2.")
        << QObject::tr("Enter them under Margins > Print offset, then print this page again: all four should read 10.")
        << QObject::tr("Printed with offset X %1 pt, Y %2 pt.").arg(offsetX, 0, 'f', 1).arg(offsetY, 0, 'f', 1);
    qreal y = 60;
    foreach (const QString &l, lines) { painter.drawText(QPointF(40, y), l); y += 13; }

    painter.end();
    return fileName;
}
