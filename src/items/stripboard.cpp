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

#include "stripboard.h"
#include "../utils/graphicsutils.h"
#include "../utils/familypropertycombobox.h"
#include "../svg/gerbergenerator.h"
#include "../sketch/infographicsview.h"
#include "moduleidnames.h"
#include "../connectors/connectoritem.h"
#include "../connectors/busshared.h"
#include "../connectors/connectorshared.h"
#include "../debugdialog.h"

#include <QCursor>
#include <QBitmap>


//////////////////////////////////////////////////

// TODO:
//
//	new cursors, new hover states?
//	disconnect and reconnect affected parts
//	swapping

static QCursor * SpotFaceCutterCursor = nullptr;
static QCursor * MagicWandCursor = nullptr;

static bool ShiftDown = false;
static QPointF OriginalShiftPos;
static bool ShiftX = false;
static bool ShiftY = false;
static bool SpaceBarWasPressed = false;
static constexpr double MinMouseMove = 2;

static QPainterPath HPath;
static QPainterPath VPath;

static QString HorizontalString("horizontal strips");
static QString VerticalString("vertical strips");
static QString EmptyString("no strips");

/////////////////////////////////////////////////////////////////////

Stripbit::Stripbit(const QPainterPath & path, int x, int y, bool horizontal, QGraphicsItem * parent = nullptr)
	: QGraphicsPathItem(path, parent)
{
	if (SpotFaceCutterCursor == nullptr) {
		QPixmap pixmap(":resources/images/cursor/spot_face_cutter.png");
		SpotFaceCutterCursor = new QCursor(pixmap, 0, 0);
	}

	if (MagicWandCursor == nullptr) {
		QPixmap pixmap(":resources/images/cursor/magic_wand.png");
		MagicWandCursor = new QCursor(pixmap, 0, 0);
	}

	setZValue(-999);			// beneath connectorItems

	setPen(Qt::NoPen);
	// TODO: don't hardcode this color
	setBrush(QColor(0xbc, 0x94, 0x51));							// QColor(0xc4, 0x9c, 0x59)


	m_horizontal = horizontal;
	m_x = x;
	m_y = y;
	m_inHover = m_removed = false;

	setAcceptHoverEvents(true);
	setAcceptedMouseButtons(Qt::LeftButton);
	setFlag(QGraphicsItem::ItemIsMovable, true);
	setFlag(QGraphicsItem::ItemIsSelectable, false);

}

Stripbit::~Stripbit() {
}

void Stripbit::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
	double newOpacity = 1;
	if (m_removed) {
		if (m_inHover) newOpacity = 0.50;
		else newOpacity = 0.00;
	}
	else {
		if (m_inHover) newOpacity = 0.40;
		else newOpacity = 1.00;
	}

	double opacity = painter->opacity();
	painter->setOpacity(newOpacity);
	QGraphicsPathItem::paint(painter, option, widget);
	painter->setOpacity(opacity);

}

void Stripbit::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
	InfoGraphicsView * infoGraphicsView = InfoGraphicsView::getInfoGraphicsView(this);
	if ((infoGraphicsView != nullptr) && infoGraphicsView->spaceBarIsPressed()) {
		event->ignore();
		return;
	}

	if (!event->buttons() && Qt::LeftButton) {
		event->ignore();
		return;
	}

	if (dynamic_cast<ItemBase *>(this->parentItem())->moveLock()) {
		event->ignore();
		return;
	}

	if (event->modifiers() & Qt::ShiftModifier) {
		ShiftDown = true;
		ShiftX = ShiftY = false;
		OriginalShiftPos = event->scenePos();
	}

	event->accept();
	dynamic_cast<Stripboard *>(this->parentItem())->initCutting(this);
	m_removed = !m_removed;
	m_inHover = false;
	m_changed = true;
	update();


	//DebugDialog::debug("got press");
}

void Stripbit::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
	Q_UNUSED(event);
	dynamic_cast<Stripboard *>(this->parentItem())->reinitBuses(true);
}

