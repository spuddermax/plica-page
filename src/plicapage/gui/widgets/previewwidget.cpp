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


#include "previewwidget.h"
#include "kernel/project.h"
#include "kernel/layout.h"
#include "render.h"

#include <QPaintEvent>
#include <QPainter>
#include <QPen>
#include <QWheelEvent>
#include <QDebug>
#include <QRectF>
#include <QTimer>
#include <QToolTip>
#include "kernel/job.h"
#include "kernel/printer.h"

#define MARGIN_H        20
#define MARGIN_V        20
#define MARGIN_BOOKLET  4
#define RESOLUTIN       150

#define CACHE_PRE       10
#define CACHE_POST      20



/************************************************
 *
 ************************************************/
RenderCache::RenderCache(double resolution, int threadCount, QObject *parent):
    QObject(parent),
    mRender(new Render(resolution, threadCount, this))
{
    connect(mRender, SIGNAL(sheetReady(QImage,int)),
            this, SLOT(onSheetReady(QImage,int)));
}


/************************************************
 *
 ************************************************/
RenderCache::~RenderCache()
{

}


/************************************************
 *
 ************************************************/
QString RenderCache::fileName() const
{
    return mRender->fileName();
}


/************************************************
 *
 ************************************************/
void RenderCache::setFileName(const QString &fileName)
{
    mRender->setFileName(fileName);
    mItems.clear();
}


/************************************************
 *
 ************************************************/
void RenderCache::renderSheet(int sheetNum)
{
    if (mItems.contains(sheetNum))
        emit sheetReady(mItems.value(sheetNum), sheetNum);
    else
        mRender->renderSheet(sheetNum);

    int start = qMax(0, sheetNum - CACHE_PRE);
    int end = qMin(project->previewSheetCount()-1, sheetNum + CACHE_POST);

    for (int i=start; i<sheetNum; ++i)
    {
        if (!mItems.contains(i))
            mRender->renderSheet(i);
    }

    for (int i=sheetNum+1; i<=end; ++i)
    {
        if (!mItems.contains(i))
            mRender->renderSheet(i);
    }

    // Remove old values ........................
    QHash<int, QImage>::iterator it = mItems.begin();
    while (it != mItems.end())
    {
        if (it.key() < start || it.key() > end)
            it = mItems.erase(it);
        else
            ++it;
    }
}


/************************************************
 *
 ************************************************/
void RenderCache::cancelSheet(int sheetNum)
{
    mRender->cancelSheet(sheetNum);
}


/************************************************
 *
 ************************************************/
void RenderCache::onSheetReady(const QImage &img, int sheetNum)
{
    mItems.insert(sheetNum, img);
    emit sheetReady(img, sheetNum);
}



/************************************************

 ************************************************/
PreviewWidget::PreviewWidget(QWidget *parent) :
    QFrame(parent),
    mDisplayedSheetNum(-1),
    mScaleFactor(0),
    mWheelDelta(0),
    mHoverTimer(new QTimer(this)),
    mHoverPage(nullptr)
{
    // Page details after a two-second hover: long enough not to get in the
    // way of clicking about, unlike Qt's own tooltip delay.
    setMouseTracking(true);
    mHoverTimer->setSingleShot(true);
    mHoverTimer->setInterval(2000);
    connect(mHoverTimer, SIGNAL(timeout()), this, SLOT(showPageToolTip()));

    QPalette pal(palette());
    pal.setColor(QPalette::Background, QColor(105, 101, 98));
    setPalette(pal);
    setAutoFillBackground(true);

    connect(project, SIGNAL(changed()),
            this, SLOT(refresh()));

    connect(project, SIGNAL(selectionChanged()),
            this, SLOT(update()));

    mRender = new RenderCache(RESOLUTIN, 8, this);

    connect(project, SIGNAL(tmpFileRenamed(QString)),
            mRender, SLOT(setFileName(QString)));

    connect(mRender, SIGNAL(sheetReady(QImage,int)),
            this, SLOT(sheetImageReady(QImage,int)));
}


