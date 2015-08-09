/*******************************************************************************
 * Copyright (C) 2013-2015 Ragnar Thomsen <rthomsen6@gmail.com>                *
 *                                                                             *
 * This program is free software: you can redistribute it and/or modify it     *
 * under the terms of the GNU General Public License as published by the Free  *
 * Software Foundation, either version 2 of the License, or (at your option)   *
 * any later version.                                                          *
 *                                                                             *
 * This program is distributed in the hope that it will be useful, but WITHOUT *
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or       *
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for    *
 * more details.                                                               *
 *                                                                             *
 * You should have received a copy of the GNU General Public License along     *
 * with this program. If not, see <http://www.gnu.org/licenses/>.              *
 *******************************************************************************/

#ifndef CONFOPTION_H
#define CONFOPTION_H

#include <QStringList>
#include <QVariant>
#include "kcm-systemd-features.h"

#if defined(HAVE_CXX11_CHRONO)
#include <ratio>
#include <chrono>

namespace ratio = std;
namespace chrono = std::chrono;
#else
#include <boost/ratio.hpp>
#include <boost/chrono.hpp>

namespace ratio = boost;
namespace chrono = boost::chrono;
#endif

enum settingType
{
  BOOL, TIME, INTEGER, STRING, LIST, MULTILIST, RESLIMIT, SIZE
};

enum confFile
{
  SYSTEMD, JOURNALD, LOGIND, COREDUMP
};

class confOption {
  
  public:
    typedef chrono::duration<double, ratio::nano > nanoseconds;
    typedef chrono::duration<double, ratio::micro > microseconds;
    typedef chrono::duration<double, ratio::milli > milliseconds;
    typedef chrono::duration<double> seconds;
    typedef chrono::duration<double, ratio::ratio<60, 1> > minutes;
    typedef chrono::duration<double, ratio::ratio<3600, 1> > hours;
    typedef chrono::duration<double, ratio::ratio<86400, 1> > days;
    typedef chrono::duration<double, ratio::ratio<604800, 1> > weeks;
    typedef chrono::duration<double, ratio::ratio<2629800, 1> > months; // define a month as 30.4375days
    typedef chrono::duration<double, ratio::ratio<29030400, 1> > years;
    typedef enum timeUnit { ns, us, ms, s, min, h, d, w, month, year } timeUnit;
    
    confFile file;
    settingType type;
    QString uniqueName, realName, toolTip;
    qlonglong minVal = 0, maxVal = 999999999;
    QStringList possibleVals = QStringList();
    static QStringList capabilities;
    
    confOption();
    // Used for comparing
    explicit confOption(QString newName);
    explicit confOption(QVariantMap);
    
    bool operator==(const confOption& other) const;
    int setValue(QVariant);
    int setValueFromFile(QString);
    bool isDefault() const;
    void setToDefault();
    QVariant getValue() const;
    QString getValueAsString() const;
    QString getLineForFile() const;
    QString getFilename() const;
    QString getTimeUnit() const;

  private:
    bool hasNsec = false;
    QVariant value, defVal;
    timeUnit defUnit = timeUnit::s, defReadUnit = timeUnit::s, minUnit = timeUnit::ns, maxUnit = timeUnit::year;
    QVariant convertTimeUnit(double, timeUnit, timeUnit);
};

#endif
