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

#include "FProbeWire.h"
#include "mainwindow/mainwindow.h"
#include "sketch/sketchwidget.h"
#include "items/wire.h"
#include "items/itembase.h"
#include "items/moduleidnames.h"
#include "connectors/connectoritem.h"
#include "viewlayer.h"
#include "commands.h"
#include "waitpushundostack.h"
#include "debugdialog.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QGraphicsScene>
#include <QSet>

FProbeWire::FProbeWire(MainWindow *mainWindow)
	: QObject(mainWindow)
	, FProbe("WireProbe")
	, m_mainWindow(mainWindow)
{
	connect(this, &FProbeWire::requestOperation, mainWindow, [this](const QString &jsonCommand) {
		QJsonDocument doc = QJsonDocument::fromJson(jsonCommand.toUtf8());
		QJsonObject params = doc.object();
		QString cmd = params["cmd"].toString();

		SketchWidget *view = currentView();
		if (!view) {
			m_lastResult = QVariant(QString("{\"ok\":false,\"error\":\"no active view\"}"));
			return;
		}

		QJsonObject result;

		if (cmd == "getWires") {
			result = handleGetWires(view, params);
		} else if (cmd == "getConnections") {
			result = handleGetConnections(view, params);
		} else if (cmd == "getWireInfo") {
			result = handleGetWireInfo(view, params);
		} else if (cmd == "getWireStartPos") {
			result = handleGetWirePos(view, params, true);
		} else if (cmd == "getWireEndPos") {
			result = handleGetWirePos(view, params, false);
		} else if (cmd == "getConnectorScenePos") {
			result = handleGetConnectorScenePos(view, params);
		} else if (cmd == "sceneToScreen") {
			result = handleSceneToScreen(view, params);
		} else if (cmd == "isBigDotStart") {
			result = handleIsBigDot(view, params, true);
		} else if (cmd == "isBigDotEnd") {
			result = handleIsBigDot(view, params, false);
		} else if (cmd == "moveWireEnd") {
			result = handleMoveWireEndpoint(view, params, false);
		} else if (cmd == "moveWireStart") {
			result = handleMoveWireEndpoint(view, params, true);
		} else if (cmd == "moveWireEndRelative") {
			result = handleMoveWireEndpointRelative(view, params, false);
		} else if (cmd == "moveWireStartRelative") {
			result = handleMoveWireEndpointRelative(view, params, true);
		} else if (cmd == "moveWireNearEnd") {
			result = handleMoveWireNear(view, params, false);
		} else if (cmd == "moveWireNearStart") {
			result = handleMoveWireNear(view, params, true);
		} else if (cmd == "deleteWire") {
			result = handleDeleteWire(view, params);
		} else if (cmd == "deleteUpToBendpoint") {
			result = handleDeleteUpToBendpoint(view, params);
		} else {
			result["ok"] = false;
			result["error"] = QString("unknown command '%1'").arg(cmd);
		}

		QJsonDocument resultDoc(result);
		m_lastResult = QVariant(QString(resultDoc.toJson(QJsonDocument::Compact)));
	}, Qt::BlockingQueuedConnection);
}

QVariant FProbeWire::read()
{
	return m_lastResult;
}

void FProbeWire::write(QVariant var)
{
	Q_EMIT requestOperation(var.toString());
}

SketchWidget * FProbeWire::currentView()
{
	return m_mainWindow->currentGraphicsView();
}

ItemBase * FProbeWire::findPartByTitle(SketchWidget *view, const QString &title)
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

ConnectorItem * FProbeWire::findConnector(ItemBase *part, const QString &connectorId)
{
	if (!part) return nullptr;
	return part->findConnectorItemWithSharedID(connectorId);
}