/************************************************

 ************************************************/
PreviewWidget::~PreviewWidget()
{
    delete mRender;
}


/************************************************
 * A rect on the sheet - the unrotated portrait frame that the layout and the
 * printer work in - to widget coordinates. A landscape sheet is shown turned
 * a quarter clockwise, so the sheet's top edge is on the right; pageRect()
 * places pages the same way.
 * ***********************************************/
QRectF PreviewWidget::sheetToWidget(const QRectF &rect) const
{
    const qreal s = mScaleFactor;
    QRectF res;
    if (isLandscape(project->rotation()))
    {
        res.setSize(QSizeF(rect.height() * s, rect.width() * s));
        res.moveRight(mDrawRect.right() - rect.top()  * s);
        res.moveTop(  mDrawRect.top()   + rect.left() * s);
    }
    else
    {
        res.setSize(rect.size() * s);
        res.moveLeft(mDrawRect.left() + rect.left() * s);
        res.moveTop( mDrawRect.top()  + rect.top()  * s);
    }
    return res;
}


/************************************************
 *
 * ***********************************************/
QRectF PreviewWidget::pageRect(int pageNum) const
{
    Sheet *sheet = project->currentSheet();
    if (!sheet || !mScaleFactor)
        return QRectF();

    TransformSpec spec = project->layout()->transformSpec(sheet, pageNum, project->rotation());

    QSize size = QSize(spec.rect.width()  * mScaleFactor,
                       spec.rect.height() * mScaleFactor);

    if (isLandscape(project->rotation()))
        size.transpose();

    QRect rect(QPoint(0, 0), size);
    if (isLandscape(project->rotation()))
    {
        rect.moveRight(mDrawRect.right() - spec.rect.top()  * mScaleFactor);
        rect.moveTop(  mDrawRect.top()   + spec.rect.left() * mScaleFactor);
    }
    else
    {
        rect.moveLeft(mDrawRect.left() + spec.rect.left() * mScaleFactor);
        rect.moveTop( mDrawRect.top()  + spec.rect.top()  * mScaleFactor);
    }

    if (mHints.testFlag(Sheet::HintSubBooklet))
    {
        if (rect.center().x() < mDrawRect.center().x())
            rect.adjust(-MARGIN_BOOKLET, 0, -MARGIN_BOOKLET, 0);
        else
            rect.adjust( MARGIN_BOOKLET, 0,  MARGIN_BOOKLET, 0);
    }

    return rect;
}


/************************************************
 *
 * ***********************************************/
int PreviewWidget::pageAt(const QPoint &point) const
{
    Sheet * sheet = project->currentSheet();
    if (!sheet)
        return -1;

    for (int i=0; i<sheet->count(); ++i)
    {
        if (pageRect(i).contains(point))
            return i;
    }

    return -1;
}


/************************************************
 *
 ************************************************/
