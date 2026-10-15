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

#include "FProbeBugAnnotations.h"
#include "mainwindow/mainwindow.h"
#include "sketch/sketchwidget.h"
#include "items/itembase.h"
#include "viewlayer.h"

#include <QGraphicsScene>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

FProbeBugAnnotations::FProbeBugAnnotations(MainWindow *mainWindow)
	: QObject(mainWindow)
	, FProbe("BugAnnotations")
	, m_mainWindow(mainWindow)
{
	// Probes are called from the FTesting server thread; hop to the GUI thread for scene access.
	connect(this, &FProbeBugAnnotations::requestInfo, mainWindow, [this]() {
		QJsonObject result;
		QJsonArray items;

		const ViewLayer::ViewID viewIds[] = { ViewLayer::BreadboardView, ViewLayer::SchematicView, ViewLayer::PCBView };
		const char *viewNames[] = { "breadboard", "schematic", "pcb" };
		for (int v = 0; v < 3; ++v) {
			SketchWidget *view = m_mainWindow->sketchWidgetForView(viewIds[v]);
			if (view == nullptr) continue;
			Q_FOREACH (QGraphicsItem *gi, view->scene()->items()) {
				ItemBase *ib = dynamic_cast<ItemBase *>(gi);
				if (ib == nullptr || !ib->hasBug()) continue;
				QJsonObject entry;
				entry["id"] = double(ib->id());
				entry["title"] = ib->instanceTitle();
				entry["module"] = ib->moduleID();
				entry["view"] = QString::fromLatin1(viewNames[v]);
				entry["text"] = ib->bugText();
				items.append(entry);
			}
		}
		result["count"] = items.size();
		result["items"] = items;
		m_lastResult = QVariant(QString(QJsonDocument(result).toJson(QJsonDocument::Compact)));
	}, Qt::BlockingQueuedConnection);

	// "click:<instanceTitle>" simulates a user clicking that part's obsolete badge, exercising the
	// same path as BugItem::mousePressEvent -> ItemBase::bugAnnotationClicked (open the migration
	// dialog). Hops to the GUI thread for scene access.
	connect(this, &FProbeBugAnnotations::requestAction, mainWindow, [this](const QString &action) {
		QJsonObject result;
		QString title = action.startsWith("click:") ? action.mid(6) : QString();
		if (title.isEmpty()) {
			result["ok"] = false;
			result["reason"] = QString("unknown action '%1'").arg(action);
			m_lastResult = QVariant(QString(QJsonDocument(result).toJson(QJsonDocument::Compact)));
			return;
		}

		ItemBase *target = nullptr;
		const ViewLayer::ViewID viewIds[] = { ViewLayer::BreadboardView, ViewLayer::SchematicView, ViewLayer::PCBView };
		for (int v = 0; v < 3 && target == nullptr; ++v) {
			SketchWidget *view = m_mainWindow->sketchWidgetForView(viewIds[v]);
			if (view == nullptr) continue;
			Q_FOREACH (QGraphicsItem *gi, view->scene()->items()) {
				ItemBase *ib = dynamic_cast<ItemBase *>(gi);
				if (ib != nullptr && ib->hasBug() && ib->instanceTitle() == title) {
					target = ib->layerKinChief();
					break;
				}
			}
		}

		if (target == nullptr) {
			result["ok"] = false;
			result["reason"] = QString("no part with a bug badge titled '%1'").arg(title);
		} else {
			result["ok"] = target->bugAnnotationClicked();
		}
		m_lastResult = QVariant(QString(QJsonDocument(result).toJson(QJsonDocument::Compact)));
	}, Qt::BlockingQueuedConnection);
}

QVariant FProbeBugAnnotations::read()
{
	Q_EMIT requestInfo();
	return m_lastResult;
}

void FProbeBugAnnotations::write(QVariant var)
{
	Q_EMIT requestAction(var.toString());
}