void Stripbit::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
	if (!event->buttons() && (Qt::LeftButton != 0u)) return;

	if (ShiftDown && !(event->modifiers() & Qt::ShiftModifier)) {
		ShiftDown = false;
	}

	//DebugDialog::debug("got move");

	Stripbit * other = nullptr;
	QPointF p = event->scenePos();
	if (ShiftDown) {
		if (ShiftX) {
			// moving along x, constrain y
			p.setY(OriginalShiftPos.y());
		}
		else if (ShiftY) {
			// moving along y, constrain x
			p.setX(OriginalShiftPos.x());
		}
		else {
			double dx = qAbs(p.x() - OriginalShiftPos.x());
			double dy = qAbs(p.y() - OriginalShiftPos.y());
			if (dx - dy > MinMouseMove) {
				ShiftX = true;
			}
			else if (dy - dx > MinMouseMove) {
				ShiftY = true;
			}
		}
	}


	if (!ShiftDown && ((event->modifiers() & Qt::ShiftModifier) != 0u)) {
		ShiftDown = true;
		ShiftX = ShiftY = false;
		OriginalShiftPos = event->scenePos();
	}

	Q_FOREACH (QGraphicsItem * item, scene()->items(p)) {
		other = dynamic_cast<Stripbit *>(item);
		if (other != nullptr) break;
	}

	if (other == nullptr) return;

	//DebugDialog::debug("got other");

	if (other->removed() == m_removed) return;

	//DebugDialog::debug("change other");

	other->setRemoved(m_removed);
	other->setChanged(true);
	other->update();
	dynamic_cast<Stripboard *>(this->parentItem())->restoreRowColors(other);

}

void Stripbit::hoverEnterEvent( QGraphicsSceneHoverEvent * event )
{
	if (dynamic_cast<ItemBase *>(this->parentItem())->moveLock()) return;

	InfoGraphicsView * infoGraphicsView = InfoGraphicsView::getInfoGraphicsView(this);
	if ((infoGraphicsView != nullptr) && infoGraphicsView->spaceBarIsPressed()) {
		SpaceBarWasPressed = true;
		return;
	}

	SpaceBarWasPressed = false;
	setCursor(m_removed ? *MagicWandCursor : *SpotFaceCutterCursor);
	Q_UNUSED(event);
	m_inHover = true;
	update();
}

void Stripbit::hoverLeaveEvent ( QGraphicsSceneHoverEvent * event )
{
	if (dynamic_cast<ItemBase *>(this->parentItem())->moveLock()) return;
	if (SpaceBarWasPressed) return;

	unsetCursor();
	Q_UNUSED(event);
	m_inHover = false;
	update();
}



bool Stripbit::horizontal() {
	return m_horizontal;
}

void Stripbit::setRemoved(bool removed) {
	m_removed = removed;
}

bool Stripbit::removed() {
	return m_removed;
}

void Stripbit::setChanged(bool changed) {
	m_changed = changed;
}

bool Stripbit::changed() {
	return m_changed;
}

int Stripbit::y() {
	return m_y;
}

int Stripbit::x() {
	return m_x;
}

QString Stripbit::makeRemovedString() {
	return QString("%1.%2%3 ").arg(m_x).arg(m_y).arg(m_horizontal ? 'h' : 'v');
}

/////////////////////////////////////////////////////////////////////

StripConnector::StripConnector() {
	down = right = nullptr;
	connectorItem = nullptr;
}

struct StripLayout {
	QString name;
	int rows;
	int columns;
	QString buses;

	StripLayout(QString name, int rows, int columns, QString buses);
};

StripLayout::StripLayout(QString name_, int rows_, int columns_, QString buses_) {
	name = name_;
	rows = rows_;
	columns = columns_;
	buses = buses_;
}

static QList<StripLayout> StripLayouts;

/////////////////////////////////////////////////////////////////////

Stripboard::Stripboard( ModelPart * modelPart, ViewLayer::ViewID viewID, const ViewGeometry & viewGeometry, long id, QMenu * itemMenu, bool doLabel)
	: Perfboard(modelPart, viewID, viewGeometry, id, itemMenu, doLabel)
{
	getXY(m_x, m_y, m_size);
	if (StripLayouts.count() == 0) {
		initStripLayouts();
	}

	m_layout = modelPart->localProp("layout").toString();
	if (m_layout.isEmpty()) {
		m_layout = modelPart->properties().value("m_layout");
		if (!m_layout.isEmpty()) {
			modelPart->setLocalProp("layout", m_layout);
		}
	}
}

Stripboard::~Stripboard() {
	Q_FOREACH (StripConnector * sc, m_strips) {
		delete sc;
	}

	m_strips.clear();
}