void PreviewWidget::drawShadow(QPainter &painter, const QRectF &rect)
{
    // Shadow width
    int width = qBound(4, int(qMax(rect.height(), rect.width())) / 100, 7);

    painter.save();
    painter.setClipRect(rect.adjusted(0, 0, width, width));

    QColor lightColor = this->palette().color(QPalette::Background);
    QColor darkColor  = lightColor.darker(160);

    QRectF shadowRect = rect.adjusted(0, 0, width, width);
    QLinearGradient lg;
    lg.setColorAt(0.0, darkColor);
    lg.setColorAt(1.0, lightColor);


    QRadialGradient rg;
    rg.setColorAt(0, darkColor);
    rg.setColorAt(1, lightColor);
    rg.setRadius(width);


    QPointF center = rect.center();
    // Right
    lg.setStart(QPointF(rect.right(), center.y()));
    lg.setFinalStop(QPointF(shadowRect.right(), center.y()));
    painter.fillRect(rect.right(), rect.top() + width, width, rect.height() - width, lg);

    // Bottom
    lg.setStart(center.x(),  rect.bottom());
    lg.setFinalStop(center.x(), rect.bottom() + width);
    painter.fillRect(rect.left() + width, rect.bottom(), rect.width() - width, width, lg);

    //TopRight
    QPointF p;
    p = rect.bottomRight();
    rg.setCenter(p);
    rg.setFocalPoint(p);
    painter.fillRect(rect.right(), rect.bottom(), width, width, rg);

    // BottomRight
    p = rect.topRight();
    p.ry() += width;
    rg.setCenter(p);
    rg.setFocalPoint(p);
    painter.fillRect(rect.right(), rect.top(), width, width, rg);

    //BottomLeft
    p = rect.bottomLeft();
    p.rx() += width;
    rg.setCenter(p);
    rg.setFocalPoint(p);
    painter.fillRect(rect.left(), rect.bottom(), width, width, rg);


    painter.restore();
}


/************************************************

 ************************************************/
