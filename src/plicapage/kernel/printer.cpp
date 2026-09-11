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


#include "printer.h"
#include "settings.h"
#include "cupsprinteroptions.h"
#include "layout.h"

#include <QProcess>
#include <QSharedData>
#include <QDebug>
#include <QDir>
#include <QTemporaryFile>

#define A4_HEIGHT_PT    842
#define A4_WIDTH_PT     595

// Uncomment to log the lpr command line and open each job in a viewer instead
// of relying on the printer. Must be visible to both print() and printFile().
//#define DEBUG_PRINT

// toUnit()/fromUnit() live in plicapagetypes.cpp, alongside the Unit enum.


/************************************************

 ************************************************/
PrinterProfile::PrinterProfile():
    mLeftMargin(0),
    mRightMargin(0),
    mTopMargin(0),
    mBottomMargin(0),
    mInternalMargin(14),
    mPrintOffsetX(0),
    mPrintOffsetY(0),
    mDuplexType(DuplexManual),
    mDrawBorder(false),
    mReverseOrder(false),
    mPaperSize(QSizeF(A4_WIDTH_PT, A4_HEIGHT_PT)),
    mColorMode(ColorModeAuto),
    mFlipType(FlipType::LongEdge),
    // Kept separate from mFlipType, which belongs to the duplexer. The two want
    // different defaults: long edge is the normal choice for a duplexer, while
    // short edge plus reversed order is what the old DuplexManualReverse meant,
    // and that was the default duplex type. Sharing one field would regress one
    // or the other.
    mManualFlipType(FlipType::ShortEdge),
    mManualDuplexReversesOrder(true),
    mManualDuplexHandling(HandlingFlipSideways),
    mDuplexCalibrated(false),
    mDuplexCalibrationDeclined(false)
{

}


/************************************************
 *
 ************************************************/
PrinterProfile &PrinterProfile::operator=(const PrinterProfile &other)
{
    mName           = other.mName;
    mLeftMargin     = other.mLeftMargin;
    mRightMargin    = other.mRightMargin;
    mTopMargin      = other.mTopMargin;
    mBottomMargin   = other.mBottomMargin;
    mInternalMargin = other.mInternalMargin;
    mDuplexType     = other.mDuplexType;
    mDrawBorder     = other.mDrawBorder;
    mReverseOrder   = other.mReverseOrder;
    mPaperSize      = other.mPaperSize;
    mColorMode      = other.mColorMode;
    mFlipType       = other.mFlipType;
    mManualFlipType = other.mManualFlipType;
    // This operator is hand-written, and PrinterSettings copies profiles by
    // value on every OK. A member missing from here is silently discarded.
    mManualDuplexReversesOrder = other.mManualDuplexReversesOrder;
    mManualDuplexHandling      = other.mManualDuplexHandling;
    mDuplexCalibrated          = other.mDuplexCalibrated;
    mDuplexCalibrationDeclined = other.mDuplexCalibrationDeclined;
    mPrinterOptions            = other.mPrinterOptions;
    mPaperSizeName             = other.mPaperSizeName;
    mPrintOffsetX              = other.mPrintOffsetX;
    mPrintOffsetY              = other.mPrintOffsetY;

    return *this;
}


/************************************************
 *
 ************************************************/
void PrinterProfile::setName(const QString &name)
{
    mName = name;
}


/************************************************

 ************************************************/
qreal PrinterProfile::leftMargin(Unit unit) const
{
    return toUnit(mLeftMargin, unit);
}


/************************************************

 ************************************************/
void PrinterProfile::setLeftMargin(qreal value, Unit unit)
{
    mLeftMargin = fromUnit(value, unit);
}


qreal PrinterProfile::printOffsetX(Unit unit) const { return toUnit(mPrintOffsetX, unit); }
void  PrinterProfile::setPrintOffsetX(qreal value, Unit unit) { mPrintOffsetX = fromUnit(value, unit); }
qreal PrinterProfile::printOffsetY(Unit unit) const { return toUnit(mPrintOffsetY, unit); }
void  PrinterProfile::setPrintOffsetY(qreal value, Unit unit) { mPrintOffsetY = fromUnit(value, unit); }


