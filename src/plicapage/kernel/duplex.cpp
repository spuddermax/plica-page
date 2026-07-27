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


#include "duplex.h"


/************************************************

 ************************************************/
DuplexPasses calcDuplexPasses(FlipType flip,
                              bool reversesOrder,
                              bool docReverseOrder,
                              bool sheetIsLandscape)
{
    DuplexPasses res;

    if (!docReverseOrder)
    {
        res.pages1 = Project::OddPages;
        res.pages2 = Project::EvenPages;
        res.order1 = Project::ForwardOrder;
        res.order2 = reversesOrder ? Project::BackOrder : Project::ForwardOrder;
    }
    else
    {
        res.pages1 = Project::EvenPages;
        res.pages2 = Project::OddPages;
        res.order1 = Project::BackOrder;
        res.order2 = reversesOrder ? Project::ForwardOrder : Project::BackOrder;
    }

    // Turning the sheet about its short edge swaps top and bottom, so content
    // that reads up the page comes back upside down and the first pass has to
    // be pre-rotated. Turning about the long edge swaps left and right and
    // leaves it alone.
    //
    // The sheet's own rotation flips that conclusion. A booklet lays its pages
    // sideways on the paper, so what is "up" for the content runs along the
    // paper's long axis - and then it is the short-edge turn that leaves it
    // undisturbed. Hence the exclusive or rather than a plain assignment.
    res.rotate1 = (flip == FlipType::ShortEdge) != sheetIsLandscape;

    return res;
}
