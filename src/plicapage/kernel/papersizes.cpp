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


#include "papersizes.h"
#include "ppdoptions.h"
#include "settings.h"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QTemporaryFile>
#include <QTextStream>
#include <cups/cups.h>


static const char *SETTINGS_KEY = "PaperSizes/User";


/************************************************

 ************************************************/
QString UserPaperSize::keyword() const
{
    return QString("w%1h%2").arg(qRound(size.width())).arg(qRound(size.height()));
}


/************************************************
 * Stored as "name<TAB>width<TAB>height", in points.
 ************************************************/
QList<UserPaperSize> userPaperSizes()
{
    QList<UserPaperSize> res;

    if (!settings->contains(SETTINGS_KEY))
    {
        UserPaperSize s;
        s.name = "5 x 8.25 in";
        s.size = QSizeF(360, 594);
        res << s;
        return res;
    }

    foreach (const QString &line, settings->QSettings::value(SETTINGS_KEY).toStringList())
    {
        const QStringList f = line.split('\t');
        if (f.count() != 3)
            continue;

        UserPaperSize s;
        s.name = f.at(0);
        s.size = QSizeF(f.at(1).toDouble(), f.at(2).toDouble());
        if (s.size.width() > 0 && s.size.height() > 0)
            res << s;
    }
    return res;
}


/************************************************
 * An empty list is stored too, so that removing every size is remembered
 * rather than bringing the starting one back.
 ************************************************/
void setUserPaperSizes(const QList<UserPaperSize> &sizes)
{
    QStringList lines;
    foreach (const UserPaperSize &s, sizes)
        lines << QString("%1\t%2\t%3").arg(s.name).arg(s.size.width()).arg(s.size.height());

    settings->QSettings::setValue(SETTINGS_KEY, lines);
    settings->sync();
}


/************************************************
 * The PPD is ISO Latin-1, and a translation string may not hold the
 * characters that delimit it.
 ************************************************/
QString ppdPaperName(const UserPaperSize &size)
{
    QString res;
    foreach (const QChar c, size.name.simplified())
    {
        if (c.unicode() < 32 || c.unicode() > 255 || c == ':' || c == '/' || c == '"')
            res += ' ';
        else
            res += c;
    }
    res = res.simplified().left(40);

    if (res.isEmpty())
        res = QString("%1 x %2 in").arg(size.size.width()  / 72.0)
                                    .arg(size.size.height() / 72.0);
    return res;
}


/************************************************

 ************************************************/
static QSet<QString> ppdSizeKeywords(const QString &ppd)
{
    QSet<QString> res;
    const QRegularExpression re("^\\*PaperDimension ([^/:\\s]+)", QRegularExpression::MultilineOption);
    auto it = re.globalMatch(ppd);
    while (it.hasNext())
        res << it.next().captured(1);
    return res;
}


/************************************************
 * Each size needs an entry in four places: the PageSize and PageRegion
 * options, and the ImageableArea and PaperDimension tables. Like the
 * template's own sizes, every one is imageable to the edge.
 ************************************************/
QString ppdWithPaperSizes(const QString &ppd, const QList<UserPaperSize> &sizes)
{
    const QSet<QString> existing = ppdSizeKeywords(ppd);

    QString pageSize, pageRegion, area, dimension;
    QSet<QString> added;
    foreach (const UserPaperSize &s, sizes)
    {
        const QString kw = s.keyword();
        if (existing.contains(kw) || added.contains(kw))
            continue;
        added << kw;

        const QString name = ppdPaperName(s);
        const QString w = QString::number(s.size.width(),  'f', 2);
        const QString h = QString::number(s.size.height(), 'f', 2);
        pageSize   += QString("*PageSize %1/%2: \"@PJL SET PAPER=CUSTOM<0A>\"\n").arg(kw, name);
        pageRegion += QString("*PageRegion %1/%2: \"@PJL SET PAPER=CUSTOM<0A>\"\n").arg(kw, name);
        area       += QString("*ImageableArea %1/%2: \"0 0 %3 %4\"\n").arg(kw, name, w, h);
        dimension  += QString("*PaperDimension %1/%2: \"%3 %4\"\n").arg(kw, name, w, h);
    }

    if (added.isEmpty())
        return ppd;

    QStringList lines = ppd.split('\n');
    auto insertBefore = [&lines](const QString &prefix, const QString &text)
    {
        for (int i = 0; i < lines.count(); ++i)
            if (lines.at(i).startsWith(prefix))
            {
                lines.insert(i, text.chopped(1));
                return;
            }
    };
    auto insertAfterLast = [&lines](const QString &prefix, const QString &text)
    {
        for (int i = lines.count() - 1; i >= 0; --i)
            if (lines.at(i).startsWith(prefix))
            {
                lines.insert(i + 1, text.chopped(1));
                return;
            }
    };

    insertBefore("*JCLCloseUI: *PageSize",   pageSize);
    insertBefore("*JCLCloseUI: *PageRegion", pageRegion);
    insertAfterLast("*ImageableArea ",  area);
    insertAfterLast("*PaperDimension ", dimension);

    return lines.join('\n');
}


