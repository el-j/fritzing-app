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

#include "debugdialog.h"
#include "debugconnectors.h"
#include "connectors/connectoritem.h"
#include "items/itembase.h"
#include "items/symbolpaletteitem.h"
#include "items/virtualwire.h"
#include "items/wire.h"
#include "utils/textutils.h"


#include <QEventLoop>
#include <QLineF>

DebugConnectors::DebugConnectors(SketchWidget *breadboardGraphicsView, SketchWidget *schematicGraphicsView, SketchWidget *pcbGraphicsView, QObject *parent)
	: QObject(parent),
	  m_breadboardGraphicsView(breadboardGraphicsView),
	  m_schematicGraphicsView(schematicGraphicsView),
	  m_pcbGraphicsView(pcbGraphicsView),
	  timer(new QTimer(this))
{
	monitorConnections(false);
	timer->setSingleShot(true);
	connect(timer, &QTimer::timeout, this, &DebugConnectors::performCheck);
	connect(breadboardGraphicsView,
			&SketchWidget::routingCheckSignal,
			this,
			&DebugConnectors::onChangeConnection);
	connect(schematicGraphicsView,
			&SketchWidget::routingCheckSignal,
			this,
			&DebugConnectors::onChangeConnection);
	connect(pcbGraphicsView,
			&SketchWidget::routingCheckSignal,
			this,
			&DebugConnectors::onChangeConnection);

	monitorConnections(true);
}

void DebugConnectors::monitorConnections(bool enabled)
{
	m_monitorEnabled = enabled;
}

void logConnector(QString info, ConnectorItem *connectorItem)
{
	ViewLayer::ViewLayerID layer = connectorItem->attachedToViewLayerID();
	ViewLayer::ViewID view = connectorItem->attachedToViewID();

	qDebug() << info << " " << connectorItem->attachedToTitle()
			 << connectorItem->attachedToInstanceTitle() << ViewLayer::viewLayerNameFromID(layer)
			 << ViewLayer::viewIDName(view);

}

QSet<QString> DebugConnectors::getItemConnectorSet(ConnectorItem *connectorItem) {
	QSet<QString> set;
	// logConnector("from", connectorItem);
	Q_FOREACH (ConnectorItem * toConnectorItem, connectorItem->connectedToItems()) {
		ItemBase * attachedToItem = toConnectorItem->attachedTo();
		VirtualWire * virtualWire = qobject_cast<VirtualWire *>(attachedToItem);
		if (virtualWire != nullptr) continue;
		// logConnector("to", toConnectorItem);
		// Ignore Multimeter and Probes. They have no pcb connections.
		if (attachedToItem->title().contains("Multimeter")) continue;
		if (attachedToItem->title().contains("Oscilloscope")) continue;
		QString idString = toConnectorItem->attachedToInstanceTitle() + ":" + toConnectorItem->connectorSharedID();
		set.insert(idString);
	}
	return set;
}

QList<ItemBase *> DebugConnectors::toSortedItembases(const QList<QGraphicsItem *> &graphicsItems) {
	QList<ItemBase *> itembases;
	foreach (QGraphicsItem* item, graphicsItems) {
		ItemBase * part = dynamic_cast<ItemBase *>(item);
		if (!part) continue;
		itembases.append(part);
	}
	std::sort(itembases.begin(), itembases.end(), [](ItemBase * b1, ItemBase * b2) {
		return b1->id() < b2->id();
	});
	return itembases;
}

void DebugConnectors::collectPartsForCheck(QList<ItemBase *> &partList, QGraphicsScene *scene)
{
	Q_FOREACH (QGraphicsItem * item, scene->items()) {
		ItemBase * itemBase = dynamic_cast<ItemBase *>(item);
		if (!itemBase) continue;
		if (itemBase->layerKinChief() != itemBase) continue;

		partList.append(itemBase);
	}
}

QList<Wire *> DebugConnectors::collectWiresForCheck(ViewGeometry::WireFlag flag, QGraphicsScene *scene)
{
	QList<QGraphicsItem *> items = scene->items();
	QList<Wire *> wires;
	Q_FOREACH (QGraphicsItem *item, items) {
		Wire *wire = dynamic_cast<Wire *>(item);
		if (!wire)
			continue;

		if (wire->hasFlag(flag)) {
			if (wire->parentItem()) {
				// TODO: do we want to skip module wires  for this check?
				continue;
			}

			wires.append(wire);
		}
	}
	return wires;
}

