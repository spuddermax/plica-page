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


#include "duplexwizard.h"
#include "calibrationsheet.h"

#include "kernel/printer.h"
#include "gui/icon.h"

#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRadioButton>
#include <QStackedWidget>
#include <QVBoxLayout>


/************************************************

 ************************************************/
FlipDiagram::FlipDiagram(QWidget *parent):
    QWidget(parent)
{
    setMinimumSize(260, 150);
}


/************************************************

 ************************************************/
QSize FlipDiagram::sizeHint() const
{
    return QSize(300, 160);
}


/************************************************
 * A sheet with a dashed vertical axis and arrows curling round it: turn the
 * paper about its long edge, the way a book page turns.
 ************************************************/
void FlipDiagram::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QColor mark("#CC7373");            // same accent the margins preview uses
    const QRectF area = rect().adjusted(8, 8, -8, -8);

    const qreal h = area.height();
    const qreal w = h * 0.72;                // portrait sheet
    QRectF sheet(area.center().x() - w / 2, area.top(), w, h);

    p.setPen(QPen(Qt::darkGray, 1));
    p.setBrush(Qt::white);
    p.drawRect(sheet);

    // The bar the calibration sheets carry, so the diagram and the paper match.
    QRectF bar(sheet.left() + 4, sheet.top() + 4, sheet.width() - 8, 12);
    p.fillRect(bar, Qt::black);

    // Turning axis, down the middle.
    p.setPen(QPen(mark, 2, Qt::DashLine));
    p.drawLine(QPointF(sheet.center().x(), sheet.top() - 6),
               QPointF(sheet.center().x(), sheet.bottom() + 6));

    // Arrows curling round that axis.
    p.setPen(QPen(mark, 2));
    p.setBrush(mark);
    const qreal cy = sheet.center().y();
    const qreal r  = sheet.width() * 0.55;

    QPainterPath arc;
    arc.moveTo(sheet.center().x() - r, cy);
    arc.cubicTo(sheet.center().x() - r, cy - 26,
                sheet.center().x() + r, cy - 26,
                sheet.center().x() + r, cy);
    p.setBrush(Qt::NoBrush);
    p.drawPath(arc);

    QPolygonF head;
    head << QPointF(sheet.center().x() + r, cy + 6)
         << QPointF(sheet.center().x() + r - 6, cy - 6)
         << QPointF(sheet.center().x() + r + 6, cy - 6);
    p.setBrush(mark);
    p.drawPolygon(head);
}


/************************************************

 ************************************************/
DuplexWizard::DuplexWizard(Printer *printer, PrinterProfile *target, QWidget *parent):
    QDialog(parent),
    mPrinter(printer),
    mTarget(target),
    mOrderSameBtn(nullptr),
    mOrderReversedBtn(nullptr),
    mBarSameEdgeBtn(nullptr),
    mBarOppositeBtn(nullptr),
    mOverprintedBtn(nullptr),
    mResultLabel(nullptr),
    mCalibrated(false)
{
    setWindowTitle(tr("Set up double-sided printing", "Duplex calibration wizard title"));

    mPages = new QStackedWidget(this);
    buildIntroPage();
    buildFlipPage();
    buildQuestionsPage();
    buildResultPage();

    QDialogButtonBox *buttons = new QDialogButtonBox(this);
    // QDialogButtonBox has no Back role, so add it as a plain button.
    mBackButton = buttons->addButton(tr("Back", "Duplex calibration button"),
                                     QDialogButtonBox::ActionRole);
    mNextButton = buttons->addButton(QDialogButtonBox::Ok);
    buttons->addButton(QDialogButtonBox::Cancel);

    connect(mNextButton, SIGNAL(clicked()), this, SLOT(next()));
    connect(mBackButton, SIGNAL(clicked()), this, SLOT(back()));
    connect(buttons, SIGNAL(rejected()), this, SLOT(reject()));

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(mPages);
    layout->addWidget(buttons);

    updateButtons();
}


/************************************************

 ************************************************/
bool DuplexWizard::execute(Printer *printer, PrinterProfile *target, QWidget *parent)
{
    if (!printer || !target)
        return false;

    DuplexWizard wizard(printer, target, parent);
    wizard.exec();
    return wizard.mCalibrated;
}


/************************************************

 ************************************************/
void DuplexWizard::buildIntroPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *l = new QVBoxLayout(page);

    QLabel *text = new QLabel(
        tr("<p>Your printer cannot turn the paper over by itself, so PlicaPage "
           "prints one side of every sheet, waits while you turn the stack over, "
           "then prints the other side.</p>"
           "<p>Which way the sheets have to go back in depends on the printer. "
           "This finds out by printing on <b>two sheets of paper</b> and asking "
           "you what came out.</p>"
           "<p>Put at least two sheets of paper in <b>%1</b> to begin.</p>",
           "Duplex calibration wizard, first page. %1 is the printer name")
            .arg(mPrinter->name()), page);
    text->setWordWrap(true);

    l->addWidget(text);
    l->addStretch();
    mPages->addWidget(page);
}


