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


#include "kernel/project.h"
#include "settings.h"
#include "job.h"

#include "tmppdffile.h"
#include "sheet.h"
#include "layout.h"
#include "pagetrimmer.h"
#include "duplex.h"
#include "iofiles/infile.h"
#include "iofiles/boofile.h"

#include <unistd.h>
#include <algorithm>
#include <fstream>
#include <QCoreApplication>
#include <QString>
#include <QStringList>
#include <QDir>
#include <QDebug>
#include <QFile>
#include <QMessageBox>
#include <QDateTime>
#include <QUuid>
#include <QUrl>

#define META_SIZE (4 * 1024)

using namespace  std;

class ProjectState
{
public:
    explicit ProjectState(const Project *p):
        mProject(p),
        mCurrentPage(p->currentPage()),
        mCurrentSheet(p->currentSheet())
    {
    }

    const ProjectPage *currentPage() const { return mCurrentPage; }
    const Sheet *currentSheet() const { return mCurrentSheet; }

    bool currentPageChanged() const { return mCurrentPage != mProject->currentPage(); }
    bool currentSheetChanged() const { return mCurrentSheet != mProject->currentSheet(); }

private:
    const Project *mProject;
    const ProjectPage *mCurrentPage;
    const Sheet *mCurrentSheet;
};


/************************************************

 ************************************************/
Project::Project(QObject *parent) :
    QObject(parent),
    mLayout(0),
    mCurrentPage(0),
    mCurrentSheet(0),
    mSheetCount(0),
    mTmpFile(0),
    mLastTmpFile(0),
    mNullPrinter("Fake"),
    mPrinter(&mNullPrinter),
    mDoubleSided(true),
    mTrimWhitespace(false),
    mTrimUniform(false),
    mTrimPadding(0),
    mFoldLine(FoldLineNone),
    mStitchSpacing(144),
    mFoldSides(FoldSidesAll),
    mStitchMarks(false),
    mFoldLineColor(153, 153, 153),
    mSelectionAnchor(nullptr),
    mInkBoxesReady(false),
    mRotation(NoRotate)
{
}


/************************************************

 ************************************************/
Project::~Project()
{
    free();
}


/************************************************

 ************************************************/
void Project::free()
{
    for (const Job &job: mJobs)
    {
        if (job.fileName().endsWith(AUTOREMOVE_EXT))
        {
            QFile(job.fileName()).remove();
        }
    }

    mJobs.clear();
    delete mTmpFile;
}


/************************************************

 ************************************************/
TmpPdfFile *Project::createTmpPdfFile()
{
    TmpPdfFile *res = new TmpPdfFile(this);

    connect(res, SIGNAL(progress(int,int)),
            this, SLOT(tmpFileProgress(int,int)));

    connect(res, SIGNAL(merged()),
            this, SLOT(tmpFileMerged()));

    return res;
}


/************************************************
 *
 * ***********************************************/
void Project::addJob(const Job &job)
{
    addJobs(JobList() << job);
}


/************************************************
 *
 * ***********************************************/
void Project::addJobs(const JobList &jobs)
{
    try
    {
        foreach (Job job, jobs)
        {
            mJobs << job;
        }

        stopMerging();
        update();

        mLastTmpFile = createTmpPdfFile();
        mLastTmpFile->merge(mJobs);
    }
    catch (PlicaPageError &err)
    {
        qWarning() << Q_FUNC_INFO << err.what();
        error(err.what());
    }
}


/************************************************

 ************************************************/
void Project::removeJob(int index)
{
    try
    {

        stopMerging();

        QString fileName = mJobs.at(index).fileName();
        mJobs.removeAt(index);
        update();

        if (fileName.endsWith(AUTOREMOVE_EXT))
        {
            bool remove = true;
            for (const Job &job: mJobs)
                remove = remove && job.fileName() != fileName;

            if (remove)
                QFile(fileName).remove();
        }

        mLastTmpFile = createTmpPdfFile();
        mLastTmpFile->merge(mJobs);
    }
    catch (PlicaPageError &err)
    {
        qWarning() << Q_FUNC_INFO << err.what();
        error(err.what());
    }
}


/************************************************

 ************************************************/
void Project::moveJob(int from, int to)
{
    mJobs.move(from, to);
    update();
}


/************************************************

 ************************************************/
void Project::tmpFileMerged()
{
    TmpPdfFile *tmpPdf = qobject_cast<TmpPdfFile*>(sender());
    if (!tmpPdf)
        return;

    if (tmpPdf != mLastTmpFile)
    {
        tmpPdf->deleteLater();
        return;
    }

//    foreach (const Job &job, mJobs)
//    {
//        for (int p=0; p<job.pageCount(); ++p)
//        {
//            ProjectPage *page = job.page(p);
//            page->setPdfInfo(tmpPdf->pageInfo(job, page->jobPageNum()));
//        }

//    }

    delete mTmpFile;
    mTmpFile = mLastTmpFile;
    mLastTmpFile = 0;

    // New pages, so whatever was scanned before no longer describes them.
    mInkBoxesReady = false;
    mUniformInkBox = QRectF();

    if (mMetaData.title().isEmpty() && !mJobs.isEmpty())
    {
        mMetaData.setTitle(mJobs.first().title());
    }

    update();
}


/************************************************

 ************************************************/
