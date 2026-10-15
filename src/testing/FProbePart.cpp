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

#include "FProbePart.h"
#include "mainwindow/mainwindow.h"
#include "sketch/sketchwidget.h"
#include "items/itembase.h"
#include "model/modelpart.h"
#include "commands.h"
#include "waitpushundostack.h"
#include "debugdialog.h"

#include <QJsonDocument>
#include <QGraphicsScene>
#include <QRectF>
#include <QPointF>

FProbePart::FProbePart(MainWindow *mainWindow)
	: QObject(mainWindow)
	, FProbe("PartProbe")
	, m_mainWindow(mainWindow)
{
	connect(this, &FProbePart::requestOperation, mainWindow, [this](const QString &jsonCommand) {
		QJsonDocument doc = QJsonDocument::fromJson(jsonCommand.toUtf8());
		QJsonObject params = doc.object();
		QString cmd = params["cmd"].toString();

		SketchWidget *view = currentView();
		if (!view) {
			m_lastResult = QVariant(QString("{\"ok\":false,\"error\":\"no active view\"}"));
			return;
		}

		QJsonObject result;

		if (cmd == "getPosition") {
			result = handleGetPosition(view, params);
		} else if (cmd == "movePart") {
			result = handleMovePart(view, params);
		} else if (cmd == "movePartRelative") {
			result = handleMovePartRelative(view, params);
		} else if (cmd == "getSize") {
			result = handleGetSize(view, params);
		} else if (cmd == "getResizeHandlePos") {
			result = handleGetResizeHandlePos(view, params);
		} else if (cmd == "sceneToScreen") {
			result = handleSceneToScreen(view, params);
		} else if (cmd == "getGridSize") {
			result = handleGetGridSize(view, params);
		} else {
			result["ok"] = false;
			result["error"] = QString("unknown command '%1'").arg(cmd);
		}

		QJsonDocument resultDoc(result);
		m_lastResult = QVariant(QString(resultDoc.toJson(QJsonDocument::Compact)));
	}, Qt::BlockingQueuedConnection);
}

QVariant FProbePart::read()
{
	return m_lastResult;
}

void FProbePart::write(QVariant var)
{
	Q_EMIT requestOperation(var.toString());
}

SketchWidget * FProbePart::currentView()
{
	return m_mainWindow->currentGraphicsView();
}

ItemBase * FProbePart::findPartByTitle(SketchWidget *view, const QString &title)
{
	for (QGraphicsItem *item : view->scene()->items()) {
		auto *base = dynamic_cast<ItemBase *>(item);
		if (!base) continue;
		if (base->instanceTitle() != title) continue;
		if (base->viewID() != view->viewID()) continue;
		return base;
	}
	return nullptr;
}

// Resolve a part from either a "part" instance title or, when no title is known
// (e.g. straight after a drop), the first item matching a "moduleID".
ItemBase * FProbePart::resolvePart(SketchWidget *view, const QJsonObject &params)
{
	QString title = params["part"].toString();
	if (!title.isEmpty()) {
		return findPartByTitle(view, title);
	}

	QString moduleID = params["moduleID"].toString();
	if (!moduleID.isEmpty()) {
		for (QGraphicsItem *item : view->scene()->items()) {
			auto *base = dynamic_cast<ItemBase *>(item);
			if (!base) continue;
			if (base->viewID() != view->viewID()) continue;
			if (base->moduleID() != moduleID) continue;
			return base;
		}
	}
	return nullptr;
}

QJsonObject FProbePart::handleGetPosition(SketchWidget *view, const QJsonObject &params)
{
	QJsonObject result;
	QString partTitle = params["part"].toString();
	bool select = params["select"].toBool();

	ItemBase *part = findPartByTitle(view, partTitle);
	if (!part) {
		result["ok"] = false;
		result["error"] = QString("part '%1' not found").arg(partTitle);
		return result;
	}

	if (select) {
		view->scene()->clearSelection();
		part->setSelected(true);
	}

	QPointF pos = part->pos();
	result["ok"] = true;
	result["x"] = pos.x();
	result["y"] = pos.y();
	return result;
}