/************************************************

 ************************************************/
qreal PrinterProfile::rightMargin(Unit unit) const
{
    return toUnit(mRightMargin, unit);
}


/************************************************

 ************************************************/
void PrinterProfile::setRightMargin(qreal value, Unit unit)
{
    mRightMargin = fromUnit(value, unit);
}


/************************************************

 ************************************************/
qreal PrinterProfile::topMargin(Unit unit) const
{
    return toUnit(mTopMargin, unit);
}


/************************************************

 ************************************************/
void PrinterProfile::setTopMargin(qreal value, Unit unit)
{
    mTopMargin = fromUnit(value, unit);
}


/************************************************

 ************************************************/
qreal PrinterProfile::bottomMargin(Unit unit) const
{
    return toUnit(mBottomMargin, unit);
}


/************************************************

 ************************************************/
void PrinterProfile::setBottomMargin(qreal value, Unit unit)
{
    mBottomMargin = fromUnit(value, unit);
}


/************************************************

 ************************************************/
qreal PrinterProfile::internalMargin(Unit unit) const
{
    return toUnit(mInternalMargin, unit);
}


/************************************************

 ************************************************/
void PrinterProfile::setInternalMargin(qreal value, Unit unit)
{
    mInternalMargin = fromUnit(value, unit);
}


/************************************************
 *
 ************************************************/
void PrinterProfile::setDuplexType(DuplexType duplexType)
{
    mDuplexType = duplexType;
}


/************************************************
 *
 ************************************************/
void PrinterProfile::setDrawBorder(bool value)
{
    mDrawBorder = value;
}


/************************************************
 *
 ************************************************/
void PrinterProfile::setReverseOrder(bool value)
{
    mReverseOrder = value;
}


/************************************************
 *
 ************************************************/
void PrinterProfile::setManualFlipType(FlipType value)
{
    mManualFlipType = value;
}


/************************************************
 *
 ************************************************/
void PrinterProfile::setManualDuplexReversesOrder(bool value)
{
    mManualDuplexReversesOrder = value;
}


/************************************************
 *
 ************************************************/
void PrinterProfile::setManualDuplexHandling(ManualDuplexHandling value)
{
    mManualDuplexHandling = value;
}


/************************************************
 *
 ************************************************/
void PrinterProfile::setDuplexCalibrated(bool value)
{
    mDuplexCalibrated = value;
}


/************************************************
 *
 ************************************************/
void PrinterProfile::setDuplexCalibrationDeclined(bool value)
{
    mDuplexCalibrationDeclined = value;
}


/************************************************
 *
 ************************************************/
void PrinterProfile::setColorMode(ColorMode value)
{
    mColorMode = value;
}


/************************************************

 ************************************************/
QSizeF PrinterProfile::paperSize(Unit unit) const
{
    return QSizeF(toUnit(mPaperSize.width(),  unit),
                  toUnit(mPaperSize.height(), unit));
}


/************************************************

************************************************/
void PrinterProfile::setPaperSize(const QSizeF &paperSize, Unit unit)
{
    mPaperSize.setWidth( fromUnit(paperSize.width(),  unit));
    mPaperSize.setHeight(fromUnit(paperSize.height(), unit));
}


/************************************************

************************************************/
void PrinterProfile::setFlipType(FlipType value)
{
    mFlipType = value;
}


/************************************************

 ************************************************/
/************************************************
 * An empty choice means "use the queue's default", so drop the key rather than
 * store a blank that would reach lpr as "-o Option=".
 ************************************************/
void PrinterProfile::setPrinterOption(const QString &option, const QString &choice)
{
    if (choice.isEmpty())
        mPrinterOptions.remove(option);
    else
        mPrinterOptions.insert(option, choice);
}