QString Stripboard::retrieveSvg(ViewLayer::ViewLayerID viewLayerID, QHash<QString, QString> & svgHash, bool blackOnly, double dpi, double & factor)
{
	QString svg = Perfboard::retrieveSvg(viewLayerID, svgHash, blackOnly, dpi, factor);
	if (svg.isEmpty()) return svg;
	if (!svg.contains(GerberGenerator::MagicBoardOutlineID)) return svg;

	/*
	QFile file(filename());
	if (!file.open(QFile::ReadOnly | QFile::Text)) {
		return svg;
	}
	QString originalSvg = file.readAll();
	file.close();

	QString stripSvg = originalSvg.left(svg.indexOf(">") + 1);
	*/

	QString stripSvg;
	Q_FOREACH(QGraphicsItem * item, childItems()) {
		auto * stripbit = dynamic_cast<Stripbit *>(item);
		if (stripbit == nullptr) continue;
		if (stripbit->removed()) continue;

		QPointF p = stripbit->pos();
		QRectF r = stripbit->boundingRect();

		stripSvg += QString("<path stroke='none' stroke-width='0' fill='%6' "
		                    "d='m%1,%2 %3,0 0,%4 -%3,0z m0,%4a%5,%5  0,1,0 0,-%4z  m%3,-%4a%5,%5 0,1,0 0,%4z'/>\n")
		            .arg(p.x() * dpi / GraphicsUtils::SVGDPI)
		            .arg(p.y() * dpi / GraphicsUtils::SVGDPI)
		            .arg(r.width() * dpi / GraphicsUtils::SVGDPI )
		            .arg(r.height() * dpi / GraphicsUtils::SVGDPI)
		            .arg(r.height() * dpi * .5 / GraphicsUtils::SVGDPI)
		            .arg(blackOnly ? "black" : "#c49c59")
		            ;
	}

	svg.truncate(svg.lastIndexOf("</g>"));

	return svg + stripSvg + "</g>\n";
}

QString Stripboard::genFZP(const QString & moduleid)
{
	QString fzp = Perfboard::genFZP(moduleid);
	fzp.replace(ModuleIDNames::PerfboardModuleIDName, ModuleIDNames::Stripboard2ModuleIDName);
	fzp.replace("Perfboard", "Stripboard");
	fzp.replace("perfboard", "stripboard");
	fzp.replace("stripboard.svg", "perfboard.svg");  // replaced just above; restore it
	QString findString("<properties>");
	int ix = fzp.indexOf(findString);
	if (ix > 0) {
		fzp.insert(ix + findString.length(), "<property name='layout'></property>");
		if (moduleid.endsWith(ModuleIDNames::StripboardModuleIDName)) {
			fzp.insert(ix + findString.length(), "<property name='oldstyle'>yes</property>");
		}
	}
	return fzp;
}

bool Stripboard::collectExtraInfo(QWidget * parent, const QString & family, const QString & prop, const QString & value, bool swappingEnabled, QString & returnProp, QString & returnValue, QWidget * & returnWidget, bool & hide)
{
	return Perfboard::collectExtraInfo(parent, family, prop, value, swappingEnabled, returnProp, returnValue, returnWidget, hide);
}

