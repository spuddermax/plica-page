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
    QWidget(parent),
    mHandling(HandlingFlipSideways)
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

 ************************************************/
void FlipDiagram::setHandling(ManualDuplexHandling handling)
{
    if (mHandling == handling)
        return;

    mHandling = handling;
    update();
}


/************************************************
 * A sheet with the axis it is turned about and an arrow curling round that
 * axis, or a plain down arrow when it is not turned at all.
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

    const QPointF c = sheet.center();

    if (mHandling == HandlingNoFlip)
    {
        // Nothing to turn about: just show it going straight back down.
        p.setPen(QPen(mark, 2));
        p.setBrush(Qt::NoBrush);
        p.drawLine(QPointF(c.x(), sheet.top() + 26), QPointF(c.x(), sheet.bottom() - 16));

        QPolygonF head;
        head << QPointF(c.x(), sheet.bottom() - 4)
             << QPointF(c.x() - 7, sheet.bottom() - 18)
             << QPointF(c.x() + 7, sheet.bottom() - 18);
        p.setBrush(mark);
        p.drawPolygon(head);
        return;
    }

    const bool sideways = (mHandling == HandlingFlipSideways);

    // The turning axis: down the middle for a sideways turn, across the middle
    // for an end-over-end one.
    p.setPen(QPen(mark, 2, Qt::DashLine));
    if (sideways)
        p.drawLine(QPointF(c.x(), sheet.top() - 6), QPointF(c.x(), sheet.bottom() + 6));
    else
        p.drawLine(QPointF(sheet.left() - 6, c.y()), QPointF(sheet.right() + 6, c.y()));

    // An arrow curling round it.
    p.setPen(QPen(mark, 2));
    p.setBrush(Qt::NoBrush);

    QPainterPath arc;
    QPolygonF head;

    if (sideways)
    {
        const qreal r = sheet.width() * 0.55;
        arc.moveTo(c.x() - r, c.y());
        arc.cubicTo(c.x() - r, c.y() - 26, c.x() + r, c.y() - 26, c.x() + r, c.y());
        head << QPointF(c.x() + r, c.y() + 6)
             << QPointF(c.x() + r - 6, c.y() - 6)
             << QPointF(c.x() + r + 6, c.y() - 6);
    }
    else
    {
        const qreal r = sheet.height() * 0.55;
        arc.moveTo(c.x(), c.y() - r);
        arc.cubicTo(c.x() - 26, c.y() - r, c.x() - 26, c.y() + r, c.x(), c.y() + r);
        head << QPointF(c.x() + 6, c.y() + r)
             << QPointF(c.x() - 6, c.y() + r - 6)
             << QPointF(c.x() - 6, c.y() + r + 6);
    }

    p.drawPath(arc);
    p.setBrush(mark);
    p.drawPolygon(head);
}


/************************************************

 ************************************************/
QString manualDuplexInstruction(ManualDuplexHandling handling)
{
    switch (handling)
    {
    case HandlingFlipSideways:
        return QObject::tr("Turn the stack over sideways, the way you would close a book.",
                           "Manual duplex handling, flip about the long edge");

    case HandlingFlipEndOver:
        return QObject::tr("Turn the stack over end for end, bringing the bottom edge to the top.",
                           "Manual duplex handling, flip about the short edge");

    case HandlingNoFlip:
        return QObject::tr("Put the stack straight back without turning it over.",
                           "Manual duplex handling, no flip");
    }
    return QString();
}


/************************************************

 ************************************************/
