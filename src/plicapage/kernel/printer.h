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


#ifndef PRINTER_H
#define PRINTER_H

#include "plicapagetypes.h"
#include <QObject>
#include <QVector>
#include <QString>
#include <QMap>
#include <QPrinterInfo>
#include <QExplicitlySharedDataPointer>
#include <QIODevice>

class Sheet;


class PrinterProfile
{
public:
    explicit PrinterProfile();
    PrinterProfile &operator=(const PrinterProfile &other);

    QString name() const { return mName; }
    void setName(const QString &name);

    qreal leftMargin(Unit unit=UnitPoint) const;
    void setLeftMargin(qreal value, Unit unit);

    qreal rightMargin(Unit unit=UnitPoint) const;
    void setRightMargin(qreal value, Unit unit);

    qreal topMargin(Unit unit=UnitPoint) const;
    void setTopMargin(qreal value, Unit unit);

    qreal bottomMargin(Unit unit=UnitPoint) const;
    void setBottomMargin(qreal value, Unit unit);

    qreal internalMargin(Unit unit=UnitPoint) const;
    void setInternalMargin(qreal value, Unit unit);

    DuplexType duplexType() const { return mDuplexType; }
    void setDuplexType(DuplexType duplexType);

    bool drawBorder() const { return mDrawBorder; }
    void setDrawBorder(bool value);

    bool reverseOrder() const { return mReverseOrder; }
    void setReverseOrder(bool value);

    ColorMode colorMode() const { return mColorMode; }
    void setColorMode(ColorMode value);

    bool grayscale() const { return mColorMode == ColorModeGrayscale; }

    QSizeF paperSize(Unit unit) const;
    void setPaperSize(const QSizeF & paperSize, Unit unit);

    /**
     * Which edge the paper is turned about between the two sides.
     *
     * For a printer with a duplexer this is passed to CUPS. For manual duplex it
     * is the net result of the printer's paper path and the way the user puts the
     * stack back, and it decides whether the first pass must be rotated 180.
     * The calibration wizard measures it.
     */
    FlipType flipType() const { return mFlipType; }
    void setFlipType(FlipType value);

    /// The same idea as flipType(), but for turning the paper by hand.
    FlipType manualFlipType() const { return mManualFlipType; }
    void setManualFlipType(FlipType value);

    /**
     * Whether the second manual-duplex pass reaches the sheets in the opposite
     * order to the first. True for any printer that stacks its output face down,
     * because the pile is then reversed when it goes back in the tray.
     */
    bool manualDuplexReversesOrder() const { return mManualDuplexReversesOrder; }
    void setManualDuplexReversesOrder(bool value);

    /**
     * What the user does with the stack between passes. Remembered so the
     * print-time prompt can show it back to them, and because the calibrated
     * transform is only valid for this particular handling.
     */
    ManualDuplexHandling manualDuplexHandling() const { return mManualDuplexHandling; }
    void setManualDuplexHandling(ManualDuplexHandling value);

    /**
     * True only once the wizard has actually measured this printer. Kept
     * distinct from declining the offer: if the user says "not now" we must
     * stop asking, but we still do not know which way the paper goes back in
     * and must not pretend to.
     */
    bool duplexCalibrated() const { return mDuplexCalibrated; }
    void setDuplexCalibrated(bool value);

    /// True once the user has turned the offer down, so it is not asked again.
    bool duplexCalibrationDeclined() const { return mDuplexCalibrationDeclined; }
    void setDuplexCalibrationDeclined(bool value);

    /**
     * Per-job PPD options this profile sets, as option keyword -> choice
     * keyword. Anything absent is left to the queue's default.
     */
    QMap<QString, QString> printerOptions() const { return mPrinterOptions; }
    QString printerOption(const QString &option) const { return mPrinterOptions.value(option); }
    void setPrinterOption(const QString &option, const QString &choice);

    void readSettings();
    void saveSettings() const;

private:
    QString mName;
    QMap<QString, QString> mPrinterOptions;
    qreal mLeftMargin;
    qreal mRightMargin;
    qreal mTopMargin;
    qreal mBottomMargin;
    qreal mInternalMargin;
    DuplexType mDuplexType;
    bool mDrawBorder;
    bool mReverseOrder;
    QSizeF mPaperSize;
    ColorMode mColorMode;
    FlipType mFlipType;
    FlipType mManualFlipType;
    bool mManualDuplexReversesOrder;
    ManualDuplexHandling mManualDuplexHandling;
    bool mDuplexCalibrated;
    bool mDuplexCalibrationDeclined;
};


class Printer
{
public:
    explicit Printer(const QString &name);
    virtual ~Printer();

    virtual QString name() const { return mPrinterName; }

    QVector<PrinterProfile> profiles() { return mProfiles; }
    const QVector<PrinterProfile> profiles() const { return mProfiles; }
    void setProfiles(const QVector<PrinterProfile> &value);

