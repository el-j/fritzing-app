/*******************************************************************

Part of the Fritzing project - http://fritzing.org
Copyright (c) 2026 Fritzing GmbH

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

#ifndef BUGANNOTATION_H
#define BUGANNOTATION_H

#include <QMap>
#include <QStringList>
#include <QSvgRenderer>

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QGraphicsSvgItem>
#else
#include <QtSvgWidgets/QGraphicsSvgItem>
#endif

class ItemBase;

class BugAnnotation
{
public:
	explicit BugAnnotation(ItemBase * owner);
	~BugAnnotation();

	void show(const QString & source, const QStringList & errors);
	void clear(const QString & source);
	void reposition();
	bool isActive() const;
	QString text() const;   // all current error messages joined, or empty if inactive

private:
	ItemBase * m_owner;
	QGraphicsSvgItem * m_item = nullptr;
	QMap<QString, QStringList> m_errors;

	void updateTooltip();
	void destroyItem();
	static QSvgRenderer & renderer();
};

#endif // BUGANNOTATION_H