bool showManualDuplexPrompt(const Printer *printer, QWidget *parent)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(parent ? parent->windowTitle() + " " : QString());

    QLabel *text = new QLabel(&dialog);
    text->setWordWrap(true);

    if (printer->duplexCalibrated())
    {
        // The remembered movement, shown rather than left to memory.
        text->setText(QObject::tr("<p>One side of every sheet has been printed on <b>%1</b>.</p>"
                                  "<p><b>%2</b> Keep them in the same order, then press Continue.</p>",
                                  "Manual duplex prompt. %1 is the printer, %2 the remembered movement")
                      .arg(printer->name(), manualDuplexInstruction(printer->manualDuplexHandling())));
    }
    else
    {
        // Never calibrated, so we genuinely do not know which way is right and
        // must not pretend otherwise.
        text->setText(QObject::tr("<p>One side of every sheet has been printed on <b>%1</b>.</p>"
                                  "<p>Turn the pages over, put them back in the printer and press "
                                  "Continue.</p>"
                                  "<p><i>Printer settings can work out which way round they go, so "
                                  "you do not have to remember.</i></p>",
                                  "Manual duplex prompt for a printer that has not been calibrated")
                      .arg(printer->name()));
    }

    QDialogButtonBox *buttons = new QDialogButtonBox(&dialog);
    buttons->addButton(QDialogButtonBox::Abort);
    QPushButton *ok = buttons->addButton(QDialogButtonBox::Ok);
    ok->setText(QObject::tr("Continue", "Manual duplex prompt button"));

    QObject::connect(buttons, SIGNAL(accepted()), &dialog, SLOT(accept()));
    QObject::connect(buttons, SIGNAL(rejected()), &dialog, SLOT(reject()));

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    layout->addWidget(text);

    if (printer->duplexCalibrated())
    {
        FlipDiagram *diagram = new FlipDiagram(&dialog);
        diagram->setHandling(printer->manualDuplexHandling());
        layout->addWidget(diagram, 0, Qt::AlignHCenter);
    }

    layout->addWidget(buttons);

    return dialog.exec() == QDialog::Accepted;
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
           "<p>Take the whole stack out <b>without changing its order</b>, put it "
           "back in the paper tray ready to print the other side, and tell us how "
           "you did it. PlicaPage will remember, and show you the same picture "
           "every time you print double-sided.</p>",
           "Duplex calibration wizard, choosing how the paper is put back"), page);
    text->setWordWrap(true);
    l->addWidget(text);

    mHandlingSidewaysBtn = new QRadioButton(manualDuplexInstruction(HandlingFlipSideways), page);
    mHandlingEndOverBtn  = new QRadioButton(manualDuplexInstruction(HandlingFlipEndOver), page);
    mHandlingNoFlipBtn   = new QRadioButton(manualDuplexInstruction(HandlingNoFlip), page);
    mHandlingSidewaysBtn->setChecked(true);

    l->addWidget(mHandlingSidewaysBtn);
    l->addWidget(mHandlingEndOverBtn);
    l->addWidget(mHandlingNoFlipBtn);

    mFlipDiagram = new FlipDiagram(page);
    l->addWidget(mFlipDiagram, 0, Qt::AlignHCenter);

    // The picture follows the choice, so the user can see what each one means
    // before committing to it.
    connect(mHandlingSidewaysBtn, &QRadioButton::toggled, this, [this](bool on) {
        if (on) mFlipDiagram->setHandling(HandlingFlipSideways); });
    connect(mHandlingEndOverBtn, &QRadioButton::toggled, this, [this](bool on) {
        if (on) mFlipDiagram->setHandling(HandlingFlipEndOver); });
    connect(mHandlingNoFlipBtn, &QRadioButton::toggled, this, [this](bool on) {
        if (on) mFlipDiagram->setHandling(HandlingNoFlip); });

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
ManualDuplexHandling DuplexWizard::chosenHandling() const
{
    if (mHandlingEndOverBtn && mHandlingEndOverBtn->isChecked())
        return HandlingFlipEndOver;

    if (mHandlingNoFlipBtn && mHandlingNoFlipBtn->isChecked())
        return HandlingNoFlip;

    return HandlingFlipSideways;
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
    mTarget->setManualDuplexHandling(chosenHandling());
    mTarget->setDuplexCalibrated(true);

    // Manual duplex is now described by the flip edge and the stacking order.
    // DuplexType only still distinguishes "the printer does it" from "by hand".
    if (mTarget->duplexType() == DuplexAuto)
        mTarget->setDuplexType(DuplexManual);

    mCalibrated = true;

    mResultLabel->setText(
        tr("<p><b>%1</b> is set up.</p>"
           "<p>Its sheets come back %2, and turning them over puts the two sides "
           "%3.</p>",
           "Duplex calibration result. %1 printer, %2 and %3 are filled in below")
        .arg(mPrinter->name())
        .arg(reversed ? tr("in reverse order", "Duplex calibration result fragment")
                      : tr("in the same order", "Duplex calibration result fragment"))
        .arg(flip == FlipType::ShortEdge
             ? tr("head to toe, so one side is turned round", "Duplex calibration result fragment")
             : tr("the same way up", "Duplex calibration result fragment")));

    mResultLabel->setText(mResultLabel->text() +
        tr("<p>From now on PlicaPage will remind you: <b>%1</b></p>",
           "Duplex calibration result, the movement that will be shown at print time")
        .arg(manualDuplexInstruction(chosenHandling())));
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
                tr("Then the stack went back the wrong way up, and there is "
                   "nothing to measure.\n\n"
                   "Go back and choose a different way of putting the paper in - "
                   "most likely \"%1\" - then try again. It will use two more "
                   "sheets of paper.",
                   "Duplex calibration, stack was inserted the wrong way up")
                .arg(manualDuplexInstruction(HandlingNoFlip)));
            mPages->setCurrentIndex(PageFlip);
            updateButtons();
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
    dialog.setText(tr("%1 Keep them in the same order, then press Continue.",
                      "Duplex calibration verification step. %1 is the chosen movement")
                   .arg(manualDuplexInstruction(chosenHandling())));
    dialog.addButton(QMessageBox::Abort);
    QPushButton *btn = dialog.addButton(QMessageBox::Ok);
    btn->setText(tr("Continue", "Duplex calibration button"));

    if (dialog.exec() == QMessageBox::Ok)
        printPass(2);
}
