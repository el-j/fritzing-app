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

#include "sanitizeforpath.h"

#include <QRegularExpression>
#include <QUuid>

QString sanitizeForPath(const QString & name) {
	// Characters illegal in folder names on Windows (and potentially problematic elsewhere)
	static const QString illegalChars = QStringLiteral("<>:\"/\\|?*");

	QString result;
	result.reserve(name.size());

	for (const QChar &ch : name) {
		if (ch.unicode() < 32) {
			// Strip control characters
			continue;
		}
		if (illegalChars.contains(ch)) {
			result.append('_');
		} else {
			result.append(ch);
		}
	}

	// Collapse path traversal: replace runs of two or more dots with underscore
	result.replace(QRegularExpression("\\.{2,}"), "_");

	// Trim leading/trailing dots and spaces (illegal on Windows)
	while (result.startsWith('.') || result.startsWith(' ')) {
		result.remove(0, 1);
	}
	while (result.endsWith('.') || result.endsWith(' ')) {
		result.chop(1);
	}

	// Truncate to 200 characters (well within 255 limit, leaving room for filenames inside)
	if (result.length() > 200) {
		result.truncate(200);
	}

	// Short names risk colliding with Windows reserved device names (CON, PRN,
	// AUX, NUL, COM1-9, LPT1-9 are all 3-4 chars). Prefix with a UUID to
	// make them safe and unique.
	if (result.length() < 6) {
		QString uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
		result = uuid + "_" + result;
	}

	return result;
}