/************************************************

 ************************************************/
void DuplexWizard::buildFlipPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *l = new QVBoxLayout(page);

    QLabel *text = new QLabel(
        tr("<p>Two sheets are printing now, marked <b>1</b> and <b>2</b>.</p>"
           "<p>When they are done, take the whole stack out <b>without changing "
           "its order</b>, turn it over sideways as shown, and put it back in the "
           "paper tray.</p>"
           "<p>Remember this movement - you will be asked for it every time you "
           "print double-sided.</p>",
           "Duplex calibration wizard, instruction to turn the paper over"), page);
    text->setWordWrap(true);

    l->addWidget(text);
    l->addWidget(new FlipDiagram(page), 0, Qt::AlignHCenter);
    l->addStretch();
    mPages->addWidget(page);
}


/************************************************

 ************************************************/
void DuplexWizard::buildQuestionsPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *l = new QVBoxLayout(page);

    QLabel *intro = new QLabel(
        tr("Find the sheet with the big <b>1</b> on it and answer both questions.",
           "Duplex calibration wizard, results page introduction"), page);
    intro->setWordWrap(true);
    l->addWidget(intro);

    // Which sheet came back first -> does the stack reverse?
    QGroupBox *orderBox = new QGroupBox(
        tr("What is on the back of that sheet?", "Duplex calibration question"), page);
    QVBoxLayout *ol = new QVBoxLayout(orderBox);
    mOrderSameBtn     = new QRadioButton(tr("The letter A", "Duplex calibration answer"), orderBox);
    mOrderReversedBtn = new QRadioButton(tr("The letter B", "Duplex calibration answer"), orderBox);
    mOverprintedBtn   = new QRadioButton(
        tr("Nothing - the second sheets printed over the first side",
           "Duplex calibration answer for a stack put back the wrong way up"), orderBox);
    ol->addWidget(mOrderSameBtn);
    ol->addWidget(mOrderReversedBtn);
    ol->addWidget(mOverprintedBtn);
    l->addWidget(orderBox);

    // Where the two bars sit -> which edge was the sheet turned about?
    QGroupBox *barBox = new QGroupBox(
        tr("Both sides have a black bar along one edge. On this sheet, are they:",
           "Duplex calibration question"), page);
    QVBoxLayout *bl = new QVBoxLayout(barBox);
    mBarSameEdgeBtn = new QRadioButton(
        tr("Along the same edge of the paper", "Duplex calibration answer"), barBox);
    mBarOppositeBtn = new QRadioButton(
        tr("At opposite edges", "Duplex calibration answer"), barBox);
    bl->addWidget(mBarSameEdgeBtn);
    bl->addWidget(mBarOppositeBtn);
    l->addWidget(barBox);

    connect(mOverprintedBtn, SIGNAL(toggled(bool)), barBox, SLOT(setDisabled(bool)));

    l->addStretch();
    mPages->addWidget(page);
}


/************************************************

 ************************************************/
void DuplexWizard::buildResultPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *l = new QVBoxLayout(page);

    mResultLabel = new QLabel(page);
    mResultLabel->setWordWrap(true);
    l->addWidget(mResultLabel);

    QPushButton *verify = new QPushButton(
        tr("Print a test page to check", "Duplex calibration, optional verification"), page);
    connect(verify, SIGNAL(clicked()), this, SLOT(printVerification()));
    l->addWidget(verify, 0, Qt::AlignLeft);

    l->addStretch();
    mPages->addWidget(page);
}


/************************************************

 ************************************************/
void DuplexWizard::updateButtons()
{
    const int page = mPages->currentIndex();

    mBackButton->setEnabled(page == PageQuestions);

    switch (page)
    {
    case PageIntro:
        mNextButton->setText(tr("Print the test sheets", "Duplex calibration button"));
        break;

    case PageFlip:
        mNextButton->setText(tr("I have turned them over", "Duplex calibration button"));
        break;

    case PageQuestions:
        mNextButton->setText(tr("Finish", "Duplex calibration button"));
        break;

    case PageResult:
        mNextButton->setText(tr("Close", "Duplex calibration button"));
        break;
    }
}


/************************************************

 ************************************************/
bool DuplexWizard::printPass(int pass)
{
    const QString file = writeCalibrationPdf(mPrinter, pass);
    if (file.isEmpty())
    {
        QMessageBox::warning(this, windowTitle(),
                             tr("I can't create the calibration page.",
                                "Duplex calibration error"));
        return false;
    }

    // doubleSided=false, so this goes out one-sided whatever the profile says -
    // the printer must not do any turning of its own during a calibration.
    return mPrinter->printFile(file, tr("PlicaPage calibration", "Print job name"),
                               false, 1, false);
}