QJsonObject FProbeWire::handleGetWires(SketchWidget *view, const QJsonObject &params)
{
	QJsonObject result;
	QString partTitle = params["part"].toString();
	QString connId = params["connector"].toString();
	bool select = params["select"].toBool();

	ItemBase *part = findPartByTitle(view, partTitle);
	if (!part) {
		result["ok"] = false;
		result["error"] = QString("part '%1' not found").arg(partTitle);
		return result;
	}

	ConnectorItem *ci = findConnector(part, connId);
	if (!ci) {
		result["ok"] = false;
		result["error"] = QString("connector '%1' not found on '%2'").arg(connId, partTitle);
		return result;
	}

	// In PCB a connector is split across copper layer-kin (copper0 / copper1); a trace attaches to
	// one layer's item, so connectedToItems() on a single item misses traces on the other layer.
	// Aggregate this connector item and its cross-layer twin (like getConnections does), deduped by
	// wire id -- otherwise a part wired only on the far copper layer looks like it has no wires.
	QJsonArray wireIds;
	QList<Wire *> matchedWires;
	QSet<long> seenWireIds;
	QList<ConnectorItem *> sources;
	sources << ci;
	ConnectorItem *cross = ci->getCrossLayerConnectorItem();
	if (cross != nullptr && cross != ci) sources << cross;
	for (ConnectorItem *src : sources) {
		for (const auto &connected : src->connectedToItems()) {
			if (!connected) continue;
			Wire *w = qobject_cast<Wire *>(connected->attachedTo());
			if (w && !w->getRatsnest() && !seenWireIds.contains(w->id())) {
				seenWireIds.insert(w->id());
				wireIds.append(static_cast<qint64>(w->id()));
				matchedWires.append(w);
			}
		}
	}

	if (select) {
		view->scene()->clearSelection();
		for (Wire *w : matchedWires) {
			w->setSelected(true);
		}
	}

	result["ok"] = true;
	result["wireIds"] = wireIds;
	return result;
}

QJsonObject FProbeWire::handleGetConnections(SketchWidget *currentViewArg, const QJsonObject &params)
{
	// Raw ground-truth dump of a part's per-view connections: for each connector, every item it is
	// connected to (breadboard pins, other part pins, wires), with ratsnest/wire flags. Unlike
	// getWires this includes non-wire connections and ratsnest, and unlike the DebugConnectors
	// "differing QSets" report it is the direct connectedToItems() set, not a derived comparison.
	QJsonObject result;
	QString partTitle = params["part"].toString();
	QString connId = params["connector"].toString();   // optional; empty = all connectors
	QString viewName = params["view"].toString();       // optional; empty = current view

	// Resolve the requested view directly so a query neither depends on nor disturbs the active
	// view (the migration dialog moves the active view around as it navigates).
	SketchWidget *view = currentViewArg;
	if (viewName == "breadboard") view = m_mainWindow->sketchWidgetForView(ViewLayer::BreadboardView);
	else if (viewName == "schematic") view = m_mainWindow->sketchWidgetForView(ViewLayer::SchematicView);
	else if (viewName == "pcb") view = m_mainWindow->sketchWidgetForView(ViewLayer::PCBView);
	if (!view) {
		result["ok"] = false;
		result["error"] = QString("view '%1' not found").arg(viewName);
		return result;
	}

	result["ok"] = true;
	result["view"] = ViewLayer::viewIDNaturalName(view->viewID());

	ItemBase *part = findPartByTitle(view, partTitle);
	if (!part) {
		// Absence in a view is itself ground-truth data, not an error.
		result["found"] = false;
		result["connectors"] = QJsonArray();
		return result;
	}
	part = part->layerKinChief();
	result["found"] = true;

	QJsonArray connectors;
	for (ConnectorItem *ci : part->cachedConnectorItems()) {
		if (ci == nullptr) continue;
		if (!connId.isEmpty() && ci->connectorSharedID() != connId) continue;

		// In PCB a connector is split across copper layer-kin (Copper0 / Copper1); a trace attaches
		// to one layer's item, so cachedConnectorItems() alone misses connections on the other
		// layer. Aggregate this connector item and its cross-layer twin, deduped by target.
		QList<ConnectorItem *> sources;
		sources << ci;
		ConnectorItem *cross = ci->getCrossLayerConnectorItem();
		if (cross != nullptr && cross != ci) sources << cross;

		QSet<ConnectorItem *> seen;
		QJsonArray connectedTo;
		for (ConnectorItem *src : sources) {
			for (ConnectorItem *to : src->connectedToItems()) {
				if (to == nullptr || seen.contains(to)) continue;
				seen.insert(to);
				ItemBase *toItem = to->attachedTo();
				if (toItem == nullptr) continue;
				Wire *w = qobject_cast<Wire *>(toItem);
				QJsonObject e;
				e["title"] = to->attachedToInstanceTitle();
				e["connector"] = to->connectorSharedID();
				e["isWire"] = (w != nullptr);
				e["ratsnest"] = (w != nullptr && w->getRatsnest());
				connectedTo.append(e);
			}
		}

		QJsonObject centry;
		centry["connector"] = ci->connectorSharedID();
		centry["count"] = connectedTo.size();
		centry["connectedTo"] = connectedTo;
		connectors.append(centry);
	}
	result["connectors"] = connectors;
	return result;
}