void PrinterProfile::readSettings()
{
    mName           = settings->value(Settings::PrinterProfile_Name,            mName).toString();
    mLeftMargin     = settings->value(Settings::PrinterProfile_LeftMargin,      mLeftMargin).toDouble();
    mRightMargin    = settings->value(Settings::PrinterProfile_RightMargin,     mRightMargin).toDouble();
    mTopMargin      = settings->value(Settings::PrinterProfile_TopMargin,       mTopMargin).toDouble();
    mBottomMargin   = settings->value(Settings::PrinterProfile_BottomMargin,    mBottomMargin).toDouble();
    mInternalMargin = settings->value(Settings::PrinterProfile_InternalMargin,  mInternalMargin).toDouble();
    mPaperSizeName  = settings->value(Settings::PrinterProfile_PaperSize,       mPaperSizeName).toString();
    mPrintOffsetX   = settings->value(Settings::PrinterProfile_PrintOffsetX,    mPrintOffsetX).toDouble();
    mPrintOffsetY   = settings->value(Settings::PrinterProfile_PrintOffsetY,    mPrintOffsetY).toDouble();

    QString s = settings->value(Settings::PrinterProfile_DuplexType, duplexTypeToStr(mDuplexType)).toString();
    mDuplexType = strToDuplexType(s);

    mDrawBorder     = settings->value(Settings::PrinterProfile_DrawBorder,      mDrawBorder).toBool();
    mReverseOrder   = settings->value(Settings::PrinterProfile_ReverseOrder,    mReverseOrder).toBool();

    s = settings->value(Settings::PrinterProfile_ColorMode, colorModeToStr(mColorMode)).toString();
    mColorMode      = strToColorMode(s);

    s = settings->value(Settings::PrinterProfile_FlipType, flipTypeToStr(mFlipType)).toString();
    mFlipType = strToFlipType(s);

    // Manual duplex used to be described by DuplexType alone, which tied the
    // flip edge to the stacking order and so could only express two of the four
    // combinations that actually occur. Those two facts are now stored
    // separately. A profile written before that change has no key for the new
    // one, so derive it from the old enum:
    //
    //     DuplexManual        -> long edge,  order preserved
    //     DuplexManualReverse -> short edge, order reversed
    //
    // DuplexCalibrated is absent in such a profile too, so the wizard will offer
    // itself once and can then record something the old model could not hold.
    s = settings->value(Settings::PrinterProfile_ManualFlipType, flipTypeToStr(mManualFlipType)).toString();
    mManualFlipType = strToFlipType(s);

    const QVariant reverses = settings->value(Settings::PrinterProfile_ManualDuplexReversesOrder);
    const QVariant storedDuplex = settings->value(Settings::PrinterProfile_DuplexType);

    if (reverses.isValid())
    {
        mManualDuplexReversesOrder = reverses.toBool();
    }
    else if (storedDuplex.isValid())
    {
        const DuplexType legacy = strToDuplexType(storedDuplex.toString());
        mManualDuplexReversesOrder = (legacy == DuplexManualReverse);
        mManualFlipType = (legacy == DuplexManualReverse) ? FlipType::ShortEdge
                                                          : FlipType::LongEdge;
    }
    // Neither key present means a brand new profile, so the constructor's
    // defaults stand.

    // Both legacy manual values now mean the same thing - "the user turns the
    // paper" - with the difference carried by the two fields above. Collapse it,
    // or the settings dialog would be asked to show a duplex type that is no
    // longer in its list.
    if (mDuplexType == DuplexManualReverse)
        mDuplexType = DuplexManual;

    s = settings->value(Settings::PrinterProfile_ManualDuplexHandling,
                        manualDuplexHandlingToStr(mManualDuplexHandling)).toString();
    mManualDuplexHandling = strToManualDuplexHandling(s);

    mDuplexCalibrated = settings->value(Settings::PrinterProfile_DuplexCalibrated,
                                        mDuplexCalibrated).toBool();
    mDuplexCalibrationDeclined = settings->value(Settings::PrinterProfile_DuplexCalibrationDeclined,
                                                 mDuplexCalibrationDeclined).toBool();

    // The PPD options live in their own subgroup of the profile because their
    // keys are the driver's own keywords, not ours.
    mPrinterOptions.clear();
    settings->beginGroup(settings->keyToString(Settings::PrinterProfile_OptionsGroup));
    foreach (const QString &key, settings->childKeys())
        mPrinterOptions.insert(key, settings->QSettings::value(key).toString());
    settings->endGroup();
}