void Project::update()
{
    ProjectState state(this);
    ProjectPage *curPage = 0;

    // Before any geometry is computed: the layout asks pages for their
    // trimRect(), which is only meaningful once the ink boxes are known.
    updateInkBoxes();

    mPages.clear();
    foreach(const Job &job, mJobs)
    {
        for (int p=0; p<job.pageCount(); ++p)
        {
            ProjectPage *page = job.page(p);
            if (page->visible())
            {
                page->setPageNum(mPages.count());
                mPages << page;
                if (page == mCurrentPage)
                    curPage = page;
            }
        }
    }
    mRotation = calcRotation(mPages, mLayout);

    // A selected page that is no longer shown - deleted, or gone with its
    // job - drops out of the selection. Compared by address only: the page
    // may already have been freed.
    {
        const int before = mSelectedPages.count();
        for (int i = mSelectedPages.count() - 1; i >= 0; --i)
        {
            if (!mPages.contains(mSelectedPages.at(i)))
                mSelectedPages.removeAt(i);
        }

        if (!mPages.contains(mSelectionAnchor))
            mSelectionAnchor = nullptr;

        if (mSelectedPages.count() != before)
            emit selectionChanged();
    }

    if (!mPages.isEmpty())
    {
        mCurrentPage = curPage ? curPage : mPages.first();
    }
    else
    {
        mCurrentPage = 0;
    }


    mSheetCount = 0;

    qDeleteAll(mPreviewSheets);
    mPreviewSheets.clear();
    mPrintSheetOfPage.clear();
    bool emitTmpFileRenamed = false;

    if (!mPages.isEmpty())
    {
        mLayout->updatePages(mPages);
        mSheetCount = mLayout->calcSheetCount();

        Direction direction = settings->value(Settings::RightToLeft).toBool() ? RightToLeft : LeftToRight;
        mLayout->fillPreviewSheets(&mPreviewSheets, direction);

        // The preview shows reading spreads, not printed sheets, so record
        // which printed sheet each page lands on.
        QList<Sheet*> printSheets;
        mLayout->fillSheets(&printSheets);
        for (int s = 0; s < printSheets.count(); ++s)
        {
            for (int i = 0; i < printSheets.at(s)->count(); ++i)
            {
                if (printSheets.at(s)->page(i))
                    mPrintSheetOfPage.insert(printSheets.at(s)->page(i), s);
            }
        }
        qDeleteAll(printSheets);

        if (mTmpFile)
        {
            mTmpFile->updateSheets(mPreviewSheets);
            emitTmpFileRenamed = true;
        }
    }

    foreach (Sheet *s, mPreviewSheets)
    {
        for (int i=0; i<s->count(); ++i)
        {
            if (s->page(i))
                s->page(i)->setSheet(s);

        }
    }

    if (mCurrentPage)
        mCurrentSheet = mCurrentPage->sheet();
    else
        mCurrentSheet = 0;




    if (emitTmpFileRenamed)
        emit tmpFileRenamed(mTmpFile->fileName());

    if (state.currentSheetChanged())
    {
        emit currentSheetChanged(currentSheet());
        emit currentSheetChanged(currentSheetNum());
    }

    if (state.currentPageChanged() || state.currentSheetChanged())
    {
        emit currentPageChanged(currentPage());
        emit currentPageChanged(currentPageNum());
    }

    emit changed();
}


/************************************************
 *
 ************************************************/
int Project::currentPageNum() const
{
    if (mCurrentPage)
        return mCurrentPage->pageNum();

    return -1;
}


/************************************************
 *
 ************************************************/
int Project::previewPageCount() const
{
    return mSheetCount * mLayout->pagePerSheet();
}


/************************************************
 *
 ************************************************/
int Project::previewPageNum(int pageNum) const
{
    if (pageNum < 0)
        return -1;

    const ProjectPage *page = mPages.at(pageNum);
    return mLayout->previewPageNum(page->sheet()->sheetNum()) + page->sheet()->indexOfPage(page);
}


/************************************************
 *
 ************************************************/
int Project::currentPreviewPage() const
{
    if (!mCurrentSheet)
        return 0;

    if (!mCurrentPage)
        return mLayout->previewPageNum(mCurrentSheet->sheetNum());


    return mLayout->previewPageNum(mCurrentSheet->sheetNum()) + mCurrentSheet->indexOfPage(mCurrentPage);
}


/************************************************
 *
 ************************************************/
void Project::deletePage(ProjectPage *page)
{
    // Now the current will be the next visible page.
    // If we delete the last page, the current to be previous.
    ProjectPage *nextCurPage = nextVisiblePage(page);
    if (!nextCurPage)
        nextCurPage = prevVisiblePage(page);


    if (page->isBlankPage())
    {
        int n = mJobs.indexOfProjectPage(page);
        if (n<0)
            return;

        mJobs.value(n).removePage(page);
    }
    else
    {
        page->hide();
    }


    mCurrentPage = nextCurPage;
    update();
}


/************************************************
 *
 ************************************************/
void Project::undoDeletePage(ProjectPage *page)
{
    if (page->visible())
        return;

    page->show();
    mCurrentPage = page;
    update();
}


/************************************************
 * Restores several pages with a single re-layout. The current page becomes
 * the first of them that was actually deleted.
 ************************************************/
void Project::undoDeletePages(const QList<ProjectPage*> &pages)
{
    ProjectPage *first = nullptr;
    foreach (ProjectPage *page, pages)
    {
        if (page->visible())
            continue;

        page->show();
        if (!first)
            first = page;
    }

    if (!first)
        return;

    mCurrentPage = first;
    update();
}


/************************************************
 * Deletes several pages with a single re-layout. The current page becomes
 * the first remaining page after the last one deleted, or failing that the
 * last remaining page before the first.
 ************************************************/
