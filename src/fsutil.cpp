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

#include "fsutil.h"

#include <QtDebug>
#include <kdiskfreespaceinfo.h>

qulonglong getPartitionSize(const QString &path, bool *ok) {

  KDiskFreeSpaceInfo info = KDiskFreeSpaceInfo::freeSpaceInfo(path);
  bool valid = info.isValid();

  if (ok) {
    *ok = valid;
  }

  if (!valid) {
    qDebug() << "Unable to determine size of partition:" << path;
    return 0UL;
  }

  return info.size();
}
