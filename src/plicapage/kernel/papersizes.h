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


#ifndef PAPERSIZES_H
#define PAPERSIZES_H

#include <QList>
#include <QSizeF>
#include <QString>
#include <QStringList>

/************************************************
 * Paper sizes the user adds to the PlicaPage queue, so that applications
 * printing to it - LibreOffice's page styles, the system print dialogs - can
 * lay a document out at that size.
 *
 * Applications read their paper list from the queue's copy of the PPD. The
 * installed PPD is the template; the queue gets the template plus these.
 ************************************************/
struct UserPaperSize
{
    QString name;   ///< What applications show, e.g. "5 x 8.25 in"
    QSizeF  size;   ///< points

    /// The PPD keyword, e.g. "w360h594": CUPS's own pattern for a size
    /// given in points, and unique per size.
    QString keyword() const;

    bool operator==(const UserPaperSize &other) const
    { return name == other.name && size == other.size; }
};

/// The user's sizes, from the settings. Starts with 5 x 8.25 in, which the
/// PPD used to carry itself.
QList<UserPaperSize> userPaperSizes();
void setUserPaperSizes(const QList<UserPaperSize> &sizes);

/// A name safe for a PPD translation string; the size in inches if none.
QString ppdPaperName(const UserPaperSize &size);

/// The template PPD's text with the sizes added; sizes it already has are
/// left out.
QString ppdWithPaperSizes(const QString &ppd, const QList<UserPaperSize> &sizes);

/// Every CUPS queue printing to PlicaPage's backend.
QStringList plicaPageQueues();

/// Whether some queue's paper list differs from the template plus the sizes.
bool queuesNeedPaperSizes(const QList<UserPaperSize> &sizes);

/**
 * Gives every PlicaPage queue the template PPD plus the sizes, keeping its
 * default paper. Needs CUPS administration rights: membership of the
 * lpadmin group is enough on Debian and its derivatives.
 */
bool installPaperSizes(const QList<UserPaperSize> &sizes, QString *error);

#endif // PAPERSIZES_H
