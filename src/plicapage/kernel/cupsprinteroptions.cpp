/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL2+
 *
 *
 * Copyright: 2017 Boomaga team https://github.com/Boomaga
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


#include "cupsprinteroptions.h"
#include <cups/cups.h>
#include <cups/ppd.h>
#include <QFile>
#include <QStringList>

#define CUPS_DEVICE_URI                  "device-uri"

#ifndef CUPS_SIDES
#  define CUPS_SIDES                     "sides"
#endif


/************************************************
 * A PPD option that switches between grayscale and color, with the choice
 * names it uses for each. Several color choices may be listed: the first one
 * the PPD actually offers wins.
 *
 * ColorModel is the awkward one. Naming CMYK there hands the driver a raster
 * that CUPS has already separated into ink, without a profile to do it well,
 * which costs most of the color management and comes out dull. Drivers that
 * offer RGB would rather separate it themselves, so ask for that first and
 * keep CMYK for the ones that have nothing better.
 ************************************************/
struct ColorModeCase
{
    const char *option;
    const char *grayScaleChoice;
    const char *colorChoices[4];    // In preference order, terminated by a null.
};

static const ColorModeCase colorModeCases[] = {
    { "ColorModel",     "Gray",         { "RGB", "CMYK", "KCMY", 0 } },
    { "HPColorMode",    "grayscale",    { "colorsmart", 0, 0, 0 } },
    { "BRMonoColor",    "Mono",         { "FullColor", 0, 0, 0 } },     // Brother
    { "CNIJSGrayScale", "1",            { "0", 0, 0, 0 } },             //
    { "HPColorAsGray",  "True",         { "False", 0, 0, 0 } },         // HP
    { "XRColorMode",    "Black",        { "Color", 0, 0, 0 } },         // Xerox
};


/************************************************
 * Both options are left empty if the PPD has no option we recognise, and the
 * color one is left empty if the option we matched can only name grayscale.
 * An empty option is simply not passed to lpr, which leaves the PPD's own
 * default in place - a better guess than any choice we could invent.
 ************************************************/
void findGrayScaleOption(ppd_file_t *ppd, QString *grayScaleOption, QString *colorOption)
{
    // A case that names both modes beats one that only names grayscale, so
    // remember the first grayscale-only match and keep looking.
    QString grayScaleOnly;

    for (uint i=0; i<sizeof(colorModeCases)/sizeof(colorModeCases[0]); ++i)
    {
        const ColorModeCase &c = colorModeCases[i];

        ppd_option_t *option = ppdFindOption(ppd, c.option);
        if (!option || !ppdFindChoice(option, c.grayScaleChoice))
            continue;

        for (int j=0; j<4 && c.colorChoices[j]; ++j)
        {
            if (!ppdFindChoice(option, c.colorChoices[j]))
                continue;

            *grayScaleOption = QString("%1=%2").arg(c.option, c.grayScaleChoice);
            *colorOption     = QString("%1=%2").arg(c.option, c.colorChoices[j]);
            return;
        }

        if (grayScaleOnly.isEmpty())
            grayScaleOnly = QString("%1=%2").arg(c.option, c.grayScaleChoice);
    }

    *grayScaleOption = grayScaleOnly;
}


CupsPrinterOptions::CupsPrinterOptions(const QString &printerName):
    mDuplex(false),
    mPaperSize(QSizeF(0,0)),
    mLeftMargin(0),
    mRightMargin(0),
    mTopMargin(0),
    mBottomMargin(0)
{
    cups_dest_t *dests;
    int num_dests = cupsGetDests(&dests);
    cups_dest_t *dest = cupsGetDest(printerName.toLocal8Bit().data(),
                                    0, num_dests, dests);

    if (!dest)
        return;

#if 0
    qDebug() << "**" << mPrinterInfo.printerName() << "*******************";
    for (int j=0; j<dest->num_options; ++j)
        qDebug() << "  *" << dest->options[j].name << dest->options[j].value;
#endif



    mDeviceURI = QString(cupsGetOption(CUPS_DEVICE_URI, dest->num_options, dest->options));
    //QString duplexStr = cupsGetOption(CUPS_SIDES, dest->num_options, dest->options);

    mDuplex = false;
    mDuplex = mDuplex || QString(cupsGetOption(CUPS_SIDES, dest->num_options, dest->options)).toUpper().startsWith("TWO-");
    mDuplex = mDuplex || QString(cupsGetOption("Duplex", dest->num_options, dest->options)).toUpper().startsWith("DUPLEX-");
    mDuplex = mDuplex || QString(cupsGetOption("JCLDuplex", dest->num_options, dest->options)).toUpper().startsWith("DUPLEX-");
    mDuplex = mDuplex || QString(cupsGetOption("EFDuplex", dest->num_options, dest->options)).toUpper().startsWith("DUPLEX-");
    mDuplex = mDuplex || QString(cupsGetOption("KD03Duplex", dest->num_options, dest->options)).toUpper().startsWith("DUPLEX-");

    // Read values from PPD
    // The returned filename is stored in a static buffer
    const char * ppdFile = cupsGetPPD(printerName.toLocal8Bit().data());
    if (ppdFile != 0)
    {
        ppd_file_t *ppd = ppdOpenFile(ppdFile);
        if (ppd)
        {
            ppdMarkDefaults(ppd);

            ppd_size_t *size = ppdPageSize(ppd, 0);
            if (size)
            {
                mPaperSize    = QSizeF(size->width, size->length);
                mLeftMargin   = size->left;
                mRightMargin  = size->width - size->right;
                mTopMargin    = size->length - size->top;
                mBottomMargin = size->bottom;
            }

            mDuplex = mDuplex || ppdIsMarked(ppd, "Duplex",     "DuplexNoTumble");
            mDuplex = mDuplex || ppdIsMarked(ppd, "Duplex",     "DuplexTumble");
            mDuplex = mDuplex || ppdIsMarked(ppd, "JCLDuplex",  "DuplexNoTumble");
            mDuplex = mDuplex || ppdIsMarked(ppd, "JCLDuplex",  "DuplexTumble");
            mDuplex = mDuplex || ppdIsMarked(ppd, "EFDuplex",   "DuplexNoTumble");
            mDuplex = mDuplex || ppdIsMarked(ppd, "EFDuplex",   "DuplexTumble");
            mDuplex = mDuplex || ppdIsMarked(ppd, "KD03Duplex", "DuplexNoTumble");
            mDuplex = mDuplex || ppdIsMarked(ppd, "KD03Duplex", "DuplexTumble");


            // Grayscale options ..........................
            findGrayScaleOption(ppd, &mGrayScaleOption, &mColorOption);

            ppdClose(ppd);
        }
        QFile::remove(ppdFile);
    }


    bool ok;
    int n;

    n = QString(cupsGetOption("page-left", dest->num_options, dest->options)).toInt(&ok);
    if (ok)
        mLeftMargin = n;

    n = QString(cupsGetOption("page-right", dest->num_options, dest->options)).toInt(&ok);
    if (ok)
        mRightMargin = n;

    n = QString(cupsGetOption("page-top", dest->num_options, dest->options)).toInt(&ok);
    if (ok)
        mTopMargin = n;

    n = QString(cupsGetOption("page-bottom", dest->num_options, dest->options)).toInt(&ok);
    if (ok)
        mBottomMargin =n;

    cupsFreeDests(num_dests, dests);
}