namespace {
QString viewLayerPlacementName(ViewLayer::ViewLayerPlacement p) {
	switch (p) {
		case ViewLayer::NewTop:          return "top";
		case ViewLayer::NewBottom:       return "bottom";
		case ViewLayer::NewTopAndBottom: return "topAndBottom";
		case ViewLayer::UnknownPlacement: return "unknown";
	}
	return "unknown";
}
}

QJsonObject FProbeWire::handleGetWireInfo(SketchWidget *view, const QJsonObject &params)
{
	QJsonObject result;
	long wireId = static_cast<long>(params["wireId"].toInteger());

	ItemBase *item = view->findItem(wireId);
	Wire *wire = qobject_cast<Wire *>(item);
	if (!wire) {
		result["ok"] = false;
		result["error"] = QString("wire %1 not found").arg(wireId);
		return result;
	}

	QJsonArray flags;
	struct FlagEntry { ViewGeometry::WireFlag flag; const char *name; };
	static const FlagEntry kFlagEntries[] = {
		{ViewGeometry::RoutedFlag,         "routed"},
		{ViewGeometry::PCBTraceFlag,       "pcbTrace"},
		{ViewGeometry::ObsoleteJumperFlag, "obsoleteJumper"},
		{ViewGeometry::RatsnestFlag,       "ratsnest"},
		{ViewGeometry::AutoroutableFlag,   "autoroutable"},
		{ViewGeometry::NormalFlag,         "normal"},
		{ViewGeometry::SchematicTraceFlag, "schematicTrace"},
	};
	for (const FlagEntry &entry : kFlagEntries) {
		if (wire->hasFlag(entry.flag)) {
			flags.append(QString::fromLatin1(entry.name));
		}
	}

	result["ok"] = true;
	result["id"] = static_cast<qint64>(wire->id());
	result["view"] = ViewLayer::viewIDNaturalName(view->viewID());
	result["viewLayer"] = ViewLayer::viewLayerNameFromID(wire->viewLayerID());
	result["viewLayerPlacement"] = viewLayerPlacementName(wire->viewLayerPlacement());
	result["moduleID"] = wire->moduleID();
	result["flags"] = flags;
	result["curved"] = wire->isCurved();
	return result;
}

QJsonObject FProbeWire::handleGetConnectorScenePos(SketchWidget *view, const QJsonObject &params)
{
	QJsonObject result;
	QString partTitle = params["part"].toString();
	QString connId = params["connector"].toString();

	ItemBase *part = findPartByTitle(view, partTitle);
	if (!part) {
		result["ok"] = false;
		result["error"] = QString("part '%1' not found").arg(partTitle);
		return result;
	}

	ConnectorItem *ci = findConnector(part, connId);
	if (!ci) {
		result["ok"] = false;
		result["error"] = QString("connector '%1' not found on '%2'").arg(connId, partTitle);
		return result;
	}

	QPointF pos = ci->sceneAdjustedTerminalPoint(nullptr);
	result["ok"] = true;
	result["x"] = pos.x();
	result["y"] = pos.y();
	return result;
}

QJsonObject FProbeWire::handleSceneToScreen(SketchWidget *view, const QJsonObject &params)
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