void Stripboard::addedToScene(bool temporary)
{
	Perfboard::addedToScene(temporary);
	if (this->scene() == nullptr) return;
	if (temporary) return;
	if (m_viewID != ViewLayer::BreadboardView) return;

//	QList<QGraphicsItem *> items = childItems();

	if (HPath.isEmpty()) {
		makeInitialPath();
	}

	int count = m_x * m_y;
	for (int i = 0; i < count; i++) {
		m_strips.append(new StripConnector);
	}

	bool oldStyle = false;
	if (moduleID().endsWith(ModuleIDNames::StripboardModuleIDName) ||  !modelPart()->properties().value("oldstyle", "").isEmpty()) {
		modelPart()->modelPartShared()->setModuleID(QString("%1.%2%3").arg(m_x).arg(m_y).arg(ModuleIDNames::Stripboard2ModuleIDName));
		oldStyle = true;
		m_layout = HorizontalString;
	}

	QString config = prop("buses");
	QString additionalConfig;

	Q_FOREACH (ConnectorItem * ci, cachedConnectorItems()) {
		int cx, cy;
		getXY(cx, cy, ci->connectorSharedName());
		StripConnector * sc =  getStripConnector(cx, cy);
		sc->connectorItem = ci;

		if (cx < m_x - 1) {
			auto * stripbit = new Stripbit(HPath, cx, cy, true, this);
			QRectF r = ci->rect();
			stripbit->setPos(r.center().x(), r.top());
			stripbit->setVisible(true);
			sc->right = stripbit;
			if (!oldStyle) {
				additionalConfig += stripbit->makeRemovedString();
			}
		}

		if (cy < m_y - 1) {
			auto * stripbit = new Stripbit(VPath, cx, cy, false, this);
			QRectF r = ci->rect();
			stripbit->setPos(r.left(), r.center().y());
			stripbit->setVisible(true);
			sc->down = stripbit;
			if (oldStyle) {
				additionalConfig += stripbit->makeRemovedString();
			}
		}
	}

	if (config.isEmpty() || oldStyle) {
		config += additionalConfig;
		setProp("buses", config);
	}

	if (m_layout.isEmpty()) m_layout = VerticalString;

	QStringList removed = config.split(" ", Qt::SkipEmptyParts);

	Q_FOREACH (QString name, removed) {
		int cx, cy;
		if (getXY(cx, cy, name)) {
			StripConnector * sc = getStripConnector(cx, cy);
			bool vertical = name.contains("v");
			if (vertical) {
				if (sc->down != nullptr) sc->down->setRemoved(true);
			}
			else {
				if (sc->right != nullptr) sc->right->setRemoved(true);
			}
		}
	}

	reinitBuses(false);
}

QString Stripboard::genModuleID(QMap<QString, QString> & currPropsMap)
{
	QString size = currPropsMap.value("size");
	return size + ModuleIDNames::Stripboard2ModuleIDName;
}

void Stripboard::initCutting(Stripbit *)
{
	m_beforeCut.clear();
	Q_FOREACH (QGraphicsItem * item, childItems()) {
		auto * stripbit = dynamic_cast<Stripbit *>(item);
		if (stripbit == nullptr) continue;

		stripbit->setChanged(false);
		if (stripbit->removed()) {
			m_beforeCut += stripbit->makeRemovedString();
		}
	}
}

void appendConnectors(QList<ConnectorItem *> & connectorItems, ConnectorItem * connectorItem) {
	if (connectorItem == nullptr) return;

	Q_FOREACH (ConnectorItem * ci, connectorItem->connectedToItems()) {
		connectorItems.append(ci);
	}
}

void Stripboard::reinitBuses(bool triggerUndo)
{
	if (triggerUndo) {
		QString afterCut;
		QSet<ConnectorItem *> affectedConnectors;
		int changeCount = 0;
		bool connect = true;
		Q_FOREACH (QGraphicsItem * item, childItems()) {
			auto * stripbit = dynamic_cast<Stripbit *>(item);
			if (stripbit == nullptr) continue;

			if (stripbit->removed()) {
				afterCut += stripbit->makeRemovedString();
			}
		}

		collectTo(affectedConnectors);

		InfoGraphicsView * infoGraphicsView = InfoGraphicsView::getInfoGraphicsView(this);
		if (infoGraphicsView != nullptr) {
			QString changeType = (connect) ? tr("Restored") : tr("Cut") ;
			QString changeText = tr("%1 %n strip(s)", "", changeCount).arg(changeType);
			QList<ConnectorItem *> affected = affectedConnectors.values();
			infoGraphicsView->changeBus(this, connect, m_beforeCut, afterCut, affected, changeText, m_layout, m_layout);  // affectedConnectors is used for updating ratsnests; it's just a wild guess
		}

		return;
	}
	if (viewID() != ViewLayer::BreadboardView) return;

	Q_FOREACH (BusShared * busShared, m_buses) delete busShared;
	m_buses.clear();

	Q_FOREACH (QGraphicsItem * item, childItems()) {
		auto * stripbit = dynamic_cast<Stripbit *>(item);
		if (stripbit == nullptr) {
			auto * connectorItem = dynamic_cast<ConnectorItem *>(item);
			if (connectorItem == nullptr) continue;

			connectorItem->connector()->connectorShared()->setBus(nullptr);
			connectorItem->connector()->setBus(nullptr);
			continue;
		}

	}

	QString busPropertyString;
	Q_FOREACH (QGraphicsItem * item, childItems()) {
		auto * stripbit = dynamic_cast<Stripbit *>(item);
		if (stripbit == nullptr) continue;

		if (stripbit->removed()) {
			busPropertyString += stripbit->makeRemovedString();
		}
	}

	QList<ConnectorItem *> visited;
	for (int iy = 0; iy < m_y; iy++) {
		for (int ix = 0; ix < m_x; ix++) {
			StripConnector * sc = getStripConnector(ix, iy);
			if (visited.contains(sc->connectorItem)) continue;

			QList<ConnectorItem *> connected;
			collectConnected(ix, iy, connected);
			visited.append(connected);
			nextBus(connected);
		}
	}

	modelPart()->clearBuses();
	modelPart()->initBuses();
	modelPart()->setLocalProp("buses",  busPropertyString);



	QList<ConnectorItem *> visited2;
	Q_FOREACH (ConnectorItem * connectorItem, cachedConnectorItems()) {
		connectorItem->restoreColor(visited2);
	}

	update();
}