void Project::deletePages(const QList<ProjectPage*> &pages)
{
    QList<ProjectPage*> doomed;
    foreach (ProjectPage *page, mPages)
    {
        if (pages.contains(page))
            doomed << page;
    }

    if (doomed.isEmpty())
        return;

    ProjectPage *nextCurPage = nullptr;
    for (int i = mPages.indexOf(doomed.last()) + 1; i < mPages.count() && !nextCurPage; ++i)
    {
        if (!doomed.contains(mPages.at(i)))
            nextCurPage = mPages.at(i);
    }

    for (int i = mPages.indexOf(doomed.first()) - 1; i >= 0 && !nextCurPage; --i)
    {
        if (!doomed.contains(mPages.at(i)))
            nextCurPage = mPages.at(i);
    }

    foreach (ProjectPage *page, doomed)
    {
        if (page->isBlankPage())
        {
            int n = mJobs.indexOfProjectPage(page);
            if (n >= 0)
                mJobs.value(n).removePage(page);
        }
        else
        {
            page->hide();
        }
    }

    mCurrentPage = nextCurPage;
    update();
}


/************************************************
 *
 ************************************************/
void Project::deleteSelectedPages()
{
    deletePages(selectedPages());
}


/************************************************
 *
 ************************************************/
QList<ProjectPage*> Project::selectedPages() const
{
    QList<ProjectPage*> res;
    foreach (ProjectPage *page, mPages)
    {
        if (mSelectedPages.contains(page))
            res << page;
    }
    return res;
}


/************************************************
 *
 ************************************************/
void Project::selectPage(ProjectPage *page)
{
    if (!mPages.contains(page))
        return;

    mSelectedPages.clear();
    mSelectedPages << page;
    mSelectionAnchor = page;
    emit selectionChanged();
}


/************************************************
 *
 ************************************************/
void Project::togglePageSelection(ProjectPage *page)
{
    if (!mPages.contains(page))
        return;

    if (!mSelectedPages.removeOne(page))
        mSelectedPages << page;

    mSelectionAnchor = page;
    emit selectionChanged();
}


/************************************************
 * From the page last clicked to this one, either way round. With nothing
 * clicked yet, the range starts at the current page.
 ************************************************/
void Project::selectPageRange(ProjectPage *page, bool add)
{
    int to = mPages.indexOf(page);
    if (to < 0)
        return;

    ProjectPage *anchor = mSelectionAnchor ? mSelectionAnchor : mCurrentPage.data();
    int from = mPages.indexOf(anchor);
    if (from < 0)
        from = to;

    if (!add)
        mSelectedPages.clear();

    for (int i = qMin(from, to); i <= qMax(from, to); ++i)
    {
        if (!mSelectedPages.contains(mPages.at(i)))
            mSelectedPages << mPages.at(i);
    }

    // The anchor stays put, so further Shift+clicks re-span from it.
    mSelectionAnchor = mPages.at(from);
    emit selectionChanged();
}


/************************************************
 *
 ************************************************/
void Project::clearSelection()
{
    if (mSelectedPages.isEmpty())
        return;

    mSelectedPages.clear();
    emit selectionChanged();
}


/************************************************
 *
 ************************************************/
void Project::deletePagesEnd(ProjectPage *page)
{
    int n = jobs()->indexOfProjectPage(page);
    if (n<0)
        return;

    Job job = jobs()->value(n);

    // Now the current will be the next visible page.
    // If we delete the last page, the current to be previous.
    ProjectPage *nextCurPage = nextVisiblePage(job.page(job.pageCount()-1));
    if (!nextCurPage)
        nextCurPage = prevVisiblePage(page);


    QList<ProjectPage*> deleted;
    for (int i = job.indexOfPage(page); i < job.pageCount(); ++i)
    {
        ProjectPage *p = job.page(i);
        if (p->isBlankPage())
            deleted << p;
        else
            p->hide();
    }

    job.removePages(deleted);
    mCurrentPage = nextCurPage;
    update();
}


/************************************************
 *
 ************************************************/
void Project::insertBlankPageBefore(ProjectPage *page)
{
    int j = jobs()->indexOfProjectPage(page);
    if (j<0)
        return;

    Job job = jobs()->value(j);
    mCurrentPage = job.insertBlankPage(job.indexOfPage(page));
    this->update();
}


/************************************************
 *
 ************************************************/
void Project::insertBlankPageAfter(ProjectPage *page)
{
    int j = jobs()->indexOfProjectPage(page);
    if (j<0)
        return;

    Job job = jobs()->value(j);
    mCurrentPage = job.insertBlankPage(job.indexOfPage(page) + 1);
    this->update();
}


/************************************************
 *
 ************************************************/
ProjectPage *Project::prevVisiblePage(ProjectPage *current) const
{
    int n = mPages.indexOf(current) - 1;

    for (; n > -1; n--)
    {
        if (mPages.at(n)->visible())
            return mPages.at(n);
    }

    return NULL;
}


/************************************************
 *
 ************************************************/
ProjectPage *Project::nextVisiblePage(ProjectPage *current) const
{
    int n = mPages.indexOf(current) + 1;
    if (n == 0)
        return NULL;

    for (; n < mPages.size(); n++)
    {
        if (mPages.at(n)->visible())
            return mPages.at(n);
    }

    return NULL;

}


/************************************************
 *
 ************************************************/
void Project::setCurrentPage(ProjectPage *page)
{  
    if (page == mCurrentPage)
        return;

    if (mPreviewSheets.empty())
        return;

    ProjectState state(this);

    if (page)
    {
        mCurrentPage = page;
        mCurrentSheet = page->sheet();
    }
    else
    {
        mCurrentPage = 0;
        mCurrentSheet = 0;
    }

    if (state.currentSheetChanged())
    {
        emit currentSheetChanged(currentSheet());
        emit currentSheetChanged(currentSheetNum());
    }

    if (state.currentPageChanged() || state.currentSheetChanged())
    {
        emit currentPageChanged(currentPage());
        emit currentPageChanged(currentPageNum());
    }
}