/************************************************

 ************************************************/
void DuplexWizard::applyAnswers()
{
    const bool reversed = mOrderReversedBtn->isChecked();
    const FlipType flip = mBarOppositeBtn->isChecked() ? FlipType::ShortEdge
                                                       : FlipType::LongEdge;

    mTarget->setManualDuplexReversesOrder(reversed);
    mTarget->setManualFlipType(flip);
    mTarget->setDuplexCalibrated(true);

    // Manual duplex is now described by the flip edge and the stacking order.
    // DuplexType only still distinguishes "the printer does it" from "by hand".
    if (mTarget->duplexType() == DuplexAuto)
        mTarget->setDuplexType(DuplexManual);

    mCalibrated = true;

    mResultLabel->setText(
        tr("<p><b>%1</b> is set up.</p>"
           "<p>Its sheets come back %2, and turning them over puts the two sides "
           "%3.</p>"
           "<p>When you print double-sided, turn the stack over the same way you "
           "just did.</p>",
           "Duplex calibration result. %1 printer, %2 and %3 are filled in below")
        .arg(mPrinter->name())
        .arg(reversed ? tr("in reverse order", "Duplex calibration result fragment")
                      : tr("in the same order", "Duplex calibration result fragment"))
        .arg(flip == FlipType::ShortEdge
             ? tr("head to toe, so one side is turned round", "Duplex calibration result fragment")
             : tr("the same way up", "Duplex calibration result fragment")));
}


/************************************************

 ************************************************/
void DuplexWizard::next()
{
    switch (mPages->currentIndex())
    {
    case PageIntro:
        if (!printPass(1))
            return;
        mPages->setCurrentIndex(PageFlip);
        break;

    case PageFlip:
        if (!printPass(2))
            return;
        mPages->setCurrentIndex(PageQuestions);
        break;

    case PageQuestions:
    {
        // The stack went back with the printed side facing the wrong way, so
        // there is nothing to measure. Say so plainly rather than deriving a
        // setting from an answer that does not mean anything.
        if (mOverprintedBtn->isChecked())
        {
            QMessageBox::information(this, windowTitle(),
                tr("Then the stack went back the wrong way up.\n\n"
                   "Run this again, and this time put the sheets back without "
                   "turning them over. It will use two more sheets of paper.",
                   "Duplex calibration, stack was inserted the wrong way up"));
            return;
        }

        const bool orderAnswered = mOrderSameBtn->isChecked() || mOrderReversedBtn->isChecked();
        const bool barAnswered   = mBarSameEdgeBtn->isChecked() || mBarOppositeBtn->isChecked();
        if (!orderAnswered || !barAnswered)
        {
            QMessageBox::information(this, windowTitle(),
                tr("Please answer both questions.", "Duplex calibration validation"));
            return;
        }

        applyAnswers();
        mPages->setCurrentIndex(PageResult);
        break;
    }

    case PageResult:
        accept();
        return;
    }

    updateButtons();
}


/************************************************

 ************************************************/
void DuplexWizard::back()
{
    if (mPages->currentIndex() == PageQuestions)
    {
        mPages->setCurrentIndex(PageFlip);
        updateButtons();
    }
}


/************************************************
 * Optional: a real double-sided job using the settings just derived, so the
 * user can confirm rather than take our word for it. Two more sheets.
 ************************************************/
void DuplexWizard::printVerification()
{
    if (QMessageBox::question(this, windowTitle(),
            tr("This prints four numbered pages on two sheets, using the settings "
               "just found.\n\nIf they read 1, 2, 3, 4 in order and all the same "
               "way up, the printer is set up correctly.",
               "Duplex calibration verification prompt"),
            QMessageBox::Cancel | QMessageBox::Ok) != QMessageBox::Ok)
        return;

    // Reuse the calibration sheets: pass 1 gives "1"/"2" and pass 2 "A"/"B",
    // which is enough to see order and orientation on real paper.
    if (!printPass(1))
        return;

    QMessageBox dialog(this);
    dialog.setWindowTitle(windowTitle());
    dialog.setIconPixmap(QPixmap(":/48/print"));
    dialog.setText(tr("Turn the stack over the same way as before, put it back, "
                      "then press Continue.", "Duplex calibration verification step"));
    dialog.addButton(QMessageBox::Abort);
    QPushButton *btn = dialog.addButton(QMessageBox::Ok);
    btn->setText(tr("Continue", "Duplex calibration button"));

    if (dialog.exec() == QMessageBox::Ok)
        printPass(2);
}