void PreviewWidget::paintEvent(QPaintEvent *)
{

//#define DEBUG_LAYOUT
#ifdef DEBUG_LAYOUT
    {
        if (mImage.isNull())
            return;

        if (mSheetNum < 0)
            return;

        Sheet *sheet = project->previewSheet(mSheetNum);

        if (!sheet)
            return;

        QPainter painter(this);
        QRectF rect = project->printer()->paperRect();
        painter.fillRect(rect, Qt::white);


        for (int i=0; i< sheet->count(); ++i)
        {
            QPen pen = painter.pen();
            pen.setStyle(Qt::DotLine);
            pen.setColor(Qt::darkGray);
            painter.setPen(pen);
            TransformSpec spec = project->layout()->transformSpec(sheet, i, project->rotation());

            painter.drawRect(spec.rect);
            QFont font = painter.font();
            font.setPixelSize(spec.rect.height() / 2 );
            painter.setFont(font);
            painter.drawText(spec.rect, Qt::AlignCenter, QString("%1").arg(i+1));


            pen.setStyle(Qt::SolidLine);
            pen.setColor(Qt::red);
            painter.setPen(pen);

            switch (spec.rotation)
            {
            case NoRotate:
                painter.drawLine(spec.rect.topLeft(), spec.rect.topRight());
                break;

            case Rotate90:
                painter.drawLine(spec.rect.topRight(), spec.rect.bottomRight());
                break;

            case Rotate180:
                painter.drawLine(spec.rect.bottomLeft(), spec.rect.bottomRight());
                break;

            case Rotate270:
                painter.drawLine(spec.rect.topLeft(), spec.rect.bottomLeft());
                break;
            }
        }
        return;
    }

#endif


    if (mImage.isNull())
        return;

    QSizeF printerSize =  project->printer()->paperRect().size();
    Rotation rotation = project->rotation();
    bool grayscale = project->printer()->grayscale();

    if (isLandscape(rotation))
        printerSize.transpose();

    mScaleFactor = qMin((this->geometry().width()  - 2.0 * MARGIN_H) * 1.0 / printerSize.width(),
                        (this->geometry().height() - 2.0 * MARGIN_V) * 1.0 / printerSize.height());

    if (mScaleFactor == 0)
    {
        mDrawRect = QRect();
        return;
    }

    QSize size = QSize(printerSize.width()  * mScaleFactor,
                       printerSize.height() * mScaleFactor);

    mDrawRect = QRect(QPoint(0, 0), size);
    mDrawRect.moveCenter(QPoint(0, 0));

    QRectF clipRect = mDrawRect;

    if (mHints.testFlag(Sheet::HintOnlyLeft))
    {
        switch (rotation)
        {
        case NoRotate:  clipRect.setBottom(0); break;
        case Rotate90:  clipRect.setRight(0);  break;
        case Rotate180: clipRect.setBottom(0); break;
        case Rotate270: clipRect.setRight(0);  break;
        }
    }


    if (mHints.testFlag(Sheet::HintOnlyRight))
    {
        switch (rotation)
        {
        case NoRotate:  clipRect.setTop(0);    break;
        case Rotate90:  clipRect.setLeft(0);   break;
        case Rotate180: clipRect.setTop(0);    break;
        case Rotate270: clipRect.setLeft(0);   break;
        }
    }


    QImage img = mImage.scaled(mDrawRect.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    if (grayscale)
        img = toGrayscale(img);

    // Draw .....................................
    QPainter painter(this);
    painter.save();
    QPoint center = QRect(0, 0, geometry().width(), geometry().height()).center();
    painter.translate(center);


    if (mHints.testFlag(Sheet::HintSubBooklet))
    {
        painter.save();
        QPoint center = mDrawRect.center();
        QRectF imgRect;

        clipRect = mDrawRect;
        clipRect.setRight(center.x());
        clipRect.adjust(-MARGIN_BOOKLET, 0, -MARGIN_BOOKLET, 0);
        painter.setClipRect(clipRect);

        imgRect = img.rect();
        imgRect.setRight(img.rect().center().x());
        painter.drawImage(clipRect, img, imgRect);
        drawShadow(painter, clipRect);

        clipRect = mDrawRect;
        clipRect.setLeft(center.x());
        clipRect.adjust(MARGIN_BOOKLET, 0, MARGIN_BOOKLET, 0);
        painter.setClipRect(clipRect);

        imgRect = img.rect();
        imgRect.setLeft(img.rect().center().x());
        painter.drawImage(clipRect, img, imgRect);
        drawShadow(painter, clipRect);

        painter.restore();
    }
    else
    {
        painter.save();
        painter.setClipRect(clipRect);
        painter.drawImage(mDrawRect, img);
        drawShadow(painter, clipRect);
        painter.restore();
    }



    // With a fold line chosen it is drawn into the sheet itself, and shows in
    // the rendered image exactly as it will print; this guide would hide it.
    if (mHints.testFlag(Sheet::HintDrawFold) && !mHints.testFlag(Sheet::HintSubBooklet) &&
        project->foldLine() == FoldLineNone)
    {
        QPen pen = painter.pen();
        pen.setStyle(Qt::SolidLine);
        pen.setColor(Qt::lightGray);
        painter.setPen(pen);
        if (isLandscape(rotation))
            painter.drawLine(0, mDrawRect.top(), 0, mDrawRect.bottom());
        else
            painter.drawLine(mDrawRect.left(), 0, mDrawRect.right(), 0);
    }

    painter.restore();

    mDrawRect.moveCenter(center);

    // Draw current page rect ...................
    Sheet *sheet = project->currentSheet();

    // Where the printer itself stops printing. Shown, never enforced: margins
    // can still be set inside it, and anything beyond the line is cut off.
    if (sheet && !project->printer()->hardwareMargins().isNull())
    {
        // The first and last spreads of a booklet show one page only.
        QRectF visible = mDrawRect;
        const QPointF mid = visible.center();
        if (mHints.testFlag(Sheet::HintOnlyLeft))
            isLandscape(rotation) ? visible.setRight(mid.x()) : visible.setBottom(mid.y());
        if (mHints.testFlag(Sheet::HintOnlyRight))
            isLandscape(rotation) ? visible.setLeft(mid.x()) : visible.setTop(mid.y());

        painter.save();
        painter.setBrush(Qt::NoBrush);

        // Green where the pages fit within it, red where one is cut off.
        const QRectF paper = project->printer()->paperRect();
        auto drawLimit = [&](int printSheet, const QRectF &part, bool clipped)
        {
            QPen pen(clipped ? QColor(220, 40, 40) : QColor(30, 150, 60));
            pen.setStyle(Qt::DashLine);
            painter.setPen(pen);
            painter.setClipRect(sheetToWidget(part).intersected(visible));
            painter.drawRect(sheetToWidget(paper.marginsRemoved(
                                               project->hardwareMargins(printSheet))));
        };

        if (project->layout()->id() == "Booklet")
        {
            // A reading spread is two halves of different printed sides, each
            // of which may have gone through the printer the other way round,
            // so each half gets its own side's limits. The halves meet at the
            // fold, the middle of the gap between the pages. A half with no
            // page is padding, left blank, and gets no line.
            const qreal foldY = project->printer()->pageRect().center().y();
            for (int i = 0; i < sheet->count(); ++i)
            {
                const ProjectPage *page = sheet->page(i);
                if (!page)
                    continue;

                const QRectF cell = project->layout()->transformSpec(sheet, i, rotation).rect;
                const QRectF part = (cell.center().y() < foldY)
                        ? QRectF(paper.topLeft(), QPointF(paper.right(), foldY))
                        : QRectF(QPointF(paper.left(), foldY), paper.bottomRight());
                drawLimit(project->printSheetIndex(page), part,
                          !project->pageOverflow(sheet, i).isNull());
            }
        }
        else
        {
            // Other layouts preview the printed sheets themselves, whose
            // pages all share one outline: red if any of them is cut off.
            bool clipped = false;
            for (int i = 0; i < sheet->count(); ++i)
                clipped = clipped || !project->pageOverflow(sheet, i).isNull();
            drawLimit(sheet->sheetNum(), paper, clipped);
        }

        painter.restore();
    }

    // Selected pages: what Delete would remove.
    if (sheet)
    {
        painter.save();
        QPen pen(QColor(30, 120, 220));
        pen.setWidth(3);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        for (int i = 0; i < sheet->count(); ++i)
        {
            const ProjectPage *page = sheet->page(i);
            if (page && project->isSelected(page))
                painter.drawRect(this->pageRect(i));
        }
        painter.restore();
    }

    if (sheet)
    {
        ProjectPage *curPage = project->currentPage();
        if (curPage && !project->isSelected(curPage))
        {
            painter.save();
            QPen pen = painter.pen();
            pen.setStyle(Qt::DashLine);
            //pen.setColor(QColor(142, 188, 226, 128));
            pen.setColor(QColor(105, 101, 98, 70));
            painter.setPen(pen);
            painter.drawRect(this->pageRect(sheet->indexOfPage(curPage)));
            painter.restore();
        }
    }

//#define DEBUG_CLICK_RECT
#ifdef DEBUG_CLICK_RECT
    {
        Sheet *sheet = project->currentSheet();
        if (sheet)
        {
            ProjectPage *curPage = project->currentPage();
            painter.save();
            for (int i=0; i< sheet->count(); ++i)
            {
                QPen pen = painter.pen();
                pen.setStyle(Qt::DotLine);
                if (sheet->page(i) == curPage)
                    pen.setColor(Qt::red);
                else
                    pen.setColor(QColor(142, 188, 226));
                painter.setPen(pen);
                painter.drawRect(this->pageRect(i));
                painter.drawText(this->pageRect(i).translated(10, 10), QString("%1").arg(i));
            }
            painter.restore();
        }
    }
#endif
}


/************************************************
 *
 ************************************************/
void PreviewWidget::sheetImageReady(const QImage &image, int sheetNum)
{
    int curSheet = project->currentSheetNum();
    if (sheetNum >= qMin(mDisplayedSheetNum, curSheet) &&
        sheetNum <= qMax(mDisplayedSheetNum, curSheet))
    {
        mImage = image;
        mDisplayedSheetNum = sheetNum;
        mHints = mRequests.value(sheetNum);
        update();
        emit changed();
    }
}


/************************************************

 ************************************************/
void PreviewWidget::refresh()
{
    Sheet *sheet = project->currentSheet();
    if (!sheet)
    {
        mImage = QImage();
        update();
        return;
    }


    mRequests.insert(sheet->sheetNum(), sheet->hints());
    mRender->renderSheet(sheet->sheetNum());
}



/************************************************
 * Most mouse types work in steps of 15 degrees, in which case
 * the delta value is a multiple of 120; i.e., 120 units * 1/8 = 15 degrees.
 *
 * However, some mice have finer-resolution wheels and send delta values that
 * are less than 120 units (less than 15 degrees). To support this possibility,
 * you can either cumulatively add the delta values.
 ************************************************/
void PreviewWidget::wheelEvent(QWheelEvent *event)
{
    mWheelDelta -= event->delta();
    int pages = mWheelDelta / 120;

    if (pages)
    {
        mWheelDelta = 0;
        project->setCurrentSheet(project->currentSheetNum() + pages);
    }
}


/************************************************

 ************************************************/
void PreviewWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->type() == QEvent::KeyPress)
    {
        switch (event->key())
        {

        case Qt::Key_Home:
            project->setCurrentSheet(0);
            break;

        case Qt::Key_End:
            project->setCurrentSheet(project->previewSheetCount()-1);
            break;

        case Qt::Key_PageUp:
            project->setCurrentSheet(project->currentSheetNum() - 10);
            break;

        case Qt::Key_PageDown:
            project->setCurrentSheet(project->currentSheetNum() + 10);
            break;

        case Qt::Key_Left:
        case Qt::Key_Up:
            project->setCurrentSheet(project->currentSheetNum() - 1);
            break;

        case Qt::Key_Right:
        case Qt::Key_Down:
            project->setCurrentSheet(project->currentSheetNum() + 1);
            break;

        case Qt::Key_Escape:
            project->clearSelection();
            break;

        }
    }
}