/************************************************
 *
 ************************************************/
void Project::setCurrentPage(int pageNum)
{
    if (mPages.empty())
        return;

    pageNum = qBound(0, pageNum, pageCount()-1);
    setCurrentPage(mPages.at(pageNum));
}


/************************************************
 *
 ************************************************/
void Project::prevPage()
{
    setCurrentPage(mCurrentPage - 1);
}


/************************************************
 *
 ************************************************/
void Project::nextPage()
{
    setCurrentPage(mCurrentPage + 1);
}


/************************************************
 *
 ************************************************/
Sheet *Project::currentSheet() const
{
    return mCurrentSheet;
}


/************************************************
 *
 ************************************************/
int Project::currentSheetNum() const
{
    if (mCurrentSheet)
        return mCurrentSheet->sheetNum();

    return -1;
}


/************************************************
 *
 ************************************************/
void Project::setCurrentSheet(int sheetNum)
{
    if (sheetNum == currentSheetNum())
        return;

    if (mPreviewSheets.empty())
        return;

    ProjectState state(this);

    sheetNum = qBound(0, sheetNum, mPreviewSheets.count()-1);
    mCurrentSheet = mPreviewSheets.at(sheetNum);
    mCurrentPage = mCurrentSheet->firstVisiblePage();

    if (state.currentSheetChanged())
    {
        emit currentSheetChanged(currentSheet());
        emit currentSheetChanged(currentSheetNum());
    }

    if (state.currentPageChanged() || state.currentSheetChanged())
    {
        emit currentPageChanged(currentPage());
        emit currentPageChanged(currentPageNum());
    }
}


/************************************************
 *
 ************************************************/
void Project::prevSheet()
{
    setCurrentSheet(currentSheetNum() -1 );
}


/************************************************
 *
 ************************************************/
void Project::nextSheet()
{
    setCurrentSheet(currentSheetNum() + 1 );
}


/************************************************
 *
 * ***********************************************/
Rotation Project::calcRotation(const QList<ProjectPage *> &pages, const Layout *layout) const
{
    foreach (const ProjectPage *page, pages)
    {
        if (page)
        {
            if ((isLandscape(page->pdfRotation()) ^ isLandscape(page->rect())) ^ isLandscape(layout->rotation()))
                return Rotate90;
            else
                return NoRotate;
        }
    }
    return layout->rotation();
}


/************************************************

 ************************************************/
void Project::stopMerging()
{
    if (mLastTmpFile)
    {
        mLastTmpFile->deleteLater();
        mLastTmpFile = 0;
    }
}


/************************************************

 ************************************************/
void Project::tmpFileProgress(int progr, int all) const
{
    if (sender() == mLastTmpFile)
        emit progress(progr, all);
}


/************************************************

 ************************************************/
bool Project::error(const QString &message) const
{
    QMessageBox::critical(0, tr("PlicaPage", "Error message title"), message);
    qWarning() << message;
    return false;
}


/************************************************

 ************************************************/
QList<Sheet*> Project::selectSheets(Project::PagesType pages, Project::PagesOrder order) const
{
    int start = 0;
    int inc = 0;
    int end = sheetCount();

    switch (pages)
    {
    case Project::OddPages:
        start = 0;
        inc = 2;
        break;

    case Project::EvenPages:
        start = 1;
        inc = 2;
        break;

    case Project::AllPages:
        start = 0;
        inc = 1;
        break;
    }

    QList<Sheet *> sheets;
    mLayout->fillSheets(&sheets);
    QList<Sheet *> res;

    for (int i=start; i < end; i += inc)
    {
        Sheet *sheet = sheets.at(i);
        sheets[i] = 0;

        if (order == Project::ForwardOrder)
            res.append(sheet);
        else
            res.insert(0, sheet);
    }

    qDeleteAll(sheets);

    return res;
}


/************************************************

 ************************************************/
bool Project::writeDocument(const QList<Sheet*> &sheets, QIODevice *out)
{
    return mTmpFile->writeDocument(sheets, out);
}


/************************************************

 ************************************************/
bool Project::writeDocument(const QList<Sheet*> &sheets, const QString &fileName)
{
    QFile f(fileName);
    if (!f.open(QIODevice::WriteOnly))
        return project->error(tr("I can't write to file '%1'").arg(fileName) + "\n" + f.errorString());

    bool res = writeDocument(sheets, &f);
    f.close();

    return res;
}


/************************************************
 *
 ************************************************/
bool Project::doubleSided() const
{
    if (mLayout->id() == "Booklet")
        return true;
    else
        return mDoubleSided;
}


/************************************************

 ************************************************/
void Project::setLayout(const Layout *layout)
{
    mLayout = layout;
    update();
}


/************************************************
 *
 ************************************************/
void Project::setDoubleSided(bool value)
{
    mDoubleSided = value;
    emit changed();
}


/************************************************
 * The three trim setters all change page geometry, so they need a full
 * update() to rewrite the sheet layer - emit changed() alone would only
 * refresh the widgets and leave the PDF as it was.
 ************************************************/
void Project::setTrimWhitespace(bool value)
{
    if (mTrimWhitespace == value)
        return;

    mTrimWhitespace = value;
    update();
}


/************************************************
 *
 ************************************************/
void Project::setTrimUniform(bool value)
{
    if (mTrimUniform == value)
        return;

    mTrimUniform = value;
    update();
}


/************************************************
 *
 ************************************************/
