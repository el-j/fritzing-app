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

#ifndef ITEMDECORATIONS_H
#define ITEMDECORATIONS_H

#include <QCoreApplication>
#include <QPointer>

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QGraphicsSvgItem>
#else
#include <QtSvgWidgets/QGraphicsSvgItem>
#endif

class ItemBase;
class QTimer;

// Clickable padlock shown on locked parts (and always on boards, as an open lock
// when unlocked). Double-click toggles the owner's move lock. Deliberately no
// Q_OBJECT: identified via dynamic_cast in mouse-press scans.
class LockSymbolItem : public QGraphicsSvgItem
{
public:
	explicit LockSymbolItem(ItemBase * owner);

	void setLockedAppearance(bool locked);
	void setFlashing(bool flashing);
	void applyUprightOrientation();

protected:
	void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
	void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
	void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;

private:
	ItemBase * m_owner = nullptr;
	QPointF m_basePos;
	double m_baseScale = 1.0;
	double m_baseZ = 0.0;
	bool m_flashing = false;
};

// The small status symbols at a part's corner: the lock symbol with its red warning
// flash, and the sticky symbol chained to its right. Owned by ItemBase as a value
// member, following the BugAnnotation pattern.
class ItemDecorations
{
	Q_DECLARE_TR_FUNCTIONS(ItemDecorations)

public:
	explicit ItemDecorations(ItemBase * owner);
	~ItemDecorations();

	void updateLockSymbol();
	void flashLockSymbol();
	void reorientLockSymbol();
	void setStickyVisible(bool visible);

private:
	QPointF lockSymbolPosition() const;
	bool collidesWithConnectors(const QRectF & ownerRect) const;
	void layoutStickySymbol();

	ItemBase * m_owner;
	// QPointer: during a flash the symbol floats on the scene (not a child), so scene
	// teardown may delete it before its owner
	QPointer<LockSymbolItem> m_lockItem;
	QGraphicsSvgItem * m_stickyItem = nullptr;
	QTimer * m_flashTimer = nullptr;		// parented to the owner
};

#endif // ITEMDECORATIONS_H