/************************************************
 *
 ************************************************/
void PreviewWidget::contextMenuEvent(QContextMenuEvent *event)
{
    Sheet *sheet = project->currentSheet();
    if (!sheet)
        return;

    int n = pageAt(event->pos());
    if (n < 0)
        emit contextMenuRequested(sheet, 0);
    else
        emit contextMenuRequested(sheet, sheet->page(n));
}


/************************************************
 *
 ************************************************/
void PreviewWidget::mousePressEvent(QMouseEvent *event)
{
    // A click is not a hover: start the wait over.
    QToolTip::hideText();
    mHoverPage = nullptr;
    mHoverTimer->stop();

    Sheet *sheet = project->currentSheet();

    if (!sheet)
        return;

    int n = pageAt(event->pos());
    ProjectPage *page = n < 0 ? nullptr : sheet->page(n);

    // Only the left button selects: a right-click opens the menu for the
    // selection it was made on.
    if (event->button() == Qt::LeftButton)
    {
        const Qt::KeyboardModifiers mods = event->modifiers();

        if (!page)
        {
            // A click off the pages clears the selection, as in a file manager.
            if (!(mods & (Qt::ControlModifier | Qt::ShiftModifier)))
                project->clearSelection();
            return;
        }

        if (mods & Qt::ShiftModifier)
            project->selectPageRange(page, mods & Qt::ControlModifier);
        else if (mods & Qt::ControlModifier)
            project->togglePageSelection(page);
        else
            project->selectPage(page);
    }

    if (page)
        project->setCurrentPage(page);
}