/************************************************

 ************************************************/
void PrinterProfile::saveSettings() const
{
    settings->setValue(Settings::PrinterProfile_Name,           mName);
    settings->setValue(Settings::PrinterProfile_LeftMargin,     mLeftMargin);
    settings->setValue(Settings::PrinterProfile_RightMargin,    mRightMargin);
    settings->setValue(Settings::PrinterProfile_TopMargin,      mTopMargin);
    settings->setValue(Settings::PrinterProfile_BottomMargin,   mBottomMargin);
    settings->setValue(Settings::PrinterProfile_InternalMargin, mInternalMargin);
    settings->setValue(Settings::PrinterProfile_PaperSize,      mPaperSizeName);
    settings->setValue(Settings::PrinterProfile_PrintOffsetX,   mPrintOffsetX);
    settings->setValue(Settings::PrinterProfile_PrintOffsetY,   mPrintOffsetY);

    settings->setValue(Settings::PrinterProfile_DuplexType,     duplexTypeToStr(mDuplexType));
    settings->setValue(Settings::PrinterProfile_DrawBorder,     mDrawBorder);
    settings->setValue(Settings::PrinterProfile_ReverseOrder,   mReverseOrder);
    settings->setValue(Settings::PrinterProfile_ManualFlipType, flipTypeToStr(mManualFlipType));
    settings->setValue(Settings::PrinterProfile_ManualDuplexReversesOrder, mManualDuplexReversesOrder);
    settings->setValue(Settings::PrinterProfile_ManualDuplexHandling,
                       manualDuplexHandlingToStr(mManualDuplexHandling));
    settings->setValue(Settings::PrinterProfile_DuplexCalibrated,          mDuplexCalibrated);
    settings->setValue(Settings::PrinterProfile_DuplexCalibrationDeclined, mDuplexCalibrationDeclined);
    settings->setValue(Settings::PrinterProfile_ColorMode,      colorModeToStr(mColorMode));
    settings->setValue(Settings::PrinterProfile_FlipType,       flipTypeToStr(mFlipType));

    settings->remove(settings->keyToString(Settings::PrinterProfile_OptionsGroup));
    settings->beginGroup(settings->keyToString(Settings::PrinterProfile_OptionsGroup));
    for (auto it = mPrinterOptions.constBegin(); it != mPrinterOptions.constEnd(); ++it)
        settings->QSettings::setValue(it.key(), it.value());
    settings->endGroup();
}


/************************************************

 ************************************************/
Printer::Printer(const QString &printerName):
    mCanChangeDuplexType(true),
    mPrinterName(printerName),
    mCurrentProfileIndex(-1),
    mCurrentProfile(0)
{
    CupsPrinterOptions cupsOpts(mPrinterName);

    mDeviceUri = cupsOpts.deviceURI();
    if (!cupsOpts.paperSize().isEmpty())
    {
        mDefaultCupsProfile.setPaperSize(cupsOpts.paperSize(), UnitPoint);
        mDefaultCupsProfile.setLeftMargin(cupsOpts.leftMargin(), UnitPoint);
        mDefaultCupsProfile.setRightMargin(cupsOpts.rightMargin(), UnitPoint);
        mDefaultCupsProfile.setTopMargin(cupsOpts.topMargin(), UnitPoint);
        mDefaultCupsProfile.setBottomMargin(cupsOpts.bottomMargin(), UnitPoint);
    }

    if (cupsOpts.duplex())
        mDefaultCupsProfile.setDuplexType(DuplexAuto);

    mGrayscaleOption = cupsOpts.grayScaleOption();
    mColorOption     = cupsOpts.colorOption();

    {
        PpdOptions ppd(mPrinterName);
        mPaperSizes = ppd.paperSizes();
        mDefaultPaperSizeName = ppd.defaultPaperSize();
    }

    readSettings();
    for (int i = 0; i < mProfiles.count(); ++i)
        resolvePaperSize(mProfiles[i]);
}