void DebugConnectors::onChangeConnection()
{
	if (!m_monitorEnabled) {
		return;
	}
	// Trailing-edge debounce: every signal restarts the timer; the check
	// runs once after minimumInterval of silence. Multi-step parent commands
	// (e.g. dragWireChanged building a clique) emit routingCheckSignal once
	// per child ChangeConnectionCommand; coalescing into a single trailing
	// tick avoids seeing the model mid-burst.
	timer->start(minimumInterval);
}

void DebugConnectors::performCheck()
{
	if (!m_monitorEnabled) {
		return;
	}
	// This runs from a single-shot timer, so the views (or their scenes) may have been torn down
	// since it was armed -- e.g. the example service closes one sketch window and pumps the event
	// loop while loading the next. The QPointers null out on destruction; bail instead of crashing.
	if (m_breadboardGraphicsView.isNull() || m_schematicGraphicsView.isNull() || m_pcbGraphicsView.isNull()) {
		return;
	}
	if (m_breadboardGraphicsView->scene() == nullptr
	        || m_schematicGraphicsView->scene() == nullptr
	        || m_pcbGraphicsView->scene() == nullptr) {
		return;
	}
	QSet<ItemBase *> errors;
	errors = doRoutingCheck();
	errors += doWireCheck();
	reportErrors(errors);
}

void DebugConnectors::onSelectErrors()
{
	QSet<ItemBase *> errors = doRoutingCheck();
	if (!errors.isEmpty()) {
		m_schematicGraphicsView->selectItems(errors.values());
	}
}

void DebugConnectors::onRepairErrors()
{
	bool tmp = m_monitorEnabled;
	monitorConnections(false);

	auto stack = m_schematicGraphicsView->undoStack();

	stack->waitForTimers();
	int index = stack->index();
	auto views = {m_breadboardGraphicsView.data(), m_schematicGraphicsView.data(), m_pcbGraphicsView.data()};
	for(SketchWidget * view: views) {
		stack->waitForTimers();
		QSet<ItemBase *> errors = doRoutingCheck();
		errors += doWireCheck();
		if (!errors.empty()) {
			DebugDialog::debug(QString("Repair %1 errors.").arg(errors.size()));
			view->selectItems(errors.values());
			view->deleteSelected(nullptr, false);
		}
	}
	stack->waitForTimers();
	stack->setIndex(index);
	stack->waitForTimers();
	doRoutingCheck();

	monitorConnections(tmp);
	emit repairErrorsCompleted();
}


QString rectAsString(QRectF rect)
{
	QString rectAsString = QString("Rect(x: %1, y: %2, width: %3, height: %4)")
							   .arg(QString::number(rect.x(), 'f', 2))
							   .arg(QString::number(rect.y(), 'f', 2))
							   .arg(QString::number(rect.width(), 'f', 2))
							   .arg(QString::number(rect.height(), 'f', 2));

	return rectAsString;
}

QString pointAsString(QPointF p)
{
	return QString("Point(x: %1, y: %2)")
		.arg(QString::number(p.x(), 'f', 2), QString::number(p.y(), 'f', 2));
}

bool validConnectors(ConnectorItem *c1, ConnectorItem *c2)
{
	// More generous check: true if connectors somehow overlap
	//	QRectF rect1(c1->mapRectToScene(c1->boundingRect()));
	//	DebugDialog::debug(QString("c1 rect %1").arg(rectAsString(rect1)));

	//	QRectF rect2(c2->mapRectToScene(c2->boundingRect()));
	//	DebugDialog::debug(QString("c2 rect %1").arg(rectAsString(rect2)));

	//	bool overlap = rect1.intersects(rect2);

	// Strict check: terminal points must be within epsilon range
	QPointF t1(c1->mapToScene(c1->adjustedTerminalPoint()));
	QPointF t2(c2->mapToScene(c2->adjustedTerminalPoint()));

	bool near = QLineF(t1, t2).length() < 0.01;
	return near;
}