/************************************************
 * Starts the hover wait whenever the pointer moves onto a different page.
 ************************************************/
void PreviewWidget::mouseMoveEvent(QMouseEvent *event)
{
    QFrame::mouseMoveEvent(event);

    const Sheet *sheet = project->currentSheet();
    const int n = sheet ? pageAt(event->pos()) : -1;
    const ProjectPage *page = (n < 0) ? nullptr : sheet->page(n);

    mHoverPos = event->pos();
    if (page == mHoverPage)
        return;

    mHoverPage = page;
    QToolTip::hideText();
    if (page)
        mHoverTimer->start();
    else
        mHoverTimer->stop();
}


/************************************************

 ************************************************/
void PreviewWidget::leaveEvent(QEvent *event)
{
    QFrame::leaveEvent(event);
    mHoverTimer->stop();
    mHoverPage = nullptr;
}


/************************************************
 * The page may have gone - deleted, or the sheet turned - while the timer
 * ran, so it is looked up again under the pointer.
 ************************************************/
void PreviewWidget::showPageToolTip()
{
    const Sheet *sheet = project->currentSheet();
    if (!sheet || !mHoverPage)
        return;

    const int n = pageAt(mHoverPos);
    if (n < 0 || sheet->page(n) != mHoverPage)
        return;

    // Tied to the page's rect, so it goes away when the pointer leaves it.
    QToolTip::showText(mapToGlobal(mHoverPos), pageToolTip(sheet, n), this,
                       pageRect(n).toRect());
}