    PrinterProfile *currentProfile() const { return mCurrentProfile; }
    int currentProfileIndex() const { return mCurrentProfileIndex; }
    void setCurrentProfile(int index);

    const PrinterProfile *defaultCupsProfile() const { return &mDefaultCupsProfile; }

    QSizeF paperSize(Unit unit) const { return mCurrentProfile->paperSize(unit); }
    void setPaperSize(const QSizeF & paperSize, Unit unit) { mCurrentProfile->setPaperSize(paperSize, unit); }

    QRectF paperRect(Unit unit=UnitPoint) const;
    QRectF pageRect(Unit unit=UnitPoint) const;

    qreal leftMargin(Unit unit=UnitPoint) const { return mCurrentProfile->leftMargin(unit); }
    void setLeftMargin(qreal value, Unit unit)  { mCurrentProfile->setLeftMargin(value, unit); }

    qreal rightMargin(Unit unit=UnitPoint)      { return mCurrentProfile->rightMargin(unit); }
    void setRightMargin(qreal value, Unit unit) { mCurrentProfile->setRightMargin(value, unit); }

    qreal topMargin(Unit unit=UnitPoint)        { return mCurrentProfile->topMargin(unit); }
    void setTopMargin(qreal value, Unit unit)   { mCurrentProfile->setTopMargin(value, unit); }

    qreal bottomMargin(Unit unit=UnitPoint)     { return mCurrentProfile->bottomMargin(unit); }
    void setBottomMargin(qreal value, Unit unit){ mCurrentProfile->setBottomMargin(value, unit); }

    qreal internalMarhin(Unit unit=UnitPoint)   { return mCurrentProfile->internalMargin(unit); }
    void setInternalMargin(qreal value, Unit unit) { mCurrentProfile->setInternalMargin(value, unit); }

    bool drawBorder() const        { return mCurrentProfile->drawBorder(); }
    void setDrawBorder(bool value) { mCurrentProfile->setDrawBorder(value); }

    DuplexType duplexType() const             { return mCurrentProfile->duplexType(); }
    void setDuplexType(DuplexType duplexType) { mCurrentProfile->setDuplexType(duplexType); }

    bool reverseOrder() const        { return mCurrentProfile->reverseOrder(); }
    void setReverseOrder(bool value) { mCurrentProfile->setReverseOrder(value); }

    ColorMode colorMode() const { return mCurrentProfile->colorMode(); }
    void setColorMode(ColorMode value) { mCurrentProfile->setColorMode(value); }

    bool grayscale() const { return mCurrentProfile->grayscale(); }
    bool isSupportColor() const;

    FlipType flipType() const { return mCurrentProfile->flipType(); }
    void setFlipType(FlipType value) { mCurrentProfile->setFlipType(value);}

    FlipType manualFlipType() const { return mCurrentProfile->manualFlipType(); }
    void setManualFlipType(FlipType value) { mCurrentProfile->setManualFlipType(value); }

    bool manualDuplexReversesOrder() const { return mCurrentProfile->manualDuplexReversesOrder(); }
    void setManualDuplexReversesOrder(bool value) { mCurrentProfile->setManualDuplexReversesOrder(value); }

    ManualDuplexHandling manualDuplexHandling() const { return mCurrentProfile->manualDuplexHandling(); }
    void setManualDuplexHandling(ManualDuplexHandling v) { mCurrentProfile->setManualDuplexHandling(v); }

    bool duplexCalibrated() const { return mCurrentProfile->duplexCalibrated(); }
    void setDuplexCalibrated(bool value) { mCurrentProfile->setDuplexCalibrated(value); }

    bool duplexCalibrationDeclined() const { return mCurrentProfile->duplexCalibrationDeclined(); }
    void setDuplexCalibrationDeclined(bool v) { mCurrentProfile->setDuplexCalibrationDeclined(v); }

    bool canChangeDuplexType() const { return mCanChangeDuplexType; }

    virtual bool print(const QList<Sheet*> &sheets, const QString &jobName, bool doubleSided, int numCopies, bool collate) const;

    /**
     * Spools a ready-made PDF through the same lpr invocation print() uses.
     * Passing doubleSided=false forces "sides=one-sided" whatever the profile
     * says, which is what the duplex calibration prints need.
     */
    bool printFile(const QString &fileName, const QString &jobName, bool doubleSided, int numCopies, bool collate) const;

    QString deviceUri() const { return mDeviceUri; }

    void readSettings();
    void saveSettings();

    static QList<Printer*> availablePrinters();
    static Printer *printerByName(const QString &name);
    static Printer *nullPrinter();

protected:
    bool mCanChangeDuplexType;

private:
    const QString mPrinterName;
    QString mDeviceUri;
    QVector<PrinterProfile> mProfiles;
    int mCurrentProfileIndex;
    PrinterProfile *mCurrentProfile;
    PrinterProfile mDefaultCupsProfile;
    QString mGrayscaleOption;
    QString mColorOption;
};

#endif // PRINTER_H