void Stripboard::collectConnected(int ix, int iy, QList<ConnectorItem *> & connected) {
	StripConnector * sc = getStripConnector(ix, iy);
	if (connected.contains(sc->connectorItem)) return;

	connected << sc->connectorItem;

	if ((sc->right != nullptr) && !sc->right->removed()) {
		collectConnected(ix + 1, iy, connected);

	}
	if ((sc->down != nullptr) && !sc->down->removed()) {
		collectConnected(ix, iy + 1, connected);
	}

	StripConnector * left = ix > 0 ? getStripConnector(ix - 1, iy) : nullptr;
	if ((left != nullptr) && (left->right != nullptr) && !left->right->removed()) {
		collectConnected(ix - 1, iy, connected);
	}

	StripConnector * up = iy > 0 ? getStripConnector(ix, iy - 1) : nullptr;
	if ((up != nullptr) && (up->down != nullptr) && !up->down->removed()) {
		collectConnected(ix, iy - 1, connected);
	}
}

void Stripboard::nextBus(QList<ConnectorItem *> & soFar)
{
	if (soFar.count() > 1) {
		auto * busShared = new BusShared(QString::number(m_buses.count()));
		m_buses.append(busShared);
		Q_FOREACH (ConnectorItem * connectorItem, soFar) {
			busShared->addConnectorShared(connectorItem->connector()->connectorShared());
		}
	}
	soFar.clear();
}

void Stripboard::setProp(const QString & prop, const QString & value)
{
	if (prop.compare("buses") == 0) {
		QStringList removed = value.split(" ", Qt::SkipEmptyParts);
		Q_FOREACH (QGraphicsItem * item, childItems()) {
			auto * stripbit = dynamic_cast<Stripbit *>(item);
			if (stripbit == nullptr) continue;

			QString removedString = stripbit->makeRemovedString();
			removedString.chop(1);          // remove trailing space
			bool remove = removed.contains(removedString);
			stripbit->setRemoved(remove);
			if (remove) {
				removed.removeOne(removedString);
			}
		}

		reinitBuses(false);
		return;
	}

	if (prop.compare("layout") == 0) {
		m_layout = value;
	}

	Perfboard::setProp(prop, value);
}

void Stripboard::getConnectedColor(ConnectorItem * ci, QBrush &brush, QPen &pen, double & opacity, double & negativePenWidth, bool & negativeOffsetRect) {
	Perfboard::getConnectedColor(ci, brush, pen, opacity, negativePenWidth, negativeOffsetRect);
	opacity *= .66667;
}

void Stripboard::restoreRowColors(Stripbit * stripbit)
{
	// TODO: find a quick way to update the buses (and connectorItem colors) in just the row...
	Q_UNUSED(stripbit);
	//int y = stripbit->y();
	//stripbit = m_firstColumn.at(y);


	//ci->restoreColor(false, 0, false);
}

QString Stripboard::getRowLabel() {
	return tr("rows");
}

QString Stripboard::getColumnLabel() {
	return tr("columns");
}

