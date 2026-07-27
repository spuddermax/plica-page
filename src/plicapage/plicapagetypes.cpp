/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL2+
 *
 *
 * Copyright: 2012-2013 Boomaga team https://github.com/Boomaga
 * Authors:
 *   Alexander Sokoloff <sokoloff.a@gmail.com>
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

#include "plicapagetypes.h"
#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <QUuid>

/************************************************

 ************************************************/
QString flipTypeToStr(FlipType value)
{
    switch (value)
    {
    case FlipType::LongEdge:    return "LongEdge";
    case FlipType::ShortEdge:   return "ShortEdge";
    }
    return "";
}


/************************************************

 ************************************************/
FlipType strToFlipType(const QString &str)
{
    QString s = str.toUpper();
    if (s.contains("SHORT"))    return FlipType::ShortEdge;
    return FlipType::LongEdge;
}


/************************************************

 ************************************************/
QString duplexTypeToStr(DuplexType value)
{
    switch (value)
    {
    case DuplexAuto:          return "Auto";
    case DuplexManual:        return "Manual";
    case DuplexManualReverse: return "ManualReverse";
    }
    return "";
}


/************************************************

 ************************************************/
DuplexType strToDuplexType(const QString &str)
{
    QString s = str.toUpper();
    if (s == "AUTO")        return DuplexAuto;
    if (s == "MANUAL")      return DuplexManual;
    return DuplexManualReverse;
}


/************************************************

 ************************************************/
QString colorModeToStr(ColorMode value)
{
    switch (value)
    {
    case ColorModeAuto:         return "Auto";
    case ColorModeGrayscale:    return "Grayscale";
    case ColorModeColor:        return "Color";
    }
    return "";
}


/************************************************

 ************************************************/
ColorMode strToColorMode(const QString &str)
{
    QString s = str.toUpper();
    if (s == "GRAYSCALE")  return ColorModeGrayscale;
    if (s == "GRAY")       return ColorModeGrayscale;
    if (s == "COLOR")      return ColorModeColor;
    return ColorModeAuto;
}


/************************************************
 * A PDF point is exactly 1/72 inch, and an inch is exactly 25.4 mm.
 * Using the exact ratios (rather than deriving them from rounded A4
 * dimensions) is what lets a value round-trip unchanged when the user
 * switches between millimeters and inches.
 ************************************************/
#define PT_PER_INCH 72.0
#define MM_PER_INCH 25.4


/************************************************

 ************************************************/
QString unitToStr(Unit value)
{
    switch (value)
    {
    case UnitMillimeter:    return "Millimeter";
    case UnitPoint:         return "Point";
    case UnitInch:          return "Inch";
    }
    return "";
}


/************************************************

 ************************************************/
Unit strToUnit(const QString &str)
{
    QString s = str.toUpper();
    if (s.startsWith("INCH"))   return UnitInch;
    if (s.startsWith("IN"))     return UnitInch;
    if (s.startsWith("POINT"))  return UnitPoint;
    if (s.startsWith("PT"))     return UnitPoint;
    return UnitMillimeter;
}


/************************************************
 * Points -> display unit.
 ************************************************/
qreal toUnit(qreal value, Unit unit)
{
    switch (unit)
    {
    case UnitPoint:         return value;
    case UnitMillimeter:    return value * (MM_PER_INCH / PT_PER_INCH);
    case UnitInch:          return value / PT_PER_INCH;
    }
    return 0;
}


/************************************************
 * Display unit -> points.
 ************************************************/
qreal fromUnit(qreal value, Unit unit)
{
    switch (unit)
    {
    case UnitPoint:         return value;
    case UnitMillimeter:    return value * (PT_PER_INCH / MM_PER_INCH);
    case UnitInch:          return value * PT_PER_INCH;
    }
    return 0;
}


/************************************************

 ************************************************/
QString unitSuffix(Unit unit)
{
    switch (unit)
    {
    case UnitMillimeter:    return QObject::tr("mm", "Length unit, millimeters");
    case UnitPoint:         return QObject::tr("pt", "Length unit, typographic points");
    case UnitInch:          return QObject::tr("in", "Length unit, inches");
    }
    return "";
}


/************************************************

 ************************************************/
int unitDecimals(Unit unit)
{
    switch (unit)
    {
    case UnitMillimeter:    return 2;
    case UnitPoint:         return 1;
    case UnitInch:          return 3;
    }
    return 2;
}


/************************************************

 ************************************************/
double unitStep(Unit unit)
{
    switch (unit)
    {
    case UnitMillimeter:    return 1.0;
    case UnitPoint:         return 1.0;
    case UnitInch:          return 0.125;
    }
    return 1.0;
}


/************************************************
 * Upper bound for a margin-sized length. The three values are the same
 * physical distance (~100mm), so switching units never truncates a value.
 ************************************************/
double unitMax(Unit unit)
{
    switch (unit)
    {
    case UnitMillimeter:    return 99.99;
    case UnitPoint:         return 288.0;
    case UnitInch:          return 4.0;
    }
    return 99.99;
}


/************************************************

 ************************************************/
QString safeFileName(const QString &str)
{
    QString res = str;
    res.replace('|', "-");
    res.replace('/', "-");
    res.replace('\\', "-");
    res.replace(':', "-");
    res.replace('*', "-");
    res.replace('?', "-");
    return res;
}


/************************************************

 ************************************************/
QString expandHomeDir(const QString &fileName)
{
    QString res = fileName;

    if (res.startsWith("~"))
        res.replace("~", QDir::homePath());

    return res;
}


/************************************************

 ************************************************/
QString shrinkHomeDir(const QString &fileName)
{
    QString res = fileName;

    if (res.startsWith(QDir::homePath()))
        res.replace(QDir::homePath(), "~");

    return res;

}


/************************************************
 *
 ************************************************/
QString plicapageChacheDir()
{
    QString res = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
    res = expandHomeDir(res);
    return res;
}


/************************************************
 *
 ************************************************/
void mustOpenFile(const QString &fileName, QFile *file)
{
    QFileInfo fi(fileName);

    if (fi.filePath().isEmpty())
        throw PlicaPageError(QObject::tr("I can't open file \"%1\" (Empty file name)",
                                       "Error message. %1 is a file name")
                           .arg(fi.filePath()));

    if (!fi.exists())
        throw PlicaPageError(QObject::tr("I can't open file \"%1\" (No such file or directory)",
                                       "Error message. %1 is a file name")
                           .arg(fi.filePath()));

    if (!fi.isReadable())
        throw PlicaPageError(QObject::tr("I can't open file \"%1\" (Access denied)",
                                       "Error message. %1 is a file name")
                           .arg(fi.filePath()));


    file->setFileName(fileName);
    if(!file->open(QFile::ReadOnly))
        throw PlicaPageError(QObject::tr("I can't open file \"%1\"",
                                       "Error message. %1 is a file name")
                           .arg(fi.filePath()) +
                           "\n" + file->errorString());
}


/************************************************
 *
 ************************************************/
QString appUUID()
{
    static QString res = "plicapage-" + QUuid::createUuid().toString().mid(1, 36);
    return res;
}


/************************************************
 *
 ************************************************/
QString genTmpFileName(const QString &suffix)
{
    static QAtomicInt num = 1;

    return QString("%1%2%3_%4%5")
            .arg(plicapageChacheDir())
            .arg(QDir::separator())
            .arg(appUUID())
            .arg(num.fetchAndAddRelaxed(1), 3, 10, QChar('0'))
            .arg(suffix);
}