void Project::setTrimPadding(qreal points)
{
    if (qFuzzyCompare(mTrimPadding, points))
        return;

    mTrimPadding = points;

    // Padding is applied on top of the ink boxes, so they stay valid.
    if (mTrimWhitespace)
        update();
}


/************************************************

 ************************************************/
int Project::paperCount() const
{
    return doubleSided() ? (mSheetCount + 1) / 2 : mSheetCount;
}


/************************************************

 ************************************************/
int Project::paperOf(int printSheetIndex) const
{
    return doubleSided() ? printSheetIndex / 2 : printSheetIndex;
}


/************************************************

 ************************************************/
bool Project::isSecondSide(int printSheetIndex) const
{
    return doubleSided() && (printSheetIndex % 2);
}


/************************************************

 ************************************************/
QString Project::sheetDescription(int printSheetIndex) const
{
    QString res = tr("Sheet %1 of %2", "Status bar")
            .arg(paperOf(printSheetIndex) + 1)
            .arg(paperCount());

    if (doubleSided())
    {
        const bool booklet = mLayout->id() == "Booklet";
        const bool second  = isSecondSide(printSheetIndex);
        res += ", " + (booklet ? (second ? tr("inside",  "Booklet sheet side")
                                         : tr("outside", "Booklet sheet side"))
                               : (second ? tr("back",    "Sheet side")
                                         : tr("front",   "Sheet side")));
    }
    return res;
}


/************************************************

 ************************************************/
QMarginsF Project::hardwareMargins(int printSheetIndex) const
{
    const QMarginsF hw = mPrinter->hardwareMargins();
    const PrinterProfile *profile = mPrinter->currentProfile();
    if (!profile)
        return hw;

    const bool turned = sheetGoesThroughTurned(printSheetIndex, doubleSided(), *profile, *mPrinter,
                                               mLayout->flipType(profile->flipType()),
                                               mRotation);
    return turned ? turnMargins(hw) : hw;
}


/************************************************
 * The pattern of turned sides repeats every two printed sheets, so the first
 * two cover them all.
 ************************************************/
QMarginsF Project::hardwareLimit() const
{
    const QMarginsF a = hardwareMargins(0);
    if (!doubleSided())
        return a;

    const QMarginsF b = hardwareMargins(1);
    return QMarginsF(qMax(a.left(), b.left()),   qMax(a.top(), b.top()),
                     qMax(a.right(), b.right()), qMax(a.bottom(), b.bottom()));
}


/************************************************
 * Preview sheets place pages exactly where the printed sheets do - only which
 * pages share a sheet differs for a booklet - so the slot's rect is the
 * printed rect, and the printed side comes from printSheetIndex().
 ************************************************/
QMarginsF Project::pageOverflow(const Sheet *sheet, int pageNumOnSheet) const
{
    const ProjectPage *page = sheet->page(pageNumOnSheet);
    if (!page)
        return QMarginsF();

    const int printSheet = printSheetIndex(page);
    const QMarginsF hw = hardwareMargins(printSheet >= 0 ? printSheet : sheet->sheetNum());
    if (hw.isNull())
        return QMarginsF();

    const QRectF printable = mPrinter->paperRect().marginsRemoved(hw);
    const QRectF rect = mLayout->transformSpec(sheet, pageNumOnSheet, mRotation).rect;

    // Hundredths of a point are rounding, not paper.
    auto over = [](qreal v) { return v > 0.01 ? v : 0.0; };
    return QMarginsF(over(printable.left()   - rect.left()),
                     over(printable.top()    - rect.top()),
                     over(rect.right()       - printable.right()),
                     over(rect.bottom()      - printable.bottom()));
}


/************************************************

 ************************************************/
QList<Project::ClippedPage> Project::clippedPages() const
{
    QMap<int, ClippedPage> found;   // by page number, for page order
    foreach (const Sheet *sheet, mPreviewSheets)
    {
        for (int i = 0; i < sheet->count(); ++i)
        {
            ProjectPage *page = const_cast<Sheet*>(sheet)->page(i);
            if (!page)
                continue;

            const QMarginsF overflow = pageOverflow(sheet, i);
            if (overflow.isNull())
                continue;

            ClippedPage c;
            c.page = page;
            c.overflow = overflow;
            c.rect = mLayout->transformSpec(sheet, i, mRotation).rect;
            found.insert(page->pageNum(), c);
        }
    }
    return found.values();
}


/************************************************
 * "5-8", "2, 4, 6-7" for page numbers from 1, in order.
 ************************************************/
static QString pageRanges(QList<int> nums)
{
    std::sort(nums.begin(), nums.end());
    QStringList res;
    for (int i = 0; i < nums.count(); )
    {
        int j = i;
        while (j + 1 < nums.count() && nums.at(j + 1) == nums.at(j) + 1)
            ++j;
        res << ((j > i) ? QString("%1\u2013%2").arg(nums.at(i)).arg(nums.at(j))
                        : QString::number(nums.at(i)));
        i = j + 1;
    }
    return res.join(", ");
}


/************************************************
 * Edges are named as the page is seen in the preview, where a landscape
 * sheet is shown turned a quarter clockwise (see
 * PreviewWidget::sheetToWidget()); a booklet page's sides are its outer edge
 * and its spine. The margins to change are named in the layout's own frame,
 * the frame of the Margins tab.
 ************************************************/
