/*******************************************************************************
 * Copyright (C) 2015 Johan Ouwerkerk <jm.ouwerkerk@gmail.com>                 *
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

#include <iostream>
#include <ratio>
#include <chrono>
#include <cstdint>

/*
 * Test case to check:
 *  - if std::ratio is present and includes predefined ratios (std::milli).
 *  - if std::chrono::duration is present and includes predefined durations (std::chrono::microseconds)
 *  - if std::chrono::duration supports mixed type arithmetic/conversion
 *  - if std::chrono::duration specifically supports using double as 'representation' type.
 *
 * This should catch out compilers with broken std::chrono::duration support.
 */

typedef std::chrono::duration<uint64_t, std::milli> msec_t;
typedef std::chrono::duration<double> sec_t;

int main()
{
    sec_t secs(3.5);
    sec_t next = secs + sec_t(1.5) - sec_t(2.0);
    msec_t msec(std::chrono::duration_cast<msec_t>(secs));
    std::chrono::microseconds usec(std::chrono::duration_cast<std::chrono::microseconds>(msec));

    bool checkNext = 3.0 == next.count();
    bool checkUsec = 3500000 == usec.count();
    bool checkSRep = sizeof(secs.count()) == sizeof(double);
    bool checkMSRep = sizeof(msec.count()) == sizeof(uint64_t);
    bool checkMsec = 3500 == msec.count();

    std::cout
        << "Original duration in (s): " << secs.count() << std::endl
        << "Converted to (ms): " << msec.count() << std::endl
        << "Converted to (us): " << usec.count() << std::endl
        << "After arithmetic (+ 1.5s - 2.0s): " << next.count() << std::endl;

    return (checkNext && checkUsec && checkSRep && checkMSRep && checkMsec) ? 0 : 1;
}