/************************************************

 ************************************************/
void Printer::resolvePaperSize(PrinterProfile &profile) const
{
    const QString name = profile.paperSizeName();
    if (!name.isEmpty())
        foreach (const PpdPaperSize &p, mPaperSizes)
            if (p.keyword == name)
            {
                profile.setPaperSize(p.size, UnitPoint);
                return;
            }

    // Nothing chosen, or a size this PPD no longer lists: the queue default.
    profile.setPaperSizeName(QString());
    profile.setPaperSize(mDefaultCupsProfile.paperSize(UnitPoint), UnitPoint);
}


/************************************************
 *
 ************************************************/
Printer::~Printer()
{

}


/************************************************
 *
 ************************************************/
void Printer::setProfiles(const QVector<PrinterProfile> &value)
{
    mProfiles = value;
    for (int i = 0; i < mProfiles.count(); ++i)
        resolvePaperSize(mProfiles[i]);
}


/************************************************
 *
 ************************************************/
void Printer::setCurrentProfile(int index)
{
    mCurrentProfileIndex = index;
    mCurrentProfile = &mProfiles[mCurrentProfileIndex];
}


/************************************************

 ************************************************/
void Printer::readSettings()
{
    settings->beginGroup(QString("Printer_%1").arg(mPrinterName));
    mCurrentProfileIndex = settings->value(Settings::Printer_CurrentProfile, 0).toInt();
    int size = settings->beginReadArray("Profiles");
    for (int i=0; i<size; ++i)
    {
        PrinterProfile profile;
        profile = mDefaultCupsProfile;

        settings->setArrayIndex(i);
        profile.readSettings();
        mProfiles.append(profile);
    }
    settings->endArray();

    if (mProfiles.isEmpty())
    {
        PrinterProfile profile;
        profile = mDefaultCupsProfile;

        profile.readSettings();
        profile.setName(QObject::tr("Default", "Printer profile default name"));
        mProfiles.append(profile);
    }
    settings->endGroup();

    if (mCurrentProfileIndex < 0 || mCurrentProfileIndex >= mProfiles.count())
        mCurrentProfileIndex = 0;

    mCurrentProfile = &mProfiles[mCurrentProfileIndex];
}


/************************************************

 ************************************************/
void Printer::saveSettings()
{
    settings->beginGroup(QString("Printer_%1").arg(mPrinterName));
    settings->setValue(Settings::Printer_CurrentProfile, mCurrentProfileIndex);

    settings->beginWriteArray("Profiles");
    for (int i=0; i<mProfiles.count(); ++i)
    {
        settings->setArrayIndex(i);
        mProfiles[i].saveSettings();
    }
    settings->endArray();
    settings->endGroup();
}


/************************************************

 ************************************************/
QList<Printer*> Printer::availablePrinters()
{
    static QList<Printer*>* printers = 0;

    if (!printers)
    {
        printers = new QList<Printer*>;

        QList<QPrinterInfo> piList = QPrinterInfo::availablePrinters();
        foreach (const QPrinterInfo &pi, piList)
        {
            Printer *printer = new Printer(pi.printerName());
            if (printer->deviceUri() != CUPS_BACKEND_URI)
                *printers << printer;
            else
                delete printer;
        }
    }

    return QList<Printer *>(*printers);
}


/************************************************
 *
 ************************************************/
Printer *Printer::printerByName(const QString &printerName)
{
    foreach(Printer *printer, availablePrinters())
    {
        if (printer->name() == printerName)
            return printer;
    }

    return 0;
}


/************************************************
 *
 ************************************************/