QStringList Project::printableAreaReport(bool *fits) const
{
    *fits = true;
    const QMarginsF limit = hardwareLimit();
    if (limit.isNull() || mPages.isEmpty())
        return QStringList();

    const Unit unit = currentUnit();
    auto len = [unit](qreal points)
    {
        return QString("%1 %2").arg(toUnit(points, unit), 0, 'f', unitDecimals(unit))
                               .arg(unitSuffix(unit));
    };

    const QList<ClippedPage> clipped = clippedPages();
    if (clipped.isEmpty())
        return QStringList() << tr("Every page fits within what %1 can print.").arg(mPrinter->name());

    *fits = false;

    const bool landscape = isLandscape(mRotation);
    const bool booklet = landscape && mLayout->id() == "Booklet";
    const qreal foldY = mPrinter->pageRect().center().y();

    // Each page's losses, worded; pages worded alike are reported together.
    QStringList order;
    QHash<QString, QList<int> > pagesByLoss;
    QMarginsF needed;   // layout frame: edges some page crosses
    foreach (const ClippedPage &c, clipped)
    {
        const QMarginsF o = c.overflow;
        const QMarginsF d = landscape ? QMarginsF(o.bottom(), o.left(), o.top(), o.right()) : o;

        QList<QPair<QString, qreal> > edges;
        edges << qMakePair(tr("top"), d.top()) << qMakePair(tr("bottom"), d.bottom());
        if (booklet)
        {
            // A page on the left of the spread has its outer edge on the left.
            const bool leftHalf = c.rect.center().y() > foldY;
            edges << qMakePair(tr("outer edge"), leftHalf ? d.left()  : d.right())
                  << qMakePair(tr("spine"),      leftHalf ? d.right() : d.left());
        }
        else
        {
            edges << qMakePair(tr("left"), d.left()) << qMakePair(tr("right"), d.right());
        }

        QStringList parts;
        for (const auto &e : edges)
            if (e.second > 0)
                parts << tr("%1 at the %2", "e.g. 0.37 in at the outer edge").arg(len(e.second), e.first);
        const QString loss = parts.join(tr(" and "));

        if (!pagesByLoss.contains(loss))
            order << loss;
        pagesByLoss[loss] << c.page->pageNum() + 1;

        needed = QMarginsF(qMax(needed.left(), o.left()),   qMax(needed.top(), o.top()),
                           qMax(needed.right(), o.right()), qMax(needed.bottom(), o.bottom()));
    }

    QStringList res;
    res << tr("%1 of %2 pages will be cut off by %3:")
           .arg(clipped.count()).arg(mPages.count()).arg(mPrinter->name());

    foreach (const QString &loss, order)
    {
        const QList<int> nums = pagesByLoss.value(loss);
        res << ((nums.count() == 1) ? tr("Page %1 loses %2.") : tr("Pages %1 lose %2."))
               .arg(pageRanges(nums), loss);
    }

    // Margins at the printer's limit fit any page: name only the ones some
    // page crosses, grouping those that need the same value.
    QStringList fieldOrder;
    QHash<QString, QStringList> fieldsByValue;
    auto need = [&](const QString &field, qreal crossed, qreal value)
    {
        if (crossed <= 0)
            return;
        const QString v = len(value);
        if (!fieldsByValue.contains(v))
            fieldOrder << v;
        fieldsByValue[v] << field;
    };
    need(tr("Top"),    needed.top(),    limit.top());
    need(tr("Bottom"), needed.bottom(), limit.bottom());
    need(tr("Left"),   needed.left(),   limit.left());
    need(tr("Right"),  needed.right(),  limit.right());

    QStringList settingsParts;
    foreach (const QString &v, fieldOrder)
    {
        const QStringList fields = fieldsByValue.value(v);
        QString names = fields.count() > 1
                ? QStringList(fields.mid(0, fields.count() - 1)).join(", ") + tr(" and ") + fields.last()
                : fields.first();
        settingsParts << ((fields.count() > 1) ? tr("the %1 margins to at least %2")
                                               : tr("the %1 margin to at least %2")).arg(names, v);
    }
    res << tr("To fit every page, set %1 in Printer settings \u2192 Margins.")
           .arg(settingsParts.join(tr("; ")));

    return res;
}


/************************************************
 * Exactly to the limit, not past it: the most of the paper the printer can
 * reach. An edge no page crosses keeps the margin it has, even below the
 * limit - it is not costing anything.
 ************************************************/
bool Project::fitMarginsToPrinter()
{
    PrinterProfile *profile = mPrinter->currentProfile();
    if (!profile)
        return false;

    QMarginsF crossed;
    foreach (const ClippedPage &c, clippedPages())
        crossed = QMarginsF(qMax(crossed.left(), c.overflow.left()),   qMax(crossed.top(), c.overflow.top()),
                            qMax(crossed.right(), c.overflow.right()), qMax(crossed.bottom(), c.overflow.bottom()));
    if (crossed.isNull())
        return false;

    const QMarginsF limit = hardwareLimit();
    bool changed = false;
    if (crossed.left()   > 0 && profile->leftMargin()   < limit.left())   { profile->setLeftMargin(limit.left(), UnitPoint);     changed = true; }
    if (crossed.top()    > 0 && profile->topMargin()    < limit.top())    { profile->setTopMargin(limit.top(), UnitPoint);       changed = true; }
    if (crossed.right()  > 0 && profile->rightMargin()  < limit.right())  { profile->setRightMargin(limit.right(), UnitPoint);   changed = true; }
    if (crossed.bottom() > 0 && profile->bottomMargin() < limit.bottom()) { profile->setBottomMargin(limit.bottom(), UnitPoint); changed = true; }

    if (changed)
    {
        mPrinter->saveSettings();
        update();
    }
    return changed;
}


/************************************************
 * The fold line is drawn into the sheet layer, so like the trim settings it
 * needs a full update() rather than just changed().
 ************************************************/
void Project::setFoldLine(FoldLine value)
{
    if (mFoldLine == value)
        return;

    mFoldLine = value;
    update();
}


