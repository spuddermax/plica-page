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


#include "papersizesdialog.h"

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

enum Column { ColName, ColWidth, ColHeight };

// The PPD's custom-size range, which these stay inside: 1 in to 200 in.
static const qreal MIN_SIZE = 72;
static const qreal MAX_SIZE = 14400;


/************************************************

 ************************************************/
PaperSizesDialog::PaperSizesDialog(QWidget *parent):
    QDialog(parent),
    mUnit(currentUnit())
{
    setWindowTitle(tr("Paper sizes for applications"));

    QVBoxLayout *layout = new QVBoxLayout(this);

    QLabel *intro = new QLabel(
        tr("These sizes are offered to applications printing to PlicaPage, so a document "
           "can be laid out at one of them - in LibreOffice Writer under the PlicaPage "
           "printer's properties, or in a print dialog's paper size list. "
           "An application that is already open lists them after a restart."), this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    mTable = new QTableWidget(0, 3, this);
    mTable->setHorizontalHeaderLabels(QStringList()
                                      << tr("Name")
                                      << tr("Width (%1)").arg(unitSuffix(mUnit))
                                      << tr("Height (%1)").arg(unitSuffix(mUnit)));
    mTable->horizontalHeader()->setSectionResizeMode(ColName, QHeaderView::Stretch);
    mTable->verticalHeader()->hide();
    mTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    layout->addWidget(mTable);

    QHBoxLayout *rowButtons = new QHBoxLayout();
    QPushButton *add    = new QPushButton(tr("Add"), this);
    QPushButton *remove = new QPushButton(tr("Remove"), this);
    rowButtons->addWidget(add);
    rowButtons->addWidget(remove);
    rowButtons->addStretch();
    layout->addLayout(rowButtons);
    connect(add,    &QPushButton::clicked, this, [this]() { addRow(); });
    connect(remove, &QPushButton::clicked, this, &PaperSizesDialog::removeRows);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &PaperSizesDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &PaperSizesDialog::reject);
    layout->addWidget(buttons);

    foreach (const UserPaperSize &s, userPaperSizes())
        addRow(s);

    resize(520, 360);
}


/************************************************
 * A new row starts from the size of the one before, which is usually what is
 * being varied.
 ************************************************/
void PaperSizesDialog::addRow(const UserPaperSize &size)
{
    UserPaperSize s = size;
    if (s.size.isEmpty())
    {
        const QList<UserPaperSize> current = sizes();
        s.size = current.isEmpty() ? QSizeF(360, 594) : current.last().size;
    }

    const int row = mTable->rowCount();
    mTable->insertRow(row);
    mTable->setItem(row, ColName, new QTableWidgetItem(s.name));

    auto spin = [this](qreal points)
    {
        QDoubleSpinBox *box = new QDoubleSpinBox(mTable);
        box->setDecimals(unitDecimals(mUnit));
        box->setSingleStep(unitStep(mUnit));
        box->setRange(toUnit(MIN_SIZE, mUnit), toUnit(MAX_SIZE, mUnit));
        box->setValue(toUnit(points, mUnit));
        box->setFrame(false);
        return box;
    };
    mTable->setCellWidget(row, ColWidth,  spin(s.size.width()));
    mTable->setCellWidget(row, ColHeight, spin(s.size.height()));

    if (size.size.isEmpty())
        mTable->editItem(mTable->item(row, ColName));
}


/************************************************

 ************************************************/
void PaperSizesDialog::removeRows()
{
    QList<int> rows;
    foreach (const QModelIndex &index, mTable->selectionModel()->selectedRows())
        rows << index.row();
    if (rows.isEmpty() && mTable->currentRow() >= 0)
        rows << mTable->currentRow();

    std::sort(rows.begin(), rows.end(), std::greater<int>());
    foreach (int row, rows)
        mTable->removeRow(row);
}


/************************************************
 * A row without a name is named after its size, as applications will
 * show it.
 ************************************************/
QList<UserPaperSize> PaperSizesDialog::sizes() const
{
    QList<UserPaperSize> res;
    for (int row = 0; row < mTable->rowCount(); ++row)
    {
        UserPaperSize s;
        s.name = mTable->item(row, ColName) ? mTable->item(row, ColName)->text().simplified() : QString();
        s.size = QSizeF(fromUnit(qobject_cast<QDoubleSpinBox*>(mTable->cellWidget(row, ColWidth))->value(),  mUnit),
                        fromUnit(qobject_cast<QDoubleSpinBox*>(mTable->cellWidget(row, ColHeight))->value(), mUnit));
        if (s.name.isEmpty())
            s.name = ppdPaperName(s);
        res << s;
    }
    return res;
}


/************************************************

 ************************************************/
void PaperSizesDialog::accept()
{
    const QList<UserPaperSize> list = sizes();

    // Two rows of the same size would be one size to the PPD.
    for (int i = 0; i < list.count(); ++i)
        for (int j = i + 1; j < list.count(); ++j)
            if (list.at(i).keyword() == list.at(j).keyword())
            {
                QMessageBox::warning(this, windowTitle(),
                    tr("\"%1\" and \"%2\" are the same size. Remove one of them, "
                       "or change its size.").arg(list.at(i).name, list.at(j).name));
                return;
            }

    setUserPaperSizes(list);

    QString error;
    if (!installPaperSizes(list, &error))
    {
        QMessageBox::warning(this, windowTitle(),
            tr("The sizes are saved, but the PlicaPage printer could not be updated:\n\n%1")
            .arg(error));
        return;
    }

    QMessageBox::information(this, windowTitle(),
        tr("The PlicaPage printer now offers these sizes. Restart any application "
           "that was already open, such as LibreOffice, to see them."));
    QDialog::accept();
}
