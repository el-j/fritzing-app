/*******************************************************************

Part of the Fritzing project - http://fritzing.org
Copyright (c) 2026 Fritzing

Fritzing is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Fritzing is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Fritzing.  If not, see <http://www.gnu.org/licenses/>.

********************************************************************/

#ifndef SANITIZEFORPATH_H
#define SANITIZEFORPATH_H

#include <QString>

/// Sanitize a string for safe use as a single folder name component.
/// Strips path separators, control characters, Windows-illegal characters,
/// and Windows reserved device names. Returns a hash-based fallback if the
/// result would be empty.
QString sanitizeForPath(const QString & name);

#endif // SANITIZEFORPATH_H
