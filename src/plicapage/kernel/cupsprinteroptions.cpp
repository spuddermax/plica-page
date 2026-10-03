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
 * Both options are left empty if the PPD has no option we recognize, and the
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

/************************************************
 * PPD options that select two-sided printing. Duplex is the standard one; the
 * rest are what some vendors' drivers call it.
 ************************************************/
static const char *const duplexOptions[] = {
    "Duplex", "JCLDuplex", "EFDuplex", "KD03Duplex", 0
};


/************************************************
 * Whether the hardware the queue says is installed rules this choice out.
 *
 * A PPD describes a model, not a machine: a duplexer that is an accessory is
 * an installable option, and a UIConstraints line forbids the two-sided
 * choices while it is marked as absent. Only those constraints count here -
 * one against a paper size or media type says the duplexer cannot take that
 * paper, not that there is none.
 ************************************************/
static bool ruledOutByHardware(ppd_file_t *ppd, const char *option, const char *choice)
{
    for (int i = 0; i < ppd->num_consts; ++i)
    {
        const ppd_const_t &c = ppd->consts[i];

        // A constraint names two option/choice pairs, in either order.
        for (int side = 0; side < 2; ++side)
        {
            const char *ownOption   = side ? c.option2 : c.option1;
            const char *ownChoice   = side ? c.choice2 : c.choice1;
            const char *otherOption = side ? c.option1 : c.option2;
            const char *otherChoice = side ? c.choice1 : c.choice2;

            // No choice named means every choice but None and False.
            if (qstricmp(ownOption, option) != 0)
                continue;
            if (ownChoice[0] && qstricmp(ownChoice, choice) != 0)
                continue;

            bool installable = false;
            for (int g = 0; g < ppd->num_groups && !installable; ++g)
            {
                const ppd_group_t &group = ppd->groups[g];
                if (qstricmp(group.name, "InstallableOptions") != 0)
                    continue;
                for (int o = 0; o < group.num_options && !installable; ++o)
                    installable = qstricmp(group.options[o].keyword, otherOption) == 0;
            }
            if (!installable)
                continue;

            ppd_choice_t *marked = ppdFindMarkedChoice(ppd, otherOption);
            if (!marked)
                continue;

            if (otherChoice[0])
            {
                if (qstricmp(marked->choice, otherChoice) == 0)
                    return true;
            }
            else if (qstricmp(marked->choice, "None") != 0 && qstricmp(marked->choice, "False") != 0)
            {
                return true;
            }
        }
    }
    return false;
}


/************************************************
 * Whether the printer can turn the paper over itself: its PPD offers a
 * two-sided choice, and the installed hardware allows it.
 *
 * What the queue prints by default says nothing about this. Most duplex
 * printers are set up one-sided, so asking whether a two-sided choice is the
 * marked one - all this used to do - misses nearly all of them.
 ************************************************/
static bool ppdCanDuplex(ppd_file_t *ppd)
{
    for (int i = 0; duplexOptions[i]; ++i)
    {
        ppd_option_t *option = ppdFindOption(ppd, duplexOptions[i]);
        if (!option)
            continue;

        for (int c = 0; c < option->num_choices; ++c)
        {
            const char *choice = option->choices[c].choice;
            // DuplexNoTumble, DuplexTumble and vendors' spellings of them.
            if (!QString(choice).startsWith("Duplex", Qt::CaseInsensitive))
                continue;
            if (!ruledOutByHardware(ppd, option->keyword, choice))
                return true;
        }
    }
    return false;
}


bool ppdFileCanDuplex(const QString &ppdFileName)
{
    ppd_file_t *ppd = ppdOpenFile(ppdFileName.toLocal8Bit().data());
    if (!ppd)
        return false;

    ppdMarkDefaults(ppd);
    const bool res = ppdCanDuplex(ppd);
    ppdClose(ppd);
    return res;
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

    // A queue that prints two-sided by default has said it can.
    mDuplex = QString(cupsGetOption(CUPS_SIDES, dest->num_options, dest->options)).toUpper().startsWith("TWO-");
    for (int i = 0; duplexOptions[i]; ++i)
        mDuplex = mDuplex || QString(cupsGetOption(duplexOptions[i], dest->num_options, dest->options)).toUpper().startsWith("DUPLEX");

    // Read values from PPD
    // The returned filename is stored in a static buffer
    const char * ppdFile = cupsGetPPD(printerName.toLocal8Bit().data());
    bool ppdRead = false;
    if (ppdFile != 0)
    {
        ppd_file_t *ppd = ppdOpenFile(ppdFile);
        if (ppd)
        {
            ppdRead = true;
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

            mDuplex = mDuplex || ppdCanDuplex(ppd);


            // Grayscale options ..........................
            findGrayScaleOption(ppd, &mGrayScaleOption, &mColorOption);

            ppdClose(ppd);
        }
        QFile::remove(ppdFile);
    }

    // No PPD to read: ask the queue itself what it supports.
    if (!ppdRead && !mDuplex)
    {
        cups_dinfo_t *info = cupsCopyDestInfo(CUPS_HTTP_DEFAULT, dest);
        if (info)
        {
            mDuplex = cupsCheckDestSupported(CUPS_HTTP_DEFAULT, dest, info, CUPS_SIDES, CUPS_SIDES_TWO_SIDED_PORTRAIT) ||
                      cupsCheckDestSupported(CUPS_HTTP_DEFAULT, dest, info, CUPS_SIDES, CUPS_SIDES_TWO_SIDED_LANDSCAPE);
            cupsFreeDestInfo(info);
        }
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