QJsonObject FProbePart::handleMovePart(SketchWidget *view, const QJsonObject &params)
{
	QJsonObject result;
	QString partTitle = params["part"].toString();

	ItemBase *part = findPartByTitle(view, partTitle);
	if (!part) {
		result["ok"] = false;
		result["error"] = QString("part '%1' not found").arg(partTitle);
		return result;
	}

	QPointF oldPos = part->pos();
	QPointF newPos(params["x"].toDouble(), params["y"].toDouble());

	auto *parentCommand = new QUndoCommand(QObject::tr("Move part"));
	new SimpleMoveItemCommand(view, part->id(), oldPos, newPos, parentCommand);
	view->undoStack()->push(parentCommand);

	result["ok"] = true;
	return result;
}

QJsonObject FProbePart::handleMovePartRelative(SketchWidget *view, const QJsonObject &params)
{
	QJsonObject result;
	QString partTitle = params["part"].toString();

	ItemBase *part = findPartByTitle(view, partTitle);
	if (!part) {
		result["ok"] = false;
		result["error"] = QString("part '%1' not found").arg(partTitle);
		return result;
	}

	QPointF oldPos = part->pos();
	double dx = params["dx"].toDouble();
	double dy = params["dy"].toDouble();
	QPointF newPos = oldPos + QPointF(dx, dy);

	auto *parentCommand = new QUndoCommand(QObject::tr("Move part"));
	new SimpleMoveItemCommand(view, part->id(), oldPos, newPos, parentCommand);
	view->undoStack()->push(parentCommand);

	result["ok"] = true;
	return result;
}

QJsonObject FProbePart::handleGetSize(SketchWidget *view, const QJsonObject &params)
{
	QJsonObject result;
	ItemBase *part = resolvePart(view, params);
	if (!part) {
		result["ok"] = false;
		result["error"] = QString("part not found");
		return result;
	}

	if (params["select"].toBool()) {
		view->scene()->clearSelection();
		part->setSelected(true);
	}

	QRectF sbr = part->sceneBoundingRect();
	result["ok"] = true;
	// Logical size as stored on the model part, in mm. 0 for non-resizable parts.
	result["width"] = part->modelPart()->localProp("width").toDouble();
	result["height"] = part->modelPart()->localProp("height").toDouble();
	// Scene bounding box (scene units == SVG px at 90 dpi); includes any canvas margin.
	result["sceneWidth"] = sbr.width();
	result["sceneHeight"] = sbr.height();
	result["title"] = part->instanceTitle();
	return result;
}

QJsonObject FProbePart::handleGetResizeHandlePos(SketchWidget *view, const QJsonObject &params)
{
	QJsonObject result;
	ItemBase *part = resolvePart(view, params);
	if (!part) {
		result["ok"] = false;
		result["error"] = QString("part not found");
		return result;
	}

	// The resize handles sit at the corners of the item's bounding box (see
	// ResizableBoard::findCorner, which tests mapToScene(0,0) .. mapToScene(m_size)).
	// sceneBoundingRect() gives exactly those corners in scene coordinates.
	QRectF r = part->sceneBoundingRect();
	QString corner = params["corner"].toString();
	QPointF p;
	if (corner == "topLeft")          p = r.topLeft();
	else if (corner == "topRight")    p = r.topRight();
	else if (corner == "bottomLeft")  p = r.bottomLeft();
	else if (corner == "bottomRight") p = r.bottomRight();
	else if (corner == "center")      p = r.center();
	else {
		result["ok"] = false;
		result["error"] = QString("unknown corner '%1'").arg(corner);
		return result;
	}

	result["ok"] = true;
	result["x"] = p.x();
	result["y"] = p.y();
	return result;
}

QJsonObject FProbePart::handleSceneToScreen(SketchWidget *view, const QJsonObject &params)
{
	QJsonObject result;
	double x = params["x"].toDouble();
	double y = params["y"].toDouble();

	QPoint viewPoint = view->mapFromScene(QPointF(x, y));
	QPoint screenPoint = view->viewport()->mapToGlobal(viewPoint);

	result["ok"] = true;
	result["x"] = screenPoint.x();
	result["y"] = screenPoint.y();
	return result;
}

QJsonObject FProbePart::handleGetGridSize(SketchWidget *view, const QJsonObject &params)
{
	Q_UNUSED(params);
	QJsonObject result;
	// gridSizeInches()/shouldAlignToGrid() are public on the InfoGraphicsView
	// interface (protected on SketchWidget itself), so query them through it.
	InfoGraphicsView * igv = view;
	result["ok"] = true;
	result["gridSizeInches"] = igv->gridSizeInches();
	result["alignToGrid"] = igv->shouldAlignToGrid();
	return result;
}
