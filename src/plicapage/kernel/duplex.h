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

#endif // DUPLEX_H
