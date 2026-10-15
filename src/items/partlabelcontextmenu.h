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

#ifndef PARTLABELCONTEXTMENU_H
#define PARTLABELCONTEXTMENU_H

#include <QObject>
#include <QStringList>

class QMenu;
class QAction;
class QPoint;
class SketchWidget;
class PartLabel;

// The part label's context menu, factored out of PartLabel to mirror the part
// (item) menu: one shared instance per view, owned by the view, built once with
// slot-connected actions, and injected by MainWindow via
// SketchWidget::setPartLabelMenu(). It holds the single source of truth for the
// label's Flip/Rotate actions and dispatches every action to the view's current
// selection's label, so the SelectedPartLabel probe can drive the real menu.
class PartLabelContextMenu : public QObject {
	Q_OBJECT
public:
	explicit PartLabelContextMenu(SketchWidget * view);

	// GUI entry point. PartLabel::contextMenuEvent makes its owner the sole
	// selection, then calls this; the menu acts on that selection.
	void popup(const QPoint & screenPos);

	// The Flip/Rotate action names offered in this view (untranslated; the
	// 45/135 degree steps are present only in PCB view). Single source of truth
	// for the SelectedPartLabel probe's reported action list.
	QStringList rotateFlipActionNames() const;

	// Trigger a Flip/Rotate action by its untranslated name on the current
	// selection's label, without showing the menu (probe path). Returns false
	// if the named action is not offered in this view.
	bool triggerRotateFlip(const QString & untranslatedName);

private:
	void buildMenu();
	void prepare();                       // refresh dynamic state, on aboutToShow
	PartLabel * selectedLabel() const;    // the view's sole selected part's label

	SketchWidget * m_view = nullptr;
	QMenu * m_menu = nullptr;
	QMenu * m_flipRotateMenu = nullptr;
	QMenu * m_displayValuesMenu = nullptr;
	QAction * m_labelTextAct = nullptr;
	QAction * m_tinyAct = nullptr;
	QAction * m_smallAct = nullptr;
	QAction * m_mediumAct = nullptr;
	QAction * m_largeAct = nullptr;
	QList<QAction *> m_propertyActs;      // rebuilt each prepare()
};

#endif
