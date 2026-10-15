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

#include "FProbeSelectedPartLabel.h"
#include "mainwindow/mainwindow.h"
#include "sketch/sketchwidget.h"
#include "items/itembase.h"
#include "items/partlabel.h"
#include "items/partlabelcontextmenu.h"
#include "debugdialog.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QGraphicsScene>
#include <QSet>

#include <cmath>

FProbeSelectedPartLabel::FProbeSelectedPartLabel(MainWindow *mainWindow)
	: QObject(mainWindow)
	, FProbe("SelectedPartLabel")
	, m_mainWindow(mainWindow)
{
	// Probes are called from the FTesting server thread; hop to the GUI
	// thread for all scene access.
	connect(this, &FProbeSelectedPartLabel::requestInfo, mainWindow, [this]() {
		QJsonObject result;
		SketchWidget *view = nullptr;
		PartLabel *partLabel = findSelectedPartLabel(view, result);
		if (partLabel) {
			ItemBase *owner = partLabel->owner();
			result["found"] = true;
			result["text"] = owner->instanceTitle();
			result["visible"] = partLabel->isVisible();

			QPointF pos = partLabel->pos();
			QJsonObject position;
			position["x"] = pos.x();
			position["y"] = pos.y();
			result["position"] = position;

			QTransform t = partLabel->transform();
			double rotation = std::atan2(t.m12(), t.m11()) * 180.0 / M_PI;
			if (rotation < 0) rotation += 360.0;
			result["rotation"] = rotation;
			result["flipped"] = (t.m11() * t.m22() - t.m12() * t.m21()) < 0;

			QRectF bounds = partLabel->sceneBoundingRect();
			QJsonObject boundsObj;
			boundsObj["x"] = bounds.x();
			boundsObj["y"] = bounds.y();
			boundsObj["width"] = bounds.width();
			boundsObj["height"] = bounds.height();
			result["bounds"] = boundsObj;

			// The available actions come from the real shared menu (single
			// source of truth; the 45/135 degree steps are PCB-only there).
			QJsonArray actions;
			if (PartLabelContextMenu * menu = view->partLabelContextMenu()) {
				for (const QString & name : menu->rotateFlipActionNames()) {
					actions.append(name);
				}
			}
			result["actions"] = actions;
		}
		m_lastResult = QVariant(QString(QJsonDocument(result).toJson(QJsonDocument::Compact)));
	}, Qt::BlockingQueuedConnection);

	connect(this, &FProbeSelectedPartLabel::requestAction, mainWindow, [this](const QString &actionName) {
		QJsonObject result;
		SketchWidget *view = nullptr;
		PartLabel *partLabel = findSelectedPartLabel(view, result);
		if (partLabel) {
			// Drive the real shared menu: triggering the action fires the same
			// connected slot the GUI uses, which rotates the selected label
			// through the undo stack.
			PartLabelContextMenu * menu = view->partLabelContextMenu();
			if (menu != nullptr && menu->triggerRotateFlip(actionName)) {
				result["found"] = true;
			}
			else {
				result["found"] = false;
				result["reason"] = QString("action '%1' is not available in this view").arg(actionName);
			}
		}
		if (!result["found"].toBool()) {
			DebugDialog::debug(QString("SelectedPartLabel write failed: %1").arg(result["reason"].toString()));
		}
		m_lastResult = QVariant(QString(QJsonDocument(result).toJson(QJsonDocument::Compact)));
	}, Qt::BlockingQueuedConnection);
}

QVariant FProbeSelectedPartLabel::read()
{
	Q_EMIT requestInfo();
	return m_lastResult;
}

void FProbeSelectedPartLabel::write(QVariant var)
{
	Q_EMIT requestAction(var.toString());
}

PartLabel * FProbeSelectedPartLabel::findSelectedPartLabel(SketchWidget * & view, QJsonObject &result)
{
	view = m_mainWindow->currentGraphicsView();
	if (!view) {
		result["found"] = false;
		result["reason"] = "no current view";
		return nullptr;
	}
	// The untranslated name, "breadboard", "schematic" or "pcb"
	result["view"] = ViewLayer::viewIDNaturalName(view->viewID());

	// A part is split into one ItemBase per layer in PCB view, all of which
	// report as selected together; collapse them onto the layer kin chief so
	// a single selected part counts once.
	QSet<ItemBase *> selected;
	for (QGraphicsItem *item : view->scene()->selectedItems()) {
		auto *base = dynamic_cast<ItemBase *>(item);
		if (base) selected.insert(base->layerKinChief());
	}
	if (selected.isEmpty()) {
		result["found"] = false;
		result["reason"] = "no part selected";
		return nullptr;
	}
	if (selected.count() > 1) {
		result["found"] = false;
		result["reason"] = QString("%1 parts selected, expected 1").arg(selected.count());
		return nullptr;
	}

	ItemBase *part = *selected.constBegin();
	PartLabel *partLabel = part->partLabel();
	if (!partLabel || !partLabel->initialized()) {
		result["found"] = false;
		result["reason"] = QString("part '%1' has no initialized label").arg(part->instanceTitle());
		return nullptr;
	}
	return partLabel;
}