/************************************************

 ************************************************/
QStringList plicaPageQueues()
{
    QStringList res;
    cups_dest_t *dests;
    const int count = cupsGetDests(&dests);
    for (int i = 0; i < count; ++i)
    {
        if (dests[i].instance)
            continue;

        const char *uri = cupsGetOption("device-uri", dests[i].num_options, dests[i].options);
        if (uri && QString(uri) == CUPS_BACKEND_URI)
            res << QString::fromLocal8Bit(dests[i].name);
    }
    cupsFreeDests(count, dests);
    return res;
}


/************************************************

 ************************************************/
static QString readTemplate(QString *error)
{
    QFile f(CUPS_PPD_TEMPLATE);
    if (!f.open(QFile::ReadOnly))
    {
        *error = QObject::tr("I can't read the PlicaPage printer description %1: %2")
                .arg(f.fileName(), f.errorString());
        return QString();
    }
    return QString::fromLatin1(f.readAll());
}


/************************************************

 ************************************************/
bool queuesNeedPaperSizes(const QList<UserPaperSize> &sizes)
{
    QString error;
    const QString ppd = readTemplate(&error);
    if (ppd.isEmpty())
        return false;

    const QSet<QString> expected = ppdSizeKeywords(ppdWithPaperSizes(ppd, sizes));
    foreach (const QString &queue, plicaPageQueues())
    {
        QSet<QString> actual;
        foreach (const PpdPaperSize &p, PpdOptions(queue).paperSizes())
            actual << p.keyword;
        if (actual != expected)
            return true;
    }
    return false;
}


/************************************************

 ************************************************/
bool installPaperSizes(const QList<UserPaperSize> &sizes, QString *error)
{
    const QString ppd = readTemplate(error);
    if (ppd.isEmpty())
        return false;

    const QString newPpd = ppdWithPaperSizes(ppd, sizes);
    const QSet<QString> newSizes = ppdSizeKeywords(newPpd);

    const QStringList queues = plicaPageQueues();
    if (queues.isEmpty())
    {
        *error = QObject::tr("There is no PlicaPage printer to give the sizes to.");
        return false;
    }

    QTemporaryFile file(QDir::tempPath() + "/plicapage-XXXXXX.ppd");
    if (!file.open())
    {
        *error = QObject::tr("I can't write a temporary file: %1").arg(file.errorString());
        return false;
    }
    file.write(newPpd.toLatin1());
    file.close();

    foreach (const QString &queue, queues)
    {
        // A new PPD brings its own defaults; keep the paper the queue had.
        QStringList args;
        args << "-p" << queue << "-P" << file.fileName();
        const QString def = PpdOptions(queue).defaultPaperSize();
        if (newSizes.contains(def))
            args << "-o" << "PageSize=" + def;

        QProcess proc;
        proc.start("lpadmin", args);
        proc.waitForFinished(30000);
        if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0)
        {
            // lpadmin also warns that drivers are deprecated; only a failure
            // is worth passing on.
            *error = QObject::tr("I can't update the printer %1: %2")
                    .arg(queue, QString::fromLocal8Bit(proc.readAllStandardError()).trimmed());
            return false;
        }
    }
    return true;
}
