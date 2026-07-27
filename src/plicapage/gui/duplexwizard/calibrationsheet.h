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


#ifndef CALIBRATIONSHEET_H
#define CALIBRATIONSHEET_H

#include <QString>

class Printer;


/************************************************
 * Writes one pass of the duplex calibration test as a two page PDF, and returns
 * its path (empty on failure).
 *
 * Each page carries a big identifying glyph and a solid bar flush against one
 * edge of the printable area:
 *
 *      pass 1 -> "1" and "2", bar labelled SIDE 1
 *      pass 2 -> "A" and "B", bar labelled SIDE 2
 *
 * The bar is what makes the answer readable. Once a sheet has been printed on
 * both sides, the two bars sitting at the same edge of the paper means the
 * sides are related by a turn about the long edge; at opposite edges means the
 * short edge. The user does not have to reason about any of that - they just
 * look at the paper.
 *
 * Pages are drawn in the printer's own frame with no rotation applied, so the
 * result measures the printer and the user's handling, not our own transforms.
 ************************************************/
QString writeCalibrationPdf(const Printer *printer, int pass);

#endif // CALIBRATIONSHEET_H
