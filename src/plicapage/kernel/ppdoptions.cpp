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

#include "ppdoptions.h"

#include <cups/cups.h>
#include <cups/ppd.h>
#include <QFile>
#include <QSet>
#include <QStringList>


/************************************************
 * Options a job must not set on its own, either because PlicaPage decides them
 * from the project (paper, duplex) or because the colour-mode combo already
 * covers them through findGrayScaleOption().
 ************************************************/
static const char *const managedOptions[] = {
    "PageSize", "PageRegion", "Duplex", "JCLDuplex", "EFDuplex", "KD03Duplex",
    "ColorModel", "HPColorMode", "BRMonoColor", "CNIJSGrayScale", "HPColorAsGray", "XRColorMode",
    0
};

/************************************************
 * Names drivers use for the quality choice, best first. cupsPrintQuality is the
 * standard one; Resolution is what Gutenprint actually keys its print modes on.
 ************************************************/
static const char *const qualityOptions[] = {
    "cupsPrintQuality", "Resolution", "OutputMode", "PrintQuality", "Quality",
    "HPPrintQuality", "StpQuality", 0
};


QString PpdOption::defaultText() const
{
    foreach (const PpdChoice &c, choices)
        if (c.keyword == defaultChoice)
            return c.text;
    return defaultChoice;
}


/************************************************

 ************************************************/
PpdOptions::PpdOptions(const QString &printerName):
    mValid(false)
{
    // The returned filename is stored in a static buffer
    const char *ppdFile = cupsGetPPD(printerName.toLocal8Bit().data());
    if (!ppdFile)
        return;

    ppd_file_t *ppd = ppdOpenFile(ppdFile);
    if (!ppd)
    {
        QFile::remove(ppdFile);
        return;
    }

    ppdMarkDefaults(ppd);

    QSet<QString> managed;
    for (int i = 0; managedOptions[i]; ++i)
        managed.insert(managedOptions[i]);

    for (int g = 0; g < ppd->num_groups; ++g)
    {
        const ppd_group_t &group = ppd->groups[g];
        // Installable options describe the hardware, not the job.
        if (QString(group.name).compare("InstallableOptions", Qt::CaseInsensitive) == 0)
            continue;

        for (int o = 0; o < group.num_options; ++o)
        {
            const ppd_option_t &opt = group.options[o];
            if (opt.ui != PPD_UI_PICKONE && opt.ui != PPD_UI_BOOLEAN)
                continue;
            if (managed.contains(opt.keyword))
                continue;

            PpdOption option;
            option.keyword = opt.keyword;
            option.text    = QString::fromUtf8(opt.text).isEmpty() ? QString(opt.keyword) : QString::fromUtf8(opt.text);
            option.group   = QString::fromUtf8(group.text);
            option.defaultChoice = opt.defchoice;

            for (int c = 0; c < opt.num_choices; ++c)
            {
                const ppd_choice_t &ch = opt.choices[c];
                // A "Custom.*" entry is a template, not something a combo can offer.
                if (QString(ch.choice).startsWith("Custom."))
                    continue;
                PpdChoice choice;
                choice.keyword = ch.choice;
                choice.text    = QString::fromUtf8(ch.text).isEmpty() ? QString(ch.choice) : QString::fromUtf8(ch.text);
                option.choices << choice;
            }

            if (option.choices.count() < 2)
                continue;

            mOptions << option;
        }
    }

    // Paper sizes: dimensions come from the size table, the display text from
    // the PageSize option's choices.
    if (ppd_option_t *pageSize = ppdFindOption(ppd, "PageSize"))
    {
        mDefaultPaperSize = pageSize->defchoice;
        for (int i = 0; i < ppd->num_sizes; ++i)
        {
            const ppd_size_t &sz = ppd->sizes[i];
            if (QString(sz.name).startsWith("Custom"))
                continue;
            PpdPaperSize p;
            p.keyword = sz.name;
            p.text    = sz.name;
            if (ppd_choice_t *ch = ppdFindChoice(pageSize, sz.name))
                if (ch->text[0])
                    p.text = QString::fromUtf8(ch->text);
            p.size   = QSizeF(sz.width, sz.length);
            p.left   = sz.left;
            p.right  = sz.width - sz.right;
            p.top    = sz.length - sz.top;
            p.bottom = sz.bottom;
            mPaperSizes << p;
        }
    }

    ppdClose(ppd);
    QFile::remove(ppdFile);
    mValid = true;

    for (int i = 0; qualityOptions[i] && mQualityKeyword.isEmpty(); ++i)
        foreach (const PpdOption &option, mOptions)
            if (option.keyword == qualityOptions[i])
            {
                mQualityKeyword = option.keyword;
                break;
            }
}
