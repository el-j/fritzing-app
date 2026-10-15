/*******************************************************************

Part of the Fritzing project - http://fritzing.org
Copyright (c) 2007-2019 Fritzing

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

#include "virtualwire.h"
#include "../connectors/connectoritem.h"
#include "../model/modelpart.h"

 const double VirtualWire::ShapeWidthExtra = 3.0;

VirtualWire::VirtualWire( ModelPart * modelPart, ViewLayer::ViewID viewID,  const ViewGeometry & viewGeometry, long id, QMenu * itemMenu  )
	: ClipableWire(modelPart, viewID,  viewGeometry,  id, itemMenu, false)
{
	// note: at this point in fritzing development, the VirtualWire class is only ever used for ratsnest lines
	modelPart->setLocalProp("ratsnest", "true");
	m_colorWasNamed = false;
	setFlag(QGraphicsItem::ItemIsSelectable, false);
}

VirtualWire::~VirtualWire() {
}

void VirtualWire::setWireWidth(double width,
							   InfoGraphicsView *infoGraphicsView,
							   double hoverStrokeWidth)
{
	m_wireWidth = width;
	Wire::setWireWidth(m_wireWidth, infoGraphicsView, hoverStrokeWidth);
}

void VirtualWire::paint (QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget ) {
	if (m_hidden) return;

	qreal currentScale = painter->worldTransform().m11();
	double minScale = qMax(currentScale, 6.0);
	m_adjustedHoverStrokeWidth = m_hoverStrokeWidth / minScale;
	m_adjustedWidth = m_wireWidth / minScale;
	m_pen.setWidthF(m_adjustedWidth);
	Wire::paint(painter, option, widget);
}

QString VirtualWire::makeWireSVG(QPointF offset, double dpi, double printerScale, bool blackOnly)
{
	return makeWireSVGAux(offset, dpi, printerScale, blackOnly, 0.1);
}

double VirtualWire::wireWidth()
{
	return m_wireWidth;
}

void VirtualWire::paintHover(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
	Q_UNUSED(widget);
	Q_UNUSED(option);

	painter->save();
	painter->setOpacity(HoverOpacity);
	painter->fillPath(this->shape(), QBrush(HoverColor));

	painter->restore();
}

QPainterPath VirtualWire::shape() const
{
	QPainterPath path;
	// if (m_squashShape) return path;
	if (m_line == QLineF()) {
		return path;
	}

	path.moveTo(m_line.p1());
	path.lineTo(m_line.p2());

	QPainterPathStroker ps(m_pen);
	ps.setWidth(m_adjustedHoverStrokeWidth);
	ps.setDashPattern(Qt::SolidLine);

	QPainterPath p = ps.createStroke(path);
	return p;
}

void VirtualWire::connectionChange(ConnectorItem * onMe, ConnectorItem * onIt, bool connect) {
	checkVisibility(onMe, onIt, connect);
}

FSvgRenderer * VirtualWire::setUpConnectors(ModelPart * modelPart, ViewLayer::ViewID viewID) {
	FSvgRenderer * renderer = Wire::setUpConnectors(modelPart, viewID);
	hideConnectors();
	return renderer;
}

void VirtualWire::hideConnectors() {
	// m_connector0 and m_connector1 may not yet be initialized
	Q_FOREACH (ConnectorItem * item, cachedConnectorItems()) {
		item->setHidden(true);
	}
}

void VirtualWire::inactivateConnectors() {
	// m_connector0 and m_connector1 may not yet be initialized
	Q_FOREACH (ConnectorItem * item, cachedConnectorItems()) {
		item->setInactive(true);
	}
}

void VirtualWire::setInactive(bool inactivate) {
	ItemBase::setInactive(inactivate);

	if (!inactivate) {
		inactivateConnectors();
	}
}

void VirtualWire::setHidden(bool hide) {
	ItemBase::setHidden(hide);

	if (!hide) {
		hideConnectors();
	}
}

void VirtualWire::tempRemoveAllConnections() {
	ConnectorItem * connectorItem = connector0();
	for (int j = connectorItem->connectedToItems().count() - 1; j >= 0; j--) {
		connectorItem->connectedToItems()[j]->tempRemove(connectorItem, false);
		connectorItem->tempRemove(connectorItem->connectedToItems()[j], false);
	}
	connectorItem = connector1();
	for (int j = connectorItem->connectedToItems().count() - 1; j >= 0; j--) {
		connectorItem->connectedToItems()[j]->tempRemove(connectorItem, false);
		connectorItem->tempRemove(connectorItem->connectedToItems()[j], false);
	}
}

void VirtualWire::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
	// ignore clicks where our connectors are supposed to be
	// so the click can percolate to some other graphicsitem that can use it

	QList<QGraphicsItem *> items = scene()->items(event->scenePos());
	if (items.contains(m_connector0) || items.contains(m_connector1)) {
		event->ignore();
		return;
	}

	// set selectable flag temporarily here so that dragging out a bendpoint is enabled
	setFlag(QGraphicsItem::ItemIsSelectable, true);
	ClipableWire::mousePressEvent(event);
	setFlag(QGraphicsItem::ItemIsSelectable, false);
}

void VirtualWire::setColorWasNamed(bool colorWasNamed) {
	m_colorWasNamed = colorWasNamed;
}

bool VirtualWire::colorWasNamed() {
	return m_colorWasNamed;
}