/************************************************

 ************************************************/
QString PreviewWidget::pageToolTip(const Sheet *sheet, int pageNumOnSheet)
{
    const ProjectPage *page = sheet->page(pageNumOnSheet);
    if (!page)
        return QString();

    const Unit unit = currentUnit();
    auto size = [unit](const QSizeF &s)
    {
        return QString("%1 × %2 %3")
                .arg(toUnit(s.width(),  unit), 0, 'f', unitDecimals(unit))
                .arg(toUnit(s.height(), unit), 0, 'f', unitDecimals(unit))
                .arg(unitSuffix(unit));
    };

    QStringList rows;
    auto row = [&rows](const QString &name, const QString &value)
    {
        rows << QString("<tr><td style='padding-right: 12px'>%1</td><td>%2</td></tr>")
                .arg(name.toHtmlEscaped(), value.toHtmlEscaped());
    };

    // Where it comes from.
    const int jobNum = project->jobs()->indexOfProjectPage(page);
    if (jobNum >= 0)
    {
        const Job job = project->jobs()->at(jobNum);
        if (page->isBlankPage())
            row(tr("Source"), tr("Blank page inserted in job %1, %2").arg(jobNum + 1).arg(job.title()));
        else
            row(tr("Source"), tr("Job %1, %2: page %3 of %4")
                .arg(jobNum + 1).arg(job.title())
                .arg(job.indexOfPage(page) + 1).arg(job.pageCount()));
    }

    // Its size, and what is left of it after trimming.
    row(tr("Page size"), size(page->rect().size()));
    if (project->trimWhitespace() && page->trimRect() != page->rect())
        row(tr("Trimmed to"), size(page->trimRect().size()));

    if (page->manualRotation() != NoRotate)
        row(tr("Rotated"), tr("%1° by hand").arg(int(page->manualRotation())));

    // Where it goes.
    const int printSheet = project->printSheetIndex(page);
    if (printSheet >= 0)
        row(tr("Printed on"), project->sheetDescription(printSheet));

    const TransformSpec spec = project->layout()->transformSpec(sheet, pageNumOnSheet, project->rotation());
    row(tr("Scale"), QString("%1%").arg(spec.scale * 100, 0, 'f', 1));

    // Against the printer's own limits, for the side it is printed on.
    QString warning;
    if (!project->pageOverflow(sheet, pageNumOnSheet).isNull())
        warning = tr("Part of this page lies beyond the printer's limit (the red dashed "
                     "line); anything printed there will be cut off.");

    QString res = QString("<b>%1</b>").arg(tr("Page %1 of %2")
                                          .arg(page->pageNum() + 1)
                                          .arg(project->pageCount()));
    res += "<table>" + rows.join("") + "</table>";
    if (!warning.isEmpty())
        res += QString("<p style='color: #c62828'>%1</p>").arg(warning.toHtmlEscaped());
    return res;
}
