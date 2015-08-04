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

#ifndef KCMSYSTEMD_FSUTIL_H
#define KCMSYSTEMD_FSUTIL_H

#include <QString>

/**
 *
 * \brief determines the size (capacity) of an arbitrary partition in bytes.
 * Use like this:
 * \code
 * bool ok = false;
 * qulonglong size = getPartitionSize("some path", &ok);
 * if (ok)
 *    doSomething(size);
 * \code
 *
 * \param path An arbitrary path. The available space will be
 * determined for the partition containing path.
 * \param ok Error indicator (optional). If passed error status is reported by means
 * of a boolean flag which will be set to \p true if the function determined the partition
 * size successfully \p false otherwise.
 * \return the size of the partition containing the given path or 0 in case of errors.
 */
qulonglong getPartitionSize(const QString &path, bool *ok = NULL);

#endif