QJsonObject FProbeWire::handleGetWirePos(SketchWidget *view, const QJsonObject &params, bool start)
{
	QJsonObject result;
	long wireId = static_cast<long>(params["wireId"].toInteger());

	ItemBase *item = view->findItem(wireId);
	Wire *wire = qobject_cast<Wire *>(item);
	if (!wire) {
		result["ok"] = false;
		result["error"] = QString("wire %1 not found").arg(wireId);
		return result;
	}

	ConnectorItem *ci = start ? wire->connector0() : wire->connector1();
	if (!ci) {
		result["ok"] = false;
		result["error"] = QString("connector not found on wire %1").arg(wireId);
		return result;
	}

	QPointF pos = ci->sceneAdjustedTerminalPoint(nullptr);
	result["ok"] = true;
	result["x"] = pos.x();
	result["y"] = pos.y();
	return result;
}

QJsonObject FProbeWire::handleIsBigDot(SketchWidget *view, const QJsonObject &params, bool start)
{
	QJsonObject result;
	long wireId = static_cast<long>(params["wireId"].toInteger());

	ItemBase *item = view->findItem(wireId);
	Wire *wire = qobject_cast<Wire *>(item);
	if (!wire) {
		result["ok"] = false;
		result["error"] = QString("wire %1 not found").arg(wireId);
		return result;
	}

	ConnectorItem *ci = start ? wire->connector0() : wire->connector1();
	result["ok"] = true;
	result["isBigDot"] = ci ? ci->isBigDot() : false;
	return result;
}

QJsonObject FProbeWire::handleMoveWireEndpoint(SketchWidget *view, const QJsonObject &params, bool start)
{
	QJsonObject result;
	long wireId = static_cast<long>(params["wireId"].toInteger());

	ItemBase *item = view->findItem(wireId);
	Wire *wire = qobject_cast<Wire *>(item);
	if (!wire) {
		result["ok"] = false;
		result["error"] = QString("wire %1 not found").arg(wireId);
		return result;
	}

	// Determine target position
	QPointF targetPos;
	ConnectorItem *targetConnector = nullptr;
	if (params.contains("targetPart")) {
		ItemBase *targetPart = findPartByTitle(view, params["targetPart"].toString());
		if (!targetPart) {
			result["ok"] = false;
			result["error"] = QString("target part '%1' not found").arg(params["targetPart"].toString());
			return result;
		}
		targetConnector = findConnector(targetPart, params["targetConnector"].toString());
		if (!targetConnector) {
			result["ok"] = false;
			result["error"] = QString("target connector '%1' not found").arg(params["targetConnector"].toString());
			return result;
		}
		targetPos = targetConnector->sceneAdjustedTerminalPoint(nullptr);
	} else {
		targetPos = QPointF(params["x"].toDouble(), params["y"].toDouble());
	}

	// Build undo command
	QLineF oldLine = wire->line();
	QPointF oldPos = wire->pos();

	ConnectorItem *movingConnector = start ? wire->connector0() : wire->connector1();
	QString movingConnectorId = movingConnector->connectorSharedID();

	// Disconnect existing connections on the moving endpoint
	auto *parentCommand = new QUndoCommand(QObject::tr("Move wire endpoint"));
	new CleanUpWiresCommand(view, CleanUpWiresCommand::UndoOnly, parentCommand);
	new CleanUpRatsnestsCommand(view, CleanUpWiresCommand::UndoOnly, parentCommand);

	for (const auto &connected : movingConnector->connectedToItems()) {
		if (!connected) continue;
		new ChangeConnectionCommand(view, BaseCommand::CrossView,
			connected->attachedToID(), connected->connectorSharedID(),
			wireId, movingConnectorId,
			ViewLayer::specFromID(wire->viewLayerID()),
			false, parentCommand);
	}

	// Calculate new line
	QLineF newLine;
	QPointF newPos = oldPos;
	if (start) {
		// Moving connector0: the wire origin changes
		QPointF oldEnd = oldPos + oldLine.p2();
		newPos = targetPos;
		newLine = QLineF(QPointF(0, 0), oldEnd - targetPos);
	} else {
		// Moving connector1: keep origin, change endpoint
		newLine = QLineF(oldLine.p1(), targetPos - oldPos);
	}

	new ChangeWireCommand(view, wireId, oldLine, newLine, oldPos, newPos, true, true, parentCommand);

	// Connect to new target if specified
	if (targetConnector) {
		new ChangeConnectionCommand(view, BaseCommand::CrossView,
			targetConnector->attachedToID(), targetConnector->connectorSharedID(),
			wireId, movingConnectorId,
			ViewLayer::specFromID(wire->viewLayerID()),
			true, parentCommand);
	}

	new CleanUpRatsnestsCommand(view, CleanUpWiresCommand::RedoOnly, parentCommand);
	new CleanUpWiresCommand(view, CleanUpWiresCommand::RedoOnly, parentCommand);

	view->undoStack()->push(parentCommand);

	result["ok"] = true;
	return result;
}

