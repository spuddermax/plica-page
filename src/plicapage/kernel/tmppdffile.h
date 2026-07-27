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


#ifndef TMPPDFFILE_H
#define TMPPDFFILE_H

#include <QHash>
#include <QObject>
#include <QVector>
#include "plicapagetypes.h"

class Sheet;
class Job;
class JobList;
class ProjectPage;

namespace PDF {
    class Writer;
}

#include "job.h"

class TmpPdfFile: public QObject
{
    Q_OBJECT
    friend class PdfMerger;
public:
    explicit TmpPdfFile(QObject *parent = 0);
    virtual ~TmpPdfFile();

    void merge(const JobList &jobs);
    void updateSheets(const QList<Sheet *> &sheets);

    QString fileName() const { return mFileName; }

    bool writeDocument(const QList<Sheet*> &sheets, QIODevice *out);
    bool isValid() const { return mValid; }

    /**
     * The document as merge() left it: a valid PDF holding one page per source
     * page. updateSheets() appends a sheet layer whose catalog supersedes that
     * page tree, so the bytes have to be cut back to mOrigFileSize to see the
     * source pages again.
     */
    QByteArray baseDocument() const;

    /// The box each page of baseDocument() is rendered from, in page order.
    const QVector<QRectF> &pageRects() const { return mPageRects; }

    /// Index of a page within baseDocument(), or -1 if it is not in there.
    int pageIndex(const ProjectPage *page) const;

signals:
    void merged();
    void progress(int progress, int all) const;

private:
    void getPageStream(QString *out, const Sheet *sheet) const;
    void writeSheets(QIODevice *out, const QList<Sheet *> &sheets) const;
    void writeCatalog(PDF::Writer *writer, const QVector<PdfPageInfo> &pages);

    QString mFileName;
    qint32 mFirstFreeNum;
    qint64 mOrigFileSize;
    qint64 mOrigXrefPos;
    bool mValid;
    QVector<QRectF> mPageRects;
    QHash<uint, int> mXObjToPage;
};


#endif // TMPPDFFILE_H
