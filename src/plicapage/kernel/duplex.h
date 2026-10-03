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


#ifndef DUPLEX_H
#define DUPLEX_H

#include "plicapagetypes.h"
#include "project.h"


/************************************************
 * How a manual double-sided job is split into two printer passes.
 ************************************************/
struct DuplexPasses
{
    Project::PagesType  pages1;
    Project::PagesType  pages2;
    Project::PagesOrder order1;
    Project::PagesOrder order2;
    bool rotate1;           ///< first pass turned 180 degrees on the paper
};


/************************************************
 * Works out the two passes from the two things the calibration wizard
 * measures, plus the two things the document decides.
 *
 * @param flip              which edge the sheet ends up turned about, counting
 *                          both the printer's paper path and the way the user
 *                          puts the stack back
 * @param reversesOrder     true when the second pass reaches the sheets in the
 *                          opposite order to the first
 * @param docReverseOrder   the profile's "print in reverse order" preference
 * @param sheetIsLandscape  whether the sheet itself is rotated, i.e.
 *                          isLandscape(Project::rotation())
 *
 * Kept free of any GUI so it can be tested directly.
 ************************************************/
DuplexPasses calcDuplexPasses(FlipType flip,
                              bool reversesOrder,
                              bool docReverseOrder,
                              bool sheetIsLandscape);



/************************************************
 * Whether a printed sheet goes through the printer turned 180 degrees
 * relative to the layout's frame, so that the printer's top and bottom
 * limits land on its bottom and top, and left and right swap.
 *
 * Every side is printed in the printer's own frame. Three things can turn
 * content against it, and two turns cancel:
 *  - the sheet's /Rotate, which CUPS resolves onto portrait paper as
 *    Printer::pageTurnedOnPaper() describes;
 *  - a manual double-sided job pre-rotates its first pass by a half turn when
 *    calcDuplexPasses() says so;
 *  - an automatic duplexer flipping on the long edge prints the back of each
 *    sheet upside down in its paper path, so that it reads the right way up
 *    once turned over; on the short edge it does not.
 *
 * @param sheetIndex      the sheet's place in Layout::fillSheets(); even
 *                        indices are the first side of each piece of paper
 * @param autoFlip        the edge an automatic duplexer is asked to flip on,
 *                        i.e. Layout::flipType(profile.flipType())
 * @param sheetRotation   Project::rotation(), every sheet's /Rotate before any
 *                        pass rotation
 ************************************************/
bool sheetGoesThroughTurned(int sheetIndex, bool doubleSided,
                            const PrinterProfile &profile, const Printer &printer,
                            FlipType autoFlip, Rotation sheetRotation);

/// Margins as seen from the far side of a half turn.
inline QMarginsF turnMargins(const QMarginsF &m)
{
    return QMarginsF(m.right(), m.bottom(), m.left(), m.top());
}

#endif // DUPLEX_H