QJsonObject FProbeWire::handleMoveWireEndpointRelative(SketchWidget *view, const QJsonObject &params, bool start)
{
	QJsonObject result;
	long wireId = static_cast<long>(params["wireId"].toInteger());

	ItemBase *item = view->findItem(wireId);
	Wire *wire = qobject_cast<Wire *>(item);
	if (!wire) {
		result["ok"] = false;
		result["error"] = QString("wire %1 not found").arg(wireId);
		return result;
	}

	double dx = params["dx"].toDouble();
	double dy = params["dy"].toDouble();

	ConnectorItem *movingConnector = start ? wire->connector0() : wire->connector1();
	QPointF currentTerminal = movingConnector->sceneAdjustedTerminalPoint(nullptr);
	QPointF targetPos = currentTerminal + QPointF(dx, dy);

	// Disconnect existing connections on the moving endpoint
	QLineF oldLine = wire->line();
	QPointF oldPos = wire->pos();
	QString movingConnectorId = movingConnector->connectorSharedID();

	auto *parentCommand = new QUndoCommand(QObject::tr("Move wire endpoint"));
	new CleanUpWiresCommand(view, CleanUpWiresCommand::UndoOnly, parentCommand);
	new CleanUpRatsnestsCommand(view, CleanUpWiresCommand::UndoOnly, parentCommand);

	for (const auto &connected : movingConnector->connectedToItems()) {
		if (!connected) continue;
		new ChangeConnectionCommand(view, BaseCommand::CrossView,
			connected->attachedToID(), connected->connectorSharedID(),
			wireId, movingConnectorId,
			ViewLayer::specFromID(wire->viewLayerID()),
			false, parentCommand);
	}

	QLineF newLine;
	QPointF newPos = oldPos;
	if (start) {
		QPointF oldEnd = oldPos + oldLine.p2();
		newPos = targetPos;
		newLine = QLineF(QPointF(0, 0), oldEnd - targetPos);
	} else {
		newLine = QLineF(oldLine.p1(), targetPos - oldPos);
	}

	new ChangeWireCommand(view, wireId, oldLine, newLine, oldPos, newPos, true, true, parentCommand);

	new CleanUpRatsnestsCommand(view, CleanUpWiresCommand::RedoOnly, parentCommand);
	new CleanUpWiresCommand(view, CleanUpWiresCommand::RedoOnly, parentCommand);

	view->undoStack()->push(parentCommand);

	result["ok"] = true;
	return result;
}