void Stripboard::makeInitialPath() {
	ConnectorItem * ciFirst = nullptr;
	ConnectorItem * ciNextH = nullptr;
	ConnectorItem * ciNextV = nullptr;
	Q_FOREACH (ConnectorItem * ci, cachedConnectorItems()) {
		int cx, cy;
		getXY(cx, cy, ci->connectorSharedName());
		if (cy == 0 && cx == 0) {
			ciFirst = ci;
			break;
		}
	}
	Q_FOREACH (ConnectorItem * ci, cachedConnectorItems()) {
		int cx, cy;
		getXY(cx, cy, ci->connectorSharedName());
		if (cy == 0 && cx == 1) {
			ciNextH = ci;
			if (ciNextV != nullptr) break;
		}
		else if (cy == 1 && cx == 0) {
			ciNextV = ci;
			if (ciNextH != nullptr) break;
		}
	}

	if (ciFirst == nullptr) return;
	if (ciNextH == nullptr) return;
	if (ciNextV == nullptr) return;

	QRectF r1 = ciFirst->rect();
	QRectF rh = ciNextH->rect();

	double h = r1.height();
	double w = rh.center().x() - r1.center().x();

	r1.moveTo(-(r1.width() / 2), 0);
	rh.moveTo(w - (rh.width() / 2), 0);

	HPath.addRect(0, 0, w / 2, h);
	HPath.arcTo(r1, 90, -180);
	HPath.addRect(w / 2, 0, w / 2, h);
	HPath.moveTo(w, 0);
	HPath.arcTo(rh, 90, 180);

	r1 = ciFirst->rect();
	QRectF rv = ciNextV->rect();

	h = rv.center().y() - r1.center().y();
	w = r1.width();

	r1.moveTo(0, -(r1.height() / 2));
	rv.moveTo(0, h - (rv.height() / 2));

	VPath.addRect(0, 0, w, h / 2);
	VPath.arcTo(r1, 0, -180);
	VPath.addRect(0, h / 2, w, h / 2);
	VPath.moveTo(0, h);
	VPath.arcTo(rv, 0, 180);
}

StripConnector * Stripboard::getStripConnector(int ix, int iy) {
	return m_strips.at((iy * m_x) + ix);
}

void Stripboard::swapEntry(const QString & text) {

	auto * comboBox = qobject_cast<FamilyPropertyComboBox *>(sender());
	if (comboBox == nullptr) return;

	if (comboBox->prop().compare("layout", Qt::CaseInsensitive) == 0) {
		InfoGraphicsView * infoGraphicsView = InfoGraphicsView::getInfoGraphicsView(this);
		if (infoGraphicsView == nullptr) return;

		QString afterCut;
		if (text.compare(HorizontalString, Qt::CaseInsensitive) == 0) {
			Q_FOREACH (QGraphicsItem * item, childItems()) {
				auto * stripbit = dynamic_cast<Stripbit *>(item);
				if (stripbit == nullptr) continue;

				if (!stripbit->horizontal()) {
					afterCut += stripbit->makeRemovedString();
				}
			}
		}
		else if (text.compare(VerticalString, Qt::CaseInsensitive) == 0) {
			Q_FOREACH (QGraphicsItem * item, childItems()) {
				auto * stripbit = dynamic_cast<Stripbit *>(item);
				if (stripbit == nullptr) continue;

				if (stripbit->horizontal()) {
					afterCut += stripbit->makeRemovedString();
				}
			}
		}
		else if (text.compare(EmptyString, Qt::CaseInsensitive) == 0) {
			Q_FOREACH (QGraphicsItem * item, childItems()) {
				auto * stripbit = dynamic_cast<Stripbit *>(item);
				if (stripbit == nullptr) continue;

				afterCut += stripbit->makeRemovedString();
			}
		}
		else {
			for (int i = 0; i < StripLayouts.count(); i++) {
				if (StripLayouts.at(i).name.compare(text) == 0) {
					StripLayout stripLayout = StripLayouts.at(i);
					QMap<QString, QString> propsMap;
					propsMap.insert("size", QString("%1.%2").arg(stripLayout.columns).arg(stripLayout.rows));
					propsMap.insert("type", "Stripboard");
					propsMap.insert("buses", stripLayout.buses);
					propsMap.insert("layout", text);
					InfoGraphicsView * infoGraphicsView = InfoGraphicsView::getInfoGraphicsView(this);
					if (infoGraphicsView != nullptr) {
						infoGraphicsView->swap(family(), "size", propsMap, this);
					}
					return;
				}
			}
		}

		if (!afterCut.isEmpty()) {
			initCutting(nullptr);
			QString changeText = tr("%1 layout").arg(text);
			QSet<ConnectorItem *> affectedConnectors;
			collectTo(affectedConnectors);
			QList<ConnectorItem *> affected = affectedConnectors.values();
			infoGraphicsView->changeBus(this, true, m_beforeCut, afterCut, affected, changeText, m_layout, text);  // affectedConnectors is used for updating ratsnests; it's just a wild guess
		}

		return;
	}


	Perfboard::swapEntry(text);
}

