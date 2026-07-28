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


#ifndef DUPLEXWIZARD_H
#define DUPLEXWIZARD_H

#include <QDialog>
#include "plicapagetypes.h"

class Printer;
class PrinterProfile;
class QLabel;
class QPushButton;
class QRadioButton;
class QStackedWidget;


/************************************************
 * Draws the sheet-turning instruction: a page seen face on, with the axis it
 * should be turned about and arrows going round that axis.
 *
 * Shared by the wizard and the prompt shown midway through a real double-sided
 * job, so that the action the user calibrated is the action they are later
 * asked to repeat. Painted rather than shipped as bitmaps, following
 * PrinterSettings::updatePreview().
 ************************************************/
class FlipDiagram : public QWidget
{
    Q_OBJECT
public:
    explicit FlipDiagram(QWidget *parent = nullptr);
    QSize sizeHint() const override;

public slots:
    void setHandling(ManualDuplexHandling handling);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    ManualDuplexHandling mHandling;
};


/// One sentence describing @a handling, shared by the wizard and the print
/// prompt so the two can never describe the movement differently.
QString manualDuplexInstruction(ManualDuplexHandling handling);

/// The "and put it back in the tray" half of the instruction, shared so every
/// place that asks for the movement also asks for the paper to be reloaded.
QString manualDuplexReinsertHint();

/**
 * The "now turn the stack over" prompt shown midway through a real job.
 *
 * Shows the printer's remembered handling with the same diagram the wizard
 * used, so the user does not have to remember which movement they calibrated.
 * Returns false if they abort.
 */
bool showManualDuplexPrompt(const Printer *printer, QWidget *parent);


/************************************************
 * Works out how a printer behaves in manual double-sided printing by printing
 * two marked sheets, having the user turn them over, printing their backs, and
 * asking what landed where.
 *
 * Two sheets is the least that can answer it: telling whether the stack comes
 * back reversed needs at least two distinguishable sheets, and the turn itself
 * can be read off the same two.
 ************************************************/
class DuplexWizard : public QDialog
{
    Q_OBJECT
public:
    explicit DuplexWizard(Printer *printer, PrinterProfile *target, QWidget *parent = nullptr);

    /**
     * Runs the wizard modally, writing what it measures into @a target.
     *
     * The result goes to a caller-supplied profile rather than straight to the
     * printer because PrinterSettings edits copies of the profiles and pushes
     * them back when the user presses OK - writing anywhere else would simply be
     * overwritten. Saving is likewise left to the caller.
     */
    static bool execute(Printer *printer, PrinterProfile *target, QWidget *parent);

private slots:
    void next();
    void back();
    void printVerification();

private:
    enum Page { PageIntro = 0, PageFlip, PageQuestions, PageResult };

    void buildIntroPage();
    void buildFlipPage();
    void buildQuestionsPage();
    void buildResultPage();
    void updateButtons();
    bool printPass(int pass);
    void applyAnswers();
    ManualDuplexHandling chosenHandling() const;

    Printer *mPrinter;
    PrinterProfile *mTarget;
    QStackedWidget *mPages;
    QPushButton *mNextButton;
    QPushButton *mBackButton;

    QRadioButton *mOrderSameBtn;      ///< the sheet marked 1 has A on its back
    QRadioButton *mOrderReversedBtn;  ///< it has B
    QRadioButton *mBarSameEdgeBtn;    ///< both bars along one edge -> long edge turn
    QRadioButton *mBarOppositeBtn;    ///< bars at opposite edges  -> short edge turn
    QRadioButton *mOverprintedBtn;    ///< second pass landed on the printed side

    FlipDiagram *mFlipDiagram;
    QRadioButton *mHandlingSidewaysBtn;
    QRadioButton *mHandlingEndOverBtn;
    QRadioButton *mHandlingNoFlipBtn;

    QLabel *mResultLabel;
    bool mCalibrated;
};

#endif // DUPLEXWIZARD_H