QJsonObject FProbeWire::handleMoveWireNear(SketchWidget *view, const QJsonObject &params, bool start)
{
	QJsonObject result;
	long wireId = static_cast<long>(params["wireId"].toInteger());

	ItemBase *item = view->findItem(wireId);
	Wire *wire = qobject_cast<Wire *>(item);
	if (!wire) {
		result["ok"] = false;
		result["error"] = QString("wire %1 not found").arg(wireId);
		return result;
	}

	// Determine target position for the new wire segment
	QPointF targetPos;
	ConnectorItem *targetConnector = nullptr;
	if (params.contains("targetPart")) {
		ItemBase *targetPart = findPartByTitle(view, params["targetPart"].toString());
		if (!targetPart) {
			result["ok"] = false;
			result["error"] = QString("target part '%1' not found").arg(params["targetPart"].toString());
			return result;
		}
		targetConnector = findConnector(targetPart, params["targetConnector"].toString());
		if (!targetConnector) {
			result["ok"] = false;
			result["error"] = QString("target connector '%1' not found").arg(params["targetConnector"].toString());
			return result;
		}
		targetPos = targetConnector->sceneAdjustedTerminalPoint(nullptr);
	} else {
		targetPos = QPointF(params["x"].toDouble(), params["y"].toDouble());
	}

	// Split wire near the specified end (90% along for "near end", 10% for "near start")
	QLineF oldLine = wire->line();
	QPointF oldPos = wire->pos();
	double ratio = start ? 0.1 : 0.9;
	QPointF splitPoint = oldLine.pointAt(ratio);
	QPointF splitScenePos = oldPos + splitPoint;

	// Create the split: reuse the logic from SketchWidget::wireSplitSlot
	QLineF newLine(oldLine.p1(), splitPoint);

	long newID = ItemBase::getNextID();
	ViewGeometry vg(wire->getViewGeometry());
	vg.setLoc(splitScenePos);
	QLineF newLine2(QPointF(0, 0), oldLine.p2() - splitPoint);
	vg.setLine(newLine2);

	BaseCommand::CrossViewType crossView = BaseCommand::CrossView;

	auto *parentCommand = new QUndoCommand(QObject::tr("Split and move wire"));
	new CleanUpWiresCommand(view, CleanUpWiresCommand::UndoOnly, parentCommand);
	new CleanUpRatsnestsCommand(view, CleanUpWiresCommand::UndoOnly, parentCommand);

	// Add the new wire segment
	new AddItemCommand(view, crossView, ModuleIDNames::WireModuleIDName,
		wire->viewLayerPlacement(), vg, newID, true, -1, parentCommand);
	new CheckStickyCommand(view, crossView, newID, false,
		CheckStickyCommand::RemoveOnly, parentCommand);
	new WireColorChangeCommand(view, newID, wire->colorString(), wire->colorString(),
		wire->opacity(), wire->opacity(), parentCommand);
	new WireWidthChangeCommand(view, newID, wire->wireWidth(), wire->wireWidth(), parentCommand);

	// Disconnect connector1 of old wire and reconnect to new wire
	ConnectorItem *connector1 = wire->connector1();
	for (const auto &toConnectorItem : connector1->connectedToItems()) {
		new ChangeConnectionCommand(view, crossView,
			toConnectorItem->attachedToID(), toConnectorItem->connectorSharedID(),
			wire->id(), connector1->connectorSharedID(),
			ViewLayer::specFromID(wire->viewLayerID()),
			false, parentCommand);
		new ChangeConnectionCommand(view, crossView,
			toConnectorItem->attachedToID(), toConnectorItem->connectorSharedID(),
			newID, connector1->connectorSharedID(),
			ViewLayer::specFromID(wire->viewLayerID()),
			true, parentCommand);
	}

	// Shorten old wire
	new ChangeWireCommand(view, wire->id(), oldLine, newLine, oldPos, oldPos,
		true, false, parentCommand);

	// Connect old wire connector1 to new wire connector0
	new ChangeConnectionCommand(view, crossView,
		wire->id(), connector1->connectorSharedID(),
		newID, "connector0",
		ViewLayer::specFromID(wire->viewLayerID()),
		true, parentCommand);

	// Now move the appropriate end of the new wire to the target
	// If "near end": the new wire's connector1 gets moved to target
	// If "near start": the old wire's connector0 gets moved to target
	// For simplicity, we handle this as a second step after split
	long wireToMove = start ? wire->id() : newID;
	QString connectorToMove = start ? "connector0" : "connector1";

	// Calculate the line change for the wire being moved
	// After the split, we need to move the free end to targetPos
	if (!start) {
		// Move new wire's connector1 to target
		QLineF movedOldLine = newLine2;
		QPointF movedOldPos = splitScenePos;
		QLineF movedNewLine(movedOldLine.p1(), targetPos - movedOldPos);

		new ChangeWireCommand(view, newID, movedOldLine, movedNewLine,
			movedOldPos, movedOldPos, true, true, parentCommand);
	} else {
		// Move old wire's connector0 to target
		QPointF oldEnd = oldPos + newLine.p2();
		QLineF movedNewLine(QPointF(0, 0), oldEnd - targetPos);

		new ChangeWireCommand(view, wire->id(), newLine, movedNewLine,
			oldPos, targetPos, true, true, parentCommand);
	}

	// Connect to target connector if specified
	if (targetConnector) {
		new ChangeConnectionCommand(view, crossView,
			targetConnector->attachedToID(), targetConnector->connectorSharedID(),
			wireToMove, connectorToMove,
			ViewLayer::specFromID(wire->viewLayerID()),
			true, parentCommand);
	}

	new CleanUpRatsnestsCommand(view, CleanUpWiresCommand::RedoOnly, parentCommand);
	new CleanUpWiresCommand(view, CleanUpWiresCommand::RedoOnly, parentCommand);

	view->undoStack()->push(parentCommand);

	result["ok"] = true;
	result["newWireId"] = static_cast<qint64>(newID);
	return result;
}

