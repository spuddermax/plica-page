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


#ifndef RENDER_H
#define RENDER_H

#include <QObject>
#include <QImage>
#include <QThread>
#include <QList>
#include <QPair>

namespace poppler
{
    class document;
}

class RenderWorker: public QObject
{
    Q_OBJECT
public:
    explicit RenderWorker(const QString &fileName, int resolution);
    virtual ~RenderWorker();

    // mBusy is written by Render on the main thread only - when a job is
    // handed out, and again when the result comes back. It used to be set by
    // the worker thread instead, which meant the main thread could dispatch a
    // whole burst of sheets before any worker had flipped its flag, and every
    // one of them landed on the first worker. Eight threads then rendered a
    // preview one page at a time.
    bool isBusy() const { return mBusy; }
    void setBusy(bool busy) { mBusy = busy; }

    // False when the document failed to open. Such a worker never answers, so
    // handing it a job would mark it busy for good.
    bool isValid() const { return mPopplerDoc != 0; }

    QThread *thread() { return &mThread; }

public slots:
    QImage renderSheet(int sheetNum);
    QImage renderPage(int sheetNum, const QRectF &pageRect, int pageNum);

signals:
    void sheetReady(QImage, int sheetNum);
    void pageReady(QImage, int pageNum);

private:
    int mSheetNum;
    int mResolution;
    bool mBusy;
    QThread mThread;
    poppler::document *mPopplerDoc;
};


class Render : public QObject
{
    Q_OBJECT
public:
    explicit Render(double resolution, int threadCount = 8, QObject *parent = 0);
    virtual ~Render();

    QString fileName() const { return mFileName; }

public slots:
    void setFileName(const QString &fileName);

    void renderSheet(int sheetNum);
    void cancelSheet(int sheetNum);

    void renderPage(int pageNum);
    void cancelPage(int pageNum);

signals:
    void sheetReady(QImage, int sheetNum);
    void pageReady(QImage, int pageNum);

private slots:
    void workerFinished();

private:
    QString mFileName;
    QVector<RenderWorker*> mWorkers;
    int mResolution;
    int mThreadCount;
    QList<QPair<int, bool> > mQueue;

    void startRenderSheet(RenderWorker *worker, int sheetNum);
    void startRenderPage(RenderWorker *worker, int pageNum);
    void stopWorkers();

};

QImage toGrayscale(const QImage &srcImage);

#endif // RENDER_H
