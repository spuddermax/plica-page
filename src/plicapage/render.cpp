/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL2+
 *
 *
 * Copyright: 2012-2013 Boomaga team https://github.com/Boomaga
 * Authors:
 *   Alexander Sokoloff <sokoloff.a@gmail.com>
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

#include "render.h"
#include <poppler-document.h>
#include <poppler-page-renderer.h>
#include <poppler-page.h>
#include <QDebug>
#include <QFile>
#include <QFileInfo>

#include "kernel/project.h"
#include "kernel/layout.h"
#include "popplergate.h"


/************************************************

 ************************************************/
QImage doRenderSheet(poppler::document *doc, int sheetNum, double resolution)
{
    // Covers the whole of the poppler work - the page, the render, and the
    // image that owns the pixels. Shared, so renders still overlap each other;
    // the first one in the process is the exception, and takes the gate alone
    // so poppler finishes building its global colour profiles before anything
    // else is inside the library. See popplergate.h.
    PopplerGate::RenderLock gate;

    poppler::page *page = doc->create_page(sheetNum);
    if (page)
    {
        poppler::page_renderer prender;
        prender.set_render_hint(poppler::page_renderer::antialiasing, true);
        prender.set_render_hint(poppler::page_renderer::text_antialiasing, true);

        poppler::image img = prender.render_page(page, resolution, resolution);

        QImage::Format format = QImage::Format_Invalid;

        switch (img.format())
        {
        case poppler::image::format_invalid: format = QImage::Format_Invalid; break;
        case poppler::image::format_mono:    format = QImage::Format_Mono;    break;
        case poppler::image::format_rgb24:   format = QImage::Format_RGB32;   break;
        case poppler::image::format_argb32:  format = QImage::Format_ARGB32;  break;
        }


        QImage result;
        if (format != QImage::Format_Invalid)
        {
            result = QImage(reinterpret_cast<const uchar*>(img.const_data()),
                            img.width(), img.height(),
                            img.bytes_per_row(),
                            format).copy();
        }

        delete page;
        return result;
    }

    return QImage();
}


/************************************************
 *
 ************************************************/
RenderWorker::RenderWorker(const QString &fileName, int resolution):
    QObject(),
    mSheetNum(0),
    mResolution(resolution),
    mBusy(false),
    mPopplerDoc(0)
{
    if (QFileInfo(fileName).exists())
    {
        // Exclusive, so no render is in flight anywhere in the process while a
        // document is opened. Loads were never shown to be what was crashing,
        // but nor were they shown to be safe; they are rare and quick, so the
        // exclusion costs nothing worth measuring.
        PopplerGate::DocumentLock gate;
        mPopplerDoc = poppler::document::load_from_file(fileName.toLocal8Bit().data());
    }
}


/************************************************
 *
 ************************************************/
RenderWorker::~RenderWorker()
{
    // Exclusive for the same reason as the load.
    //
    // The old setFileName() freed each worker's document in turn while the rest
    // of the pool was still rendering, and that looked for a long time like the
    // whole bug. It was not - see test_RenderNoReloadStress - but it was real,
    // and this closes it either way.
    PopplerGate::DocumentLock gate;
    delete mPopplerDoc;
}


/************************************************
 *
 ************************************************/
QImage RenderWorker::renderSheet(int sheetNum)
{
    if (!mPopplerDoc)
        return QImage();

    QImage img = doRenderSheet(mPopplerDoc, sheetNum, mResolution);
    emit sheetReady(img, sheetNum);
    return img;
}


/************************************************
 *
 ************************************************/
QImage RenderWorker::renderPage(int sheetNum, const QRectF &pageRect, int pageNum)
{
    if (!mPopplerDoc)
        return QImage();

    QImage img = doRenderSheet(mPopplerDoc, sheetNum, mResolution);

    QSizeF printerSize =  project->printer()->paperRect().size();

    if (isLandscape(project->rotation()))
        printerSize.transpose();

    double scale = qMin(img.width() * 1.0 / printerSize.width(),
                        img.height() * 1.0 / printerSize.height());

    QSize size = QSize(pageRect.width()  * scale,
                       pageRect.height() * scale);

    if (isLandscape(project->rotation()))
        size.transpose();

    QRect rect(QPoint(0, 0), size);
    if (isLandscape(project->rotation()))
    {
        rect.moveRight(img.width() - pageRect.top()  * scale);
        rect.moveTop(pageRect.left() * scale);
    }
    else
    {
        rect.moveLeft(pageRect.left() * scale);
        rect.moveTop(pageRect.top()  * scale);
    }

    img = img.copy(rect);

    emit pageReady(img, pageNum);
    return img;
}


/************************************************
 *
 ************************************************/
Render::Render(double resolution, int threadCount, QObject *parent):
    QObject(parent),
    mResolution(resolution),
    mThreadCount(threadCount)
{
    mWorkers.reserve(threadCount);
}


/************************************************
 *
 ************************************************/
Render::~Render()
{
    stopWorkers();
    qDeleteAll(mWorkers);
    mWorkers.clear();
}


/************************************************
 * Brings every worker in this pool to a halt.
 *
 * Both passes run to completion before the caller deletes anything, and that
 * separation is the point. Quitting, waiting for and deleting one worker at a
 * time - which is what this used to do - destroys the first worker's document
 * while the rest of the pool is still inside poppler.
 *
 * quit() only asks the event loop to stop, so asking all of them first lets
 * the threads wind down alongside each other instead of end to end.
 ************************************************/