QJsonObject FProbeWire::handleDeleteWire(SketchWidget *view, const QJsonObject &params)
{
	QJsonObject result;
	long wireId = static_cast<long>(params["wireId"].toInteger());

	ItemBase *item = view->findItem(wireId);
	Wire *wire = qobject_cast<Wire *>(item);
	if (!wire) {
		result["ok"] = false;
		result["error"] = QString("wire %1 not found").arg(wireId);
		return result;
	}

	// Collect the wire and all directly connected wires (chain)
	QSet<ItemBase *> deletedItems;
	QList<Wire *> wires;
	wire->collectDirectWires(wires);
	for (Wire *w : wires) {
		deletedItems.insert(w);
	}

	auto *parentCommand = new QUndoCommand(QObject::tr("Delete wire"));
	view->stackSelectionState(false, parentCommand);
	new CleanUpWiresCommand(view, CleanUpWiresCommand::UndoOnly, parentCommand);
	new CleanUpRatsnestsCommand(view, CleanUpWiresCommand::UndoOnly, parentCommand);
	view->deleteMiddle(deletedItems, parentCommand);
	for (ItemBase *itemBase : deletedItems) {
		view->makeDeleteItemCommand(itemBase, BaseCommand::CrossView, parentCommand);
	}
	new CleanUpRatsnestsCommand(view, CleanUpWiresCommand::RedoOnly, parentCommand);
	new CleanUpWiresCommand(view, CleanUpWiresCommand::RedoOnly, parentCommand);
	view->undoStack()->push(parentCommand);

	result["ok"] = true;
	return result;
}

QJsonObject FProbeWire::handleDeleteUpToBendpoint(SketchWidget *view, const QJsonObject &params)
{
	QJsonObject result;
	long wireId = static_cast<long>(params["wireId"].toInteger());

	ItemBase *item = view->findItem(wireId);
	Wire *wire = qobject_cast<Wire *>(item);
	if (!wire) {
		result["ok"] = false;
		result["error"] = QString("wire %1 not found").arg(wireId);
		return result;
	}

	// Delete only this single wire segment
	QSet<ItemBase *> deletedItems;
	deletedItems.insert(wire);

	auto *parentCommand = new QUndoCommand(QObject::tr("Delete wire segment"));
	view->stackSelectionState(false, parentCommand);
	new CleanUpWiresCommand(view, CleanUpWiresCommand::UndoOnly, parentCommand);
	new CleanUpRatsnestsCommand(view, CleanUpWiresCommand::UndoOnly, parentCommand);
	view->deleteMiddle(deletedItems, parentCommand);
	view->makeDeleteItemCommand(wire, BaseCommand::CrossView, parentCommand);
	new CleanUpRatsnestsCommand(view, CleanUpWiresCommand::RedoOnly, parentCommand);
	new CleanUpWiresCommand(view, CleanUpWiresCommand::RedoOnly, parentCommand);
	view->undoStack()->push(parentCommand);

	result["ok"] = true;
	return result;
}