// Conndition: c1 is a connector that is not connected to anything.
// If we can find something withing the area of c1, we have a dead connection
QSet<ItemBase *> DebugConnectors::findConnectors(ConnectorItem *c1)
{
	assert(c1->connectedToItems().empty());

	QSet<ItemBase *> errors;
	// look for connectors that collide, but are not connected
	auto checklist = m_schematicGraphicsView->scene()->collidingItems(c1);
	for (auto item : checklist) {
		ConnectorItem *connector = dynamic_cast<ConnectorItem *>(item);
		if (connector) {
			if (validConnectors(c1, connector)) {
				errors << c1->attachedTo();
				errors << connector->attachedTo();
			}
		}
	}

	for (auto item : errors) {
		DebugDialog::debug(QString("dead connection %1").arg(item->instanceTitle()));
	}
	return errors;
}


QSet<ItemBase *> DebugConnectors::doWireCheck()
{
	QSet<ItemBase *> errors;

	DebugDialog::debug("debug wires do");
	QList<Wire *> wires = collectWiresForCheck(ViewGeometry::SchematicTraceFlag,
											   m_schematicGraphicsView->scene());

	for (auto *wire : wires) {
		assert(wire);
		if (!wire) {
			DebugDialog::debug("skip null wire");
			continue;
		}
		for (ConnectorItem *c1 : {wire->connector0(), wire->connector1()}) {
			bool found = false;
			for (QPointer<ConnectorItem> c2 : c1->connectedToItems())
			{
				auto *attached = c2->attachedTo();
				assert(attached);
				if (!attached) {
					DebugDialog::debug("skip null attached");
					continue;
				}
				found = true;
				if (!validConnectors(c1, c2)) {
					DebugDialog::debug(QString("ghost connection error: wire %1 %2 attached to %3 %4")
						.arg(wire->instanceTitle(), c1->connectorSharedID(),
							 attached->instanceTitle(), c2->connectorSharedID()));
					errors << wire;
					errors << attached;
				}
			}
			if (!found) {
				errors += findConnectors(c1);
			}
		}
	}
	DebugDialog::debug("debug wires done");

	return errors;
}