void Render::stopWorkers()
{
    foreach (RenderWorker *worker, mWorkers)
        worker->thread()->quit();

    foreach (RenderWorker *worker, mWorkers)
        worker->thread()->wait();
}


/************************************************
 *
 ************************************************/
void Render::setFileName(const QString &fileName)
{
    mFileName = fileName;

    stopWorkers();

    // Jobs still waiting were asked for against the document being replaced,
    // and nothing will ever complete to drain them: the queue only advances
    // when a worker reports back, and none of them is running now.
    mQueue.clear();

    qDeleteAll(mWorkers);
    mWorkers.clear();
    mWorkers.resize(mThreadCount);

    for (int i=0; i<mWorkers.count(); ++i)
    {
        RenderWorker *worker = new RenderWorker(fileName, mResolution);
        mWorkers[i] = worker;

        connect(worker, SIGNAL(sheetReady(QImage,int)),
                this, SIGNAL(sheetReady(QImage,int)));

        connect(worker, SIGNAL(sheetReady(QImage,int)),
                this, SLOT(workerFinished()));

        connect(worker, SIGNAL(pageReady(QImage,int)),
                this, SIGNAL(pageReady(QImage,int)));

        connect(worker, SIGNAL(pageReady(QImage,int)),
                this, SLOT(workerFinished()));


        worker->moveToThread(worker->thread());
    }

    // Started only once the whole pool exists. Starting them as they were
    // built let the first threads run while later workers were still opening
    // their documents on this one.
    foreach (RenderWorker *worker, mWorkers)
        worker->thread()->start();
}


/************************************************
 *
 ************************************************/
void Render::renderSheet(int sheetNum)
{
    foreach (RenderWorker *worker, mWorkers)
    {
        if (!worker->isBusy() && worker->isValid())
        {
            startRenderSheet(worker, sheetNum);
            return;
        }
    }

    QPair<int,bool> job(sheetNum, false);
    if (!mQueue.contains(job))
        mQueue.prepend(job);
}


/************************************************
 *
 ************************************************/
void Render::renderPage(int pageNum)
{
    foreach (RenderWorker *worker, mWorkers)
    {
        if (!worker->isBusy() && worker->isValid())
        {
            startRenderPage(worker, pageNum);
            return;
        }
    }

    QPair<int,bool> job(pageNum, true);
    if (!mQueue.contains(job))
        mQueue.prepend(job);

}


/************************************************
 *
 ************************************************/
void Render::cancelSheet(int sheetNum)
{
    mQueue.removeAll(QPair<int,bool>(sheetNum, false));
}


/************************************************
 *
 ************************************************/
void Render::cancelPage(int pageNum)
{
    mQueue.removeAll(QPair<int,bool>(pageNum, true));
}


/************************************************
 *
 ************************************************/
void Render::workerFinished()
{
    RenderWorker *worker = qobject_cast<RenderWorker*>(sender());
    if (!worker)
        return;

    worker->setBusy(false);

    if (!mQueue.isEmpty())
    {
        QPair<int,bool> job = mQueue.takeFirst();
        if (!job.second)
            startRenderSheet(worker, job.first);
        else
            startRenderPage(worker, job.first);
    }
}


/************************************************
 *
 ************************************************/
void Render::startRenderSheet(RenderWorker *worker, int sheetNum)
{
    worker->setBusy(true);
    QMetaObject::invokeMethod(worker,
                              "renderSheet",
                              Qt::QueuedConnection,
                              Q_ARG(int, sheetNum));
}


/************************************************
 *
 ************************************************/
void Render::startRenderPage(RenderWorker *worker, int pageNum)
{
    int sheetNum = project->previewSheets().indexOfPage(pageNum);
    if (sheetNum < 0)
        return;

    Sheet *sheet = project->previewSheets().at(sheetNum);
    ProjectPage *page = project->page(pageNum);

    int pageOnSheet = -1;
    for (int i = 0; i<sheet->count(); ++i)
    {
        if (sheet->page(i) == page)
            pageOnSheet = i;
    }

    if (pageOnSheet < 0)
        return;

    TransformSpec spec = project->layout()->transformSpec(sheet, pageOnSheet, project->rotation());

    // Only now, past every early return: a worker marked busy for a job that
    // was never sent would never be offered another one.
    worker->setBusy(true);
    QMetaObject::invokeMethod(worker,
                              "renderPage",
                              Qt::QueuedConnection,
                              Q_ARG(int, sheetNum),
                              Q_ARG(QRectF, spec.rect),
                              Q_ARG(int, pageNum));
}


/************************************************

 ************************************************/
QImage toGrayscale(const QImage &srcImage)
{
    // Convert to 32bit pixel format
    QImage dstImage = srcImage.convertToFormat(srcImage.hasAlphaChannel() ?
                                                   QImage::Format_ARGB32 : QImage::Format_RGB32);

    unsigned int *data = (unsigned int*)dstImage.bits();
    int pixelCount = dstImage.width() * dstImage.height();

    // Convert each pixel to grayscale
    for(int i = 0; i < pixelCount; ++i)
    {
        int val = qGray(*data);
        *data = qRgba(val, val, val, qAlpha(*data));
        ++data;
    }

    return dstImage;
}