/************************************************
 *
 ************************************************/
void Project::setStitchSpacing(qreal points)
{
    if (qFuzzyCompare(mStitchSpacing, points))
        return;

    mStitchSpacing = points;

    if (mFoldLine != FoldLineNone && mStitchMarks)
        update();
}


/************************************************
 *
 ************************************************/
void Project::setFoldSides(FoldSides value)
{
    if (mFoldSides == value)
        return;

    mFoldSides = value;

    if (mFoldLine != FoldLineNone)
        update();
}


/************************************************
 *
 ************************************************/
void Project::setStitchMarks(bool value)
{
    if (mStitchMarks == value)
        return;

    mStitchMarks = value;

    if (mFoldLine != FoldLineNone)
        update();
}


/************************************************
 * An invalid color, e.g. from a hand-edited settings file, is ignored.
 ************************************************/
void Project::setFoldLineColor(const QColor &value)
{
    if (!value.isValid() || mFoldLineColor == value)
        return;

    mFoldLineColor = value;

    if (mFoldLine != FoldLineNone)
        update();
}


/************************************************
 * Rasterizes every source page once and records where its ink sits.
 *
 * Runs against TmpPdfFile::baseDocument() rather than the file on disk: by the
 * time this is first called the sheet layer has usually been appended, and its
 * catalog hides the per-source-page tree this needs.
 ************************************************/
void Project::updateInkBoxes()
{
    if (mInkBoxesReady || !mTrimWhitespace || !mTmpFile || !mTmpFile->isValid())
        return;

    const QByteArray baseDoc = mTmpFile->baseDocument();
    if (baseDoc.isEmpty())
        return;

    PageTrimmer trimmer;
    connect(&trimmer, SIGNAL(progress(int,int)), this, SIGNAL(progress(int,int)));

    const QVector<QRectF> inkBoxes = trimmer.scan(baseDoc, mTmpFile->pageRects());

    mUniformInkBox = QRectF();
    foreach (const Job &job, mJobs)
    {
        for (int p = 0; p < job.pageCount(); ++p)
        {
            ProjectPage *page = job.page(p);
            const int idx = mTmpFile->pageIndex(page);

            if (idx < 0 || idx >= inkBoxes.count())
                continue;

            const QRectF box = inkBoxes.at(idx);
            page->setInkBox(box);

            // Blank pages must not drag the shared box out to the full sheet.
            if (box.isValid() && !box.isEmpty())
                mUniformInkBox = mUniformInkBox.isValid() ? mUniformInkBox.united(box)
                                                          : box;
        }
    }

    mInkBoxesReady = true;
    emit progress(-1, -1);
}


/************************************************

 ************************************************/
void Project::setPrinterProfile(Printer *printer, int profile, bool update)
{
    if (printer)
    {
        mPrinter = printer;
        mPrinter->setCurrentProfile(profile);
    }
    else
    {
        mPrinter = &mNullPrinter;
    }

    if (update)
    {
        this->update();
    }
}


/************************************************

 ************************************************/
Project *Project::instance()
{
    static Project *inst = 0;
    if (!inst)
        inst = new Project();

    return inst;
}


#if 0
/************************************************

 ************************************************/