QSet<ItemBase *> DebugConnectors::doRoutingCheck() {
	DebugDialog::debug("debug connectors do");
	QHash<qint64, ItemBase *> bbID2ItemHash;
	QHash<qint64, ItemBase *> pcbID2ItemHash;
	QList<ItemBase *> bbList;
	collectPartsForCheck(bbList, m_breadboardGraphicsView->scene());
	Q_FOREACH (ItemBase * part, bbList) {
		bbID2ItemHash.insert(part->id(), part);
		//						QString("!!!!!! Duplicate breadboard part found. title:"
		//								"%1 id1: %2 id2: %3 moduleID1: %4 moduleID2: %5 "
		//								"viewIDname: 1: %6 2: %7 viewLayerIDs: 1: %8 2: %9")
		//						.arg(part->instanceTitle())
		//						.arg(firstPart->id())
		//						.arg(part->id())
		//						.arg(firstPart->moduleID())
		//						.arg(part->moduleID())
		//						.arg(firstPart->viewIDName())
		//						.arg(part->viewIDName())
		//						.arg(firstPart->viewLayerID())
		//						.arg(part->viewLayerID()));
	}

	QList<ItemBase *> pcbList;
	collectPartsForCheck(pcbList, m_pcbGraphicsView->scene());
	Q_FOREACH (ItemBase * part, pcbList) {
		if (part->viewLayerID() == ViewLayer::ViewLayerID::Silkscreen0) continue;
		if (part->viewLayerID() == ViewLayer::ViewLayerID::Silkscreen1) continue;
		if (part->viewLayerID() == ViewLayer::ViewLayerID::Board) continue;

		pcbID2ItemHash.insert(part->id(), part);
	}

	QSet<ItemBase *> errorList;

	QList<ItemBase *> schList;
	collectPartsForCheck(schList, m_schematicGraphicsView->scene());
	Q_FOREACH (ItemBase* schPart, schList) {
		if (dynamic_cast<NetLabel *>(schPart) != nullptr) continue;
		if (schPart->viewLayerID() == ViewLayer::ViewLayerID::SchematicText) continue;
		ItemBase * bbPart = bbID2ItemHash.value(schPart->id());
		ItemBase * pcbPart = pcbID2ItemHash.value(schPart->id());
		if (bbPart != nullptr) {
			QHash<QString, ConnectorItem *> bbID2ConnectorHash;
			Q_FOREACH (ConnectorItem * connectorItem, bbPart->cachedConnectorItems()) {
				bbID2ConnectorHash.insert(connectorItem->connectorSharedID(), connectorItem);
			}
			Q_FOREACH (ConnectorItem * schConnectorItem, schPart->cachedConnectorItems()) {
				ConnectorItem * bbConnectorItem = bbID2ConnectorHash.value(schConnectorItem->connectorSharedID());
				if (bbConnectorItem != nullptr) {
					if (bbConnectorItem != nullptr) {
						QSet<QString> schSet = getItemConnectorSet(schConnectorItem);
						QSet<QString> bbSet = getItemConnectorSet(bbConnectorItem);
						if (schSet != bbSet) {
							QString schSetString = TextUtils::setToString(schSet);
							QString bbSetString = TextUtils::setToString(bbSet);
							DebugDialog::debug(QString("Connectors with id: %1 for item: %2 have differing QSets. Schema{%3} BB{%4}").arg(
										   schConnectorItem->connectorSharedID(),
										   schPart->instanceTitle(),
										   schSetString,
										   bbSetString));

							errorList << schPart;
							errorList << bbPart;
						}
					}
				}
			}
		}
		if (pcbPart != nullptr) {
			QHash<QString, ConnectorItem *> pcbID2ConnectorHash;
			Q_FOREACH (ConnectorItem * connectorItem, pcbPart->cachedConnectorItems()) {
				pcbID2ConnectorHash.insert(connectorItem->connectorSharedID(), connectorItem);
			}
			Q_FOREACH (ConnectorItem * schConnectorItem, schPart->cachedConnectorItems()) {
				ConnectorItem * pcbConnectorItem = pcbID2ConnectorHash.value(schConnectorItem->connectorSharedID());
				if (pcbConnectorItem != nullptr) {
					if (pcbConnectorItem != nullptr) {
						QSet<QString> schSet = getItemConnectorSet(schConnectorItem);
						QSet<QString> pcbSet = getItemConnectorSet(pcbConnectorItem);
						if (schSet != pcbSet) {
							QSet<QString> setSchMinusPcb = schSet - pcbSet;
							QSet<QString> setPcbMinusSch = pcbSet - schSet;
							bool nonWireError = !setPcbMinusSch.isEmpty();
							Q_FOREACH(QString str, setSchMinusPcb) {
								if (!str.startsWith("Wire")) {
									nonWireError = true;
								} else {
									schSet -= str;
								}
							}
							if (nonWireError) {
								QString schSetString = TextUtils::setToString(schSet);
								QString pcbSetString = TextUtils::setToString(pcbSet);
								DebugDialog::debug(QString("Connectors with id: %1 for item: %2 have differing QSets. Schema{%3} PCB{%4}").arg(
											   schConnectorItem->connectorSharedID(),
											   schPart->instanceTitle(),
											   schSetString,
											   pcbSetString));

								errorList << schPart;
								errorList << pcbPart;
							}
						}
					}
				}
			}
		}
	}

	return errorList;
}

void DebugConnectors::reportErrors(QSet<ItemBase *> errors)
{
	QList<QPointer<ItemBase>> remaining;
	for (auto & ptr : m_displayedBugs) {
		if (ptr && !errors.contains(ptr.data())) {
			ptr->clearBug(QStringLiteral("routing"));
		} else if (ptr) {
			remaining << ptr;
		}
	}
	m_displayedBugs = remaining;

	for (ItemBase * item : errors) {
		if (item) {
			//: Bug marker attached to one part whose connectors differ between views.
			//: "connector" = connection point (pin/pad); "views" = breadboard/schematic/pcb.
			item->showBug(QStringLiteral("routing"),
				QStringList() << tr("Routing error: connector mismatch between views."));
			m_displayedBugs << QPointer<ItemBase>(item);
		}
	}
}
