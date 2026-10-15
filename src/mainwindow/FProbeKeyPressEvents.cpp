/*******************************************************************

Part of the Fritzing project - http://fritzing.org
Copyright (c) 2023 Fritzing GmbH

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

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QApplication>
#include <QKeyEvent>
#include <QMenu>


#include "FProbeKeyPressEvents.h"

FProbeKeyPressEvents::FProbeKeyPressEvents() :
	FProbe("KeyPressEvents")
{
}

void FProbeKeyPressEvents::write(QVariant data) {
	QMap<QString, int> keyMap;
	keyMap.insert("add", Qt::Key_Plus);
	keyMap.insert("backspace", Qt::Key_Backspace);
	keyMap.insert("ctrl", Qt::Key_Control);
	keyMap.insert("decimal", Qt::Key_Period);
	keyMap.insert("del", Qt::Key_Delete);
	keyMap.insert("delete", Qt::Key_Delete);
	keyMap.insert("divide", Qt::Key_Slash);
	keyMap.insert("down", Qt::Key_Down);
	keyMap.insert("end", Qt::Key_End);
	keyMap.insert("enter", Qt::Key_Enter);
	keyMap.insert("esc", Qt::Key_Escape);
	keyMap.insert("escape", Qt::Key_Escape);
	keyMap.insert("home", Qt::Key_Home);
	keyMap.insert("insert", Qt::Key_Insert);
	keyMap.insert("left", Qt::Key_Left);
	keyMap.insert("multiply", Qt::Key_Asterisk);
	keyMap.insert("pagedown", Qt::Key_PageDown);
	keyMap.insert("pageup", Qt::Key_PageUp);
	keyMap.insert("pgdn", Qt::Key_PageDown);
	keyMap.insert("pgup", Qt::Key_PageUp);
	keyMap.insert("print", Qt::Key_Print);
	keyMap.insert("printscreen", Qt::Key_Print);
	keyMap.insert("prntscrn", Qt::Key_Print);
	keyMap.insert("prtsc", Qt::Key_Print);
	keyMap.insert("prtscr", Qt::Key_Print);
	keyMap.insert("return", Qt::Key_Return);
	keyMap.insert("right", Qt::Key_Right);
	keyMap.insert("select", Qt::Key_Select);
	keyMap.insert("space", Qt::Key_Space);
	keyMap.insert("subtract", Qt::Key_Minus);
	keyMap.insert("tab", Qt::Key_Tab);
	keyMap.insert("up", Qt::Key_Up);

	QJsonDocument doc = QJsonDocument::fromJson(data.toString().toUtf8());
	QJsonArray events = doc.array();

	for (const QJsonValue &event_val : events) {
		QJsonObject event_obj = event_val.toObject();
		QString key = event_obj["key"].toString();
		QJsonArray modifiers = event_obj["modifiers"].toArray();

		int keyCode;
		if (key.length() > 1) {
			if (keyMap.contains(key)) {
				keyCode = keyMap[key];
			} else {
				continue;
			}
		} else {
			keyCode = key.at(0).unicode(); // Convert the key string to a Qt key code
		}
		Qt::KeyboardModifiers modFlags;

		// Check for modifier keys
		for (const QJsonValue &mod : modifiers) {
			if (mod.toString() == "shift") {
				modFlags |= Qt::ShiftModifier;
			} else if (mod.toString() == "ctrl") {
				modFlags |= Qt::ControlModifier;
			} else if (mod.toString() == "alt") {
				modFlags |= Qt::AltModifier;
			}
		}

		QWidget * widget = nullptr;
		QMenu * activeMenu = qobject_cast<QMenu*>(QApplication::activePopupWidget());

		if (activeMenu) {
			widget = activeMenu;
		} else {
			widget = QApplication::focusWidget();
		}
		QApplication::postEvent(widget, new QKeyEvent(QEvent::KeyPress, keyCode, modFlags, key.at(0)));
		QApplication::postEvent(widget, new QKeyEvent(QEvent::KeyRelease, keyCode, modFlags, key.at(0)));
	}
}
