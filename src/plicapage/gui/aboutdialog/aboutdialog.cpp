/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL2+
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

#include "aboutdialog.h"
#include "ui_aboutdialog.h"
#include "translations/translatorsinfo/translatorsinfo.h"
#include <QDebug>
#include <QtCore/QDate>
#include <QPainter>


AboutDialog::AboutDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AboutDialog)
{
    ui->setupUi(this);

    QString css="<style TYPE='text/css'> "
                    "body { font-family: sans-serif;} "
                    ".name { font-size: 16pt; color: #DE7907; font-weight: bold;} "
                    ".ver { color: #D3D3D3; } "
                    "a { white-space: nowrap ;} "
                    "h2 { font-size: 10pt;} "
                    "li { line-height: 120%;} "
                    ".techInfoKey { white-space: nowrap ; margin: 0 20px 0 16px; } "
                "</style>"
            ;

    ui->iconLabel->setFixedSize(48, 48);
    ui->iconLabel->setScaledContents(true);
    ui->iconLabel->setPixmap(QPixmap(":/48/mainicon"));

    ui->nameLabel->setText(css + titleText());

    ui->aboutBrowser->setHtml(css + aboutText());
    ui->aboutBrowser->viewport()->setAutoFillBackground(false);

    ui->autorsBrowser->setHtml(css + authorsText());
    ui->autorsBrowser->viewport()->setAutoFillBackground(false);

    ui->thanksBrowser->setHtml(css + thanksText());
    ui->thanksBrowser->viewport()->setAutoFillBackground(false);
    // Upstream hid this tab, which meant the attribution the Icons8 licence
    // requires was compiled in but never shown to anyone. It stays visible.

    ui->translationsBrowser->setHtml(css + translationsText());
    ui->translationsBrowser->viewport()->setAutoFillBackground(false);

}


/************************************************

 ************************************************/
AboutDialog::~AboutDialog()
{
    delete ui;
}


/************************************************
 *
 ************************************************/
void AboutDialog::paintEvent(QPaintEvent *)
{
    QRect rect(0, 0, this->width(), ui->nameLabel->pos().y() + ui->nameLabel->height() + ui->nameLabel->pos().y());
    QPainter painter(this);
    painter.fillRect(rect, QColor::fromRgb(0x404040));
}


/************************************************

 ************************************************/
QString AboutDialog::titleText() const
{
#ifdef GIT_BRANCH
    QString ver = QString("%1 %2 <a href='https://github.com/spuddermax/plica-page/commit/%3'>%3</a>")
            .arg(FULL_VERSION, GIT_BRANCH, GIT_COMMIT_HASH);
#else
    QString ver = QString("%1").arg(FULL_VERSION);
#endif
    return QString("<div class=name>%1</div><div class=ver>%2</div>")
                .arg(APP_DISPLAY_NAME)
                .arg(tr("Version: %1").arg(ver));
}


/************************************************

 ************************************************/
QString AboutDialog::aboutText() const
{
    // Two copyright lines, deliberately. Upstream's is fixed at 2019 - the year
    // of its last release - rather than the running year, which is both accurate
    // and keeps the credit from looking like it covers work they did not do.
    return  QString("<br>%1<br><br>%2<br>%3<br><br>%4<hr>%5<p>%6").arg(
                tr("%1 provides a virtual printer for CUPS. This can be used for print preview or for print booklets.")
                    .arg(APP_DISPLAY_NAME),

                tr("Copyright: %1-%2 %3").arg("2026", QDate::currentDate().toString("yyyy"), "PlicaPage contributors"),
                tr("Based on Boomaga, copyright %1 %2").arg("2012-2019", "Boomaga team"),

                tr("Homepage: %1").arg("<a href='https://github.com/spuddermax/plica-page'>https://github.com/spuddermax/plica-page</a>"),
                tr("Upstream: %1").arg("<a href='https://github.com/Boomaga/boomaga'>https://github.com/Boomaga/boomaga</a>"),
                tr("License: %1").arg("<a href='http://www.gnu.org/licenses/lgpl-2.1.html'>GNU Lesser General Public License version 2.1 or later</a>")
                );
}


/************************************************

 ************************************************/
QString AboutDialog::authorsText() const
{
    return QString("%1<p>%2<p>%3").arg(
                tr("%1 is developed by <a %2>its contributors</a> on GitHub.")
                    .arg(APP_DISPLAY_NAME, " href='https://github.com/spuddermax/plica-page/graphs/contributors'"),
                tr("It began as a fork of <a %1>Boomaga</a>, created by Alexander Sokoloff "
                   "and the Boomaga team, whose work the great majority of this program "
                   "still is.")
                    .arg(" href='https://github.com/Boomaga/boomaga'"),
                tr("If you are interested in working with us, <a %1>join in</a>.")
                    .arg(" href='https://github.com/spuddermax/plica-page'")
                );
}


/************************************************

 ************************************************/
QString AboutDialog::thanksText() const
{
    return QString(
                "%1"
                "<ul>"
                "<li>Alexander Sokoloff and the Boomaga team "
                "(https://github.com/Boomaga/boomaga) - the program this one is built on</li>"
                "<li>CUPS project (http://www.cups.org)</li>"
                "<li>Icons8 (https://icons8.com/) - toolbar icons, CC BY-ND 3.0</li>"
                "<li>Poppler (https://poppler.freedesktop.org/) - PDF rendering</li>"
                "</ul>"
                ).arg(tr("Special thanks to:"));
}


/************************************************

 ************************************************/
QString AboutDialog::translationsText() const
{
    TranslatorsInfo translatorsInfo;
    return QString("%1<p><ul>%2</ul>").arg(
                tr("If you want to help translate, we will be glad to see you in our translation team on <a %1>Transifex server</a>.")
                    .arg(" href='https://www.transifex.com/projects/p/boomaga/'"),
                translatorsInfo.asHtml()
                );
}



