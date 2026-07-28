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


#ifndef POPPLERGATE_H
#define POPPLERGATE_H

/************************************************
 * Keeps PlicaPage out of poppler's process-wide state races.
 *
 * The symptom was a "double free or corruption (fasttop)" abort, roughly one
 * launch in five on a large scanned document, taking the loaded jobs with it.
 * Stock Boomaga aborts the same way, so this is inherited rather than new.
 *
 * The cause is poppler's lazily initialised global colour management. The
 * first render in the process builds the shared lcms profiles and transforms
 * on its way through GfxState's constructor, and that setup is not guarded. Two
 * renders reaching it together corrupt the heap, and the abort lands later in
 * cmsCloseProfile under whichever thread frees next - often long after, and
 * with every thread apparently just rendering, which is what makes it look
 * like a mystery.
 *
 * Once that setup has completed, rendering in parallel is fine. So the gate
 * does not serialise the render path; it only makes sure the first render runs
 * on its own:
 *
 *   RenderLock    Held while touching a live document - creating a page,
 *                 rendering it, destroying it. Shared, so any number of
 *                 threads may render together and the preview stays parallel.
 *                 The exception is the first one taken in the process, which
 *                 is exclusive: that is the render that performs poppler's
 *                 one-time setup, and nothing may overlap it.
 *
 *   DocumentLock  Held across poppler::document construction and destruction.
 *                 Exclusive, so it waits for the renders in flight and holds
 *                 off new ones.
 *
 * The DocumentLock is the belt to the RenderLock's braces. Loads and destroys
 * were the original suspect, and they were never shown to be safe against a
 * concurrent render - only never shown to be the thing that was crashing. They
 * are rare and short, so excluding them costs nothing measurable and removes
 * the question.
 *
 * The gate is process-wide on purpose. PlicaPage runs two Render pools and
 * PageTrimmer opens documents of its own on the main thread; poppler's globals
 * belong to none of them, so a gate owned by one would leave the rest racing.
 *
 * The lock is not recursive. Never take a DocumentLock while holding a
 * RenderLock - open and close documents outside the scope that renders from
 * them, which is how the call sites are written.
 ************************************************/
namespace PopplerGate
{

/************************************************
 * Shared, except for the first one in the process. Hold while working with a
 * document that is already open.
 ************************************************/
class RenderLock
{
public:
    RenderLock();
    ~RenderLock();

private:
    // Whether this lock is the one doing poppler's one-time warm-up, and so
    // has to be released the same exclusive way it was taken.
    bool mExclusive;

    RenderLock(const RenderLock &);
    RenderLock &operator=(const RenderLock &);
};


/************************************************
 * Exclusive. Hold across poppler::document construction and destruction.
 ************************************************/
class DocumentLock
{
public:
    DocumentLock();
    ~DocumentLock();

private:
    DocumentLock(const DocumentLock &);
    DocumentLock &operator=(const DocumentLock &);
};


/************************************************
 * High-water mark of RenderLocks held at once since the last reset.
 *
 * Instrumentation for the tests. The fix would be just as crash-free if it
 * had quietly serialised every render, and the preview would be eight times
 * slower for it, so the tests assert that renders still overlap rather than
 * trusting that they do. The cost is one atomic increment per page rendered,
 * which is nothing beside the render.
 ************************************************/
int  peakConcurrentRenders();
void resetPeakConcurrentRenders();

} // namespace PopplerGate

#endif // POPPLERGATE_H