void Stripboard::collectTo(QSet<ConnectorItem *> & affectedConnectors) {
	Q_FOREACH (ConnectorItem * connectorItem, cachedConnectorItems()) {
		Q_FOREACH (ConnectorItem * toConnectorItem, connectorItem->connectedToItems()) {
			affectedConnectors.insert(toConnectorItem);
		}
	}
}

void Stripboard::initStripLayouts() {
	QFile file(":/resources/templates/stripboards.xml");
	if (!file.open(QIODevice::ReadOnly)) {
		DebugDialog::debug(QString("Unable to open :%1").arg(":/resources/templates/stripboards.xml"));
	}
	QString errorStr;
	int errorLine;
	int errorColumn;
	QDomDocument domDocument;

	StripLayout stripLayoutEmpty(EmptyString, 20, 30, "");
	StripLayouts.append(stripLayoutEmpty);

	if (!domDocument.setContent(&file, true, &errorStr, &errorLine, &errorColumn)) {
		DebugDialog::debug(QString("unable to parse stripboards.xml: %1 %2 %3").arg(errorStr).arg(errorLine).arg(errorColumn));
		return;
	}

	QDomElement root = domDocument.documentElement();
	QDomElement stripboard = root.firstChildElement("stripboard");
	while (!stripboard.isNull()) {
		QString name = stripboard.attribute("name");
		bool rok;
		int rows = stripboard.attribute("rows").toInt(&rok);
		bool cok;
		int columns = stripboard.attribute("columns").toInt(&cok);
		QString buses = stripboard.attribute("buses");
		stripboard = stripboard.nextSiblingElement("stripboard");
		if (!rok) continue;
		if (!cok) continue;
		if (name.isEmpty() || buses.isEmpty()) continue;

		StripLayout stripLayout(name, rows, columns, buses);
		StripLayouts.append(stripLayout);
	}
}

QStringList Stripboard::collectValues(const QString & family, const QString & prop, QString & value) {
	QStringList values = Perfboard::collectValues(family, prop, value);

	if (prop.compare("layout", Qt::CaseInsensitive) == 0) {
		Q_FOREACH (StripLayout stripLayout, StripLayouts) {
			values.append(stripLayout.name);
			value = m_layout;
		}
	}

	return values;
}

QString Stripboard::getNewBuses(bool vertical) {
	QString newBuses;

	QString hv = vertical ? "h" : "v";
	int offsetX = vertical ? 1 : 0;
	int offsetY = vertical ? 0 : 1;

	for (int i = 0; i < m_xEdit->text().toInt() - offsetX; i++) {
		for (int j = 0; j < m_yEdit->text().toInt() - offsetY; j++) {
			if (i < (m_x - offsetX) && j < (m_y - offsetY))
				continue; //Do not modify previous strips
			QString newBus = QString("%1.%2%3 ").arg(i).arg(j).arg(hv);
			newBuses += newBus;
		}
	}
	return newBuses;
}

void Stripboard::changeBoardSize()
{
	if (boardSizeWarning()) {
		return;
	}

	QString newSize = QString("%1.%2").arg(m_xEdit->text(), m_yEdit->text());
	m_propsMap.insert("size", newSize);
	// We have to create the "buses" (strip segments that are broken) for the stripboard.
	// We do not modify the previous buses. If we extend the board, we just break the strips
	// based on the layout (vertical or horizontal strips). These bus properties are passed
	// to the swap mechanism, which creates a new part and sets the new properties (the buses).

	QString buses = prop("buses");
	QString newBuses;
	if(m_layout.compare(VerticalString) == 0 || m_layout.compare(EmptyString) == 0) {
		newBuses += getNewBuses(true);
	}
	if(m_layout.compare(HorizontalString) == 0 || m_layout.compare(EmptyString) == 0) {
		newBuses += getNewBuses(false);
	}

	m_propsMap.insert("buses", buses + newBuses);

	InfoGraphicsView * infoGraphicsView = InfoGraphicsView::getInfoGraphicsView(this);
	if (infoGraphicsView != nullptr) {
		infoGraphicsView->swap(family(), "size", m_propsMap, this);
	}
}
