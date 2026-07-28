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


#include "popplergate.h"

#include <QAtomicInt>
#include <QReadWriteLock>


namespace
{

/************************************************
 * Deliberately never destroyed.
 *
 * Render workers live on QThreads, and a document freed during static
 * destruction would otherwise reach for a lock that had already gone. Leaking
 * one lock for the life of the process is the cheap way to make the gate
 * outlast everything that uses it. Function-local static initialisation is
 * itself thread-safe, so the first caller cannot race a second one.
 ************************************************/
QReadWriteLock &gate()
{
    static QReadWriteLock *instance = new QReadWriteLock(QReadWriteLock::NonRecursive);
    return *instance;
}


QAtomicInt gCurrentRenders(0);
QAtomicInt gPeakRenders(0);

/************************************************
 * Cleared until a render has completed while holding the gate exclusively,
 * which is what forces poppler's one-time global setup to happen with nothing
 * else inside the library.
 ************************************************/
QAtomicInt gWarmedUp(0);

} // namespace


namespace PopplerGate
{

/************************************************

 ************************************************/
RenderLock::RenderLock():
    mExclusive(gWarmedUp.loadAcquire() == 0)
{
    // Several threads can read the flag before any of them has set it, and so
    // several may take the exclusive path. They simply queue behind each other
    // and warm poppler up more than once, which is harmless - and much easier
    // to be sure of than trying to elect a single winner. What matters is only
    // that no shared holder can overlap an exclusive one, which the lock
    // guarantees.
    if (mExclusive)
        gate().lockForWrite();
    else
        gate().lockForRead();

    const int now = gCurrentRenders.fetchAndAddOrdered(1) + 1;

    // Raise the watermark if this render pushed it up. Another thread may be
    // doing the same, so re-read and retry rather than assuming the compare
    // held.
    forever
    {
        const int peak = gPeakRenders.loadAcquire();
        if (now <= peak || gPeakRenders.testAndSetOrdered(peak, now))
            break;
    }
}


/************************************************

 ************************************************/
RenderLock::~RenderLock()
{
    gCurrentRenders.fetchAndAddOrdered(-1);

    // Set before unlocking, so the flag is already visible to whoever the lock
    // hands off to.
    if (mExclusive)
        gWarmedUp.storeRelease(1);

    gate().unlock();
}


/************************************************

 ************************************************/
DocumentLock::DocumentLock()
{
    gate().lockForWrite();
}


/************************************************

 ************************************************/
DocumentLock::~DocumentLock()
{
    gate().unlock();
}


/************************************************

 ************************************************/
int peakConcurrentRenders()
{
    return gPeakRenders.loadAcquire();
}


/************************************************

 ************************************************/
void resetPeakConcurrentRenders()
{
    gPeakRenders.storeRelease(gCurrentRenders.loadAcquire());
}

} // namespace PopplerGate