Printer *Printer::nullPrinter()
{
    static Printer nullPrinter = Printer("Fake");
    return &nullPrinter;
}


/************************************************

 ************************************************/
QRectF Printer::paperRect(Unit unit) const
{
    return QRectF(QPointF(0, 0), paperSize(unit));
}


/************************************************

 ************************************************/
QRectF Printer::pageRect(Unit unit) const
{
    return paperRect(unit).adjusted(
                mCurrentProfile->leftMargin(),
                mCurrentProfile->topMargin(),
                -mCurrentProfile->rightMargin(),
                -mCurrentProfile->bottomMargin());
}


/************************************************

 ************************************************/
bool Printer::isSupportColor() const
{
    return !mColorOption.isEmpty();
}


/************************************************

 ************************************************/
bool Printer::print(const QList<Sheet *> &sheets, const QString &jobName, bool doubleSided, int numCopies, bool collate) const
{

    QString file = QString("%1/.cache/plicapage_tmp_%2-print.pdf")
                          .arg(QDir::homePath())
                          .arg(QCoreApplication::applicationPid());

    project->writeDocument(sheets, file);

#ifndef DEBUG_PRINT
    return printFile(file, jobName, doubleSided, numCopies, collate);
#else
    printFile(file, jobName, doubleSided, numCopies, collate);

    QString fileName;
    {
        QTemporaryFile f;
        f.open();
        fileName = f.fileName();
        f.close();
    }

    project->writeDocument(sheets, fileName);
    QProcess::startDetached("okular", QStringList() << fileName);
    return true;
#endif
}


/************************************************
 * Spools a PDF that is already on disk. Split out of print() so the duplex
 * calibration sheets, which are drawn rather than composed from Sheets, can go
 * to the printer the same way a real job does.
 ************************************************/
bool Printer::printFile(const QString &fileName, const QString &jobName, bool doubleSided, int numCopies, bool collate) const
{
    QStringList args;
    args << "-P" << name();                       // Prints files to the named printer.
    args << "-#" << QString("%1").arg(numCopies); // Sets the number of copies to print
    args << "-T" << jobName;                      // Sets the job name.
    args << "-r";                                 // The print files should be deleted after printing them

    // Duplex options ...........................
    if (duplexType() == DuplexAuto && doubleSided)
    {
        if (project->layout()->flipType(flipType()) == FlipType::LongEdge)
            args << "-o sides=two-sided-long-edge";
        else
            args << "-o sides=two-sided-short-edge";
    }
    else
    {
        args << "-o sides=one-sided";               // Turn off duplex printing
    }
    // Duplex options ...........................

    if (collate)
        args << "-o Collate=True";                      // Use the -o Collate=True option to get collated copies


    // Grayscale/color printing .................
    switch (mCurrentProfile->colorMode())
    {
    case ColorModeAuto:
        break;

    case ColorModeColor:
        if (!mColorOption.isEmpty())
            args << "-o " + mColorOption;
        break;

    case ColorModeGrayscale:
        if (!mGrayscaleOption.isEmpty())
            args << "-o " + mGrayscaleOption;
        break;

    }
    // Grayscale/color printing .................

    // Whatever the profile chose on the printer tab ........
    if (!mCurrentProfile->paperSizeName().isEmpty())
    {
        args << "-o PageSize=" + mCurrentProfile->paperSizeName();
        args << "-o media="    + mCurrentProfile->paperSizeName();
    }
    const QMap<QString, QString> options = mCurrentProfile->printerOptions();
    for (auto it = options.constBegin(); it != options.constEnd(); ++it)
        args << "-o " + it.key() + "=" + it.value();

    args << fileName.toLocal8Bit();

#ifdef DEBUG_PRINT
    QString s = "lpr";
    foreach (QString a, args)
    {
        s += " \"" + a + "\"";
    }
    qDebug("%s", s.toLocal8Bit().data());
#endif

    QProcess proc;
    proc.startDetached("lpr", args);
    return true;
}



