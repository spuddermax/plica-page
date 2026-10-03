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


#ifndef PAPERSIZESDIALOG_H
#define PAPERSIZESDIALOG_H

#include <QDialog>
#include "kernel/papersizes.h"
#include "plicapagetypes.h"

class QTableWidget;

/************************************************
 * Edits the paper sizes PlicaPage offers to applications, and installs them
 * into the PlicaPage queue on OK.
 ************************************************/
class PaperSizesDialog : public QDialog
{
    Q_OBJECT
public:
    explicit PaperSizesDialog(QWidget *parent = nullptr);

    QList<UserPaperSize> sizes() const;

public slots:
    void accept() override;

private slots:
    void addRow(const UserPaperSize &size = UserPaperSize());
    void removeRows();

private:
    QTableWidget *mTable;
    Unit mUnit;
};

#endif // PAPERSIZESDIALOG_H