QByteArray MetaData::asXMP() const
{
    QString date = QDateTime::currentDateTime().toString(Qt::ISODate);
    QByteArray res;
    res.reserve(META_SIZE);

    res.append("<?xpacket begin=\"").append("\xEF\xBB\xBF", 3).append("\" id=\"W5M0MpCehiHzreSzNTczkc9d\"?>\n");
    xmp(res, "<x:xmpmeta xmlns:x=\"adobe:ns:meta/\" x:xmptk=\"PlicaPage\">");
    xmp(res, "  <rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">");

    xmp(res, "    <rdf:Description rdf:about=\"\" xmlns:xmp=\"http://ns.adobe.com/xap/1.0/\">");
    xmp(res, "      <xmp:ModifyDate>%1</xmp:ModifyDate>", date);
    xmp(res, "      <xmp:CreateDate>%1</xmp:CreateDate>", date);
    xmp(res, "      <xmp:MetadataDate>%1</xmp:MetadataDate>", date);
    xmp(res, "      <xmp:CreatorTool>PlicaPage Version %1</xmp:CreatorTool>", FULL_VERSION);
    xmp(res, "    </rdf:Description>");

    xmp(res, "    <rdf:Description rdf:about=\"\" xmlns:dc=\"http://purl.org/dc/elements/1.1/\">");
    xmp(res, "      <dc:format>application/pdf</dc:format>");

    if (!mTitle.isEmpty())
    {
        xmp(res, "      <dc:title>");
        xmp(res, "        <rdf:Alt>");
        xmp(res, "          <rdf:li xml:lang=\"x-default\">%1</rdf:li>", mTitle);
        xmp(res, "        </rdf:Alt>");
        xmp(res, "      </dc:title>");
    }

    if (!mCreator.isEmpty())
    {
        xmp(res, "      <dc:creator>");
        xmp(res, "        <rdf:Seq>");
        xmp(res, "          <rdf:li>%1</rdf:li>", mCreator);
        xmp(res, "        </rdf:Seq>");
        xmp(res, "      </dc:creator>");
    }

    if (!mSubject.isEmpty())
    {
        xmp(res, "      <dc:subject>");
        xmp(res, "        <rdf:Bag>");
        xmp(res, "          <rdf:li>%1</rdf:li>", mSubject);
        xmp(res, "        </rdf:Bag>");
        xmp(res, "      </dc:subject>");
    }

    if (!mDescription.isEmpty())
    {
        xmp(res, "      <dc:description>");
        xmp(res, "        <rdf:Alt>");
        xmp(res, "          <rdf:li xml:lang=\"x-default\">%1</rdf:li>", mDescription);
        xmp(res, "        </rdf:Alt>");
        xmp(res, "      </dc:description>");
    }

    xmp(res, "    </rdf:Description>");

    xmp(res, "  </rdf:RDF>");
    xmp(res, "</x:xmpmeta>");

    int n = res.length();
    res = res.leftJustified(META_SIZE - 21, ' ');
    for (int i=n+100; i<res.length(); i+=100)
        res[i]='\n';

    xmp(res, "\n<?xpacket end=\"w\"?>");
    return res;
}


/************************************************

 ************************************************/
void MetaData::xmp(QByteArray &out, const QString &format) const
{
    out.append(format.toUtf8().data());
    out.append('\n');
}


/************************************************

 ************************************************/
void MetaData::xmp(QByteArray &out, const QString &format, const QString &value) const
{
    QString v = value;
    v.replace("'",  "&apos;");
    v.replace("\"", "&quot;");
    v.replace("&",  "&amp;");
    v.replace("<",  "&lt;");
    v.replace(">",  "&gt;");

    out.append(QString(format).arg(v).toUtf8());
    out.append('\n');
}

#endif

/************************************************

 ************************************************/
QByteArray MetaData::asPDFDict() const
{
    QByteArray res;
    res.reserve(META_SIZE);
    QDateTime now = QDateTime::currentDateTime();

    if (!mTitle.isEmpty())
        addDictItem(res, "Title",    mTitle);

    if (!mAuthor.isEmpty())
        addDictItem(res, "Author",   mAuthor);

    if (!mSubject.isEmpty())
        addDictItem(res, "Subject",  mSubject);

    if (!mKeywords.isEmpty())
        addDictItem(res, "Keywords", mKeywords);

    addDictItem(res, "CreationDate", now);  // The date and time the document was created
    addDictItem(res, "ModDate",      now);  // The date and time the document was most recently modified

    return res;
}


/************************************************

 ************************************************/
void MetaData::addDictItem(QByteArray &out, const QString &key, const QString &value) const
{
    out.append("/" + key + " <FEFF");
    const ushort* utf16 = value.utf16();
    for (int i=0; utf16[i]>0; ++i)
    {
#if Q_BYTE_ORDER == Q_BIG_ENDIAN
        qint8 b2 = (utf16[i] & 0xFF00) >> 8;
        qint8 b1 = (utf16[i] & 0x00FF);
#else
        qint8 b1 = (utf16[i] & 0xFF00) >> 8;
        qint8 b2 = (utf16[i] & 0x00FF);
#endif
        out.append(QString("%1%2").arg(b1, 2, 16, QChar('0')).arg(b2, 2, 16, QChar('0')));
    }
    out.append(">\n");
}


/************************************************
 *
 * ***********************************************/
void MetaData::addDictItem(QByteArray &out, const QString &key, const QDateTime &value) const
{
    QDateTime utc = value.toUTC();
    utc.setTimeSpec(Qt::LocalTime);
    int offset = utc.secsTo(value) / 60;

    out.append("/" + key + " (");
    out.append(value.toString("yyyyMMddhhmmss"));

    if (offset > 0)
        out.append(QString("+%1'%2'")
                   .arg(offset / 60, 2, 10, QChar('0'))
                   .arg(offset % 60, 2, 10, QChar('0')));
    else if (offset < 0)
        out.append(QString("-%1'%2'")
                    .arg(-offset / 60, 2, 10, QChar('0'))
                    .arg(-offset % 60, 2, 10, QChar('0')));

    out.append(")\n");
}


/************************************************

 ************************************************/
JobList Project::load(const QString &fileName)
{
    return load(QStringList() << fileName);
}


/************************************************

 ************************************************/
JobList Project::load(const QStringList &fileNames)
{
    stopMerging();
    QStringList errors;

    QStringList delFiles;
    JobList jobs;
    foreach(QString fileName, fileNames)
    {
        try
        {
            InFile::Type fileType = InFile::getType(fileName);
            if (fileType == InFile::Type::CupsBoo &&
                fileName.endsWith(AUTOREMOVE_EXT))
            {
                QString old = fileName;
                fileName = genTmpFileName(".cboo");
                QFile::copy(old, fileName);
                delFiles << old;
            }

            QObject keeper;
            InFile *parser = InFile::fromFile(fileName, &keeper);
            connect(parser, &InFile::startLongOperation,
                    [this, &parser](const QString &msg){
                auto task = new ProjectLongTask(msg, parser);
                emit longTaskStarted(task);
            });

            parser->load(fileName);
            jobs << parser->jobs();

            if (fileType == InFile::Type::Boo)
            {
                setMetadata(parser->metaData());
            }
        }
        catch (const PlicaPageError &err)
        {
            qWarning() << err.what();
            errors << err.what();
        }
    }


    foreach(QString fileName, delFiles)
    {
        QFile(fileName).remove();
    }

    if (!jobs.isEmpty())
        addJobs(jobs);

    if (!errors.isEmpty())
        error(errors.join("\n\n"));

    return jobs;
}


/************************************************

 ************************************************/
void Project::save(const QString &fileName)
{
    BooFile file;
    file.setMetadata(mMetaData);
    file.setJobs(mJobs);
    file.save(fileName);
}


/************************************************
 *
 ************************************************/
ProjectLongTask::ProjectLongTask(const QString &title, QObject *parent):
    QObject(parent),
    mTitle(title)
{

}


/************************************************
 *
 ************************************************/
ProjectLongTask::~ProjectLongTask()
{
    emit finished();
}
