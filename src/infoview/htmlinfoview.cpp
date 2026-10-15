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

#include <QApplication>
#include <QBuffer>
#include <QCache>
#include <QKeyEvent>
#include <QHBoxLayout>
#include <QSettings>
#include <QPalette>
#include <QFontMetricsF>
#include <QScrollBar>
#include <QPainter>
#include <QSvgRenderer>
#include <QSignalBlocker>
#include <qmath.h>

#include "htmlinfoview.h"
#include "scalediconframe.h"
#include "../sketch/infographicsview.h"
#include "../model/modelpart.h"
#include "../items/symbolpaletteitem.h"
#include "../items/hole.h"
#include "../utils/familypropertycombobox.h"
#include "../debugdialog.h"
#include "../connectors/connector.h"
#include "../utils/flineedit.h"
#include "../utils/clickablelabel.h"
#include "../utils/textutils.h"
#include "utils/misc.h"
#include "items/partfactory.h"



#define HTML_EOF "</body>\n</html>"

static constexpr int MaxSpinBoxWidth = 60;
static constexpr int AfterSpinBoxWidth = 5;

/////////////////////////////////////
QString format3(double d) {
	return QString("%1").arg(d, 0, 'f', 3);
}

//////////////////////////////////////


TagLabel::TagLabel(QWidget * parent) : QLabel(parent)
{
}

QSize TagLabel::sizeHint() const
{
	QSize hint = QLabel::sizeHint();
	QString t = text();
	if (t.isEmpty()) return hint;

	QFontMetricsF fm(this->font());
	double textWidth = fm.horizontalAdvance(t);

	QWidget * w = this->window();
	QPoint pos(0,0);
	pos = this->mapTo(w, pos);

	int myWidth = w->width() - pos.x() - 10;
	if (textWidth < myWidth) {
		return hint;
	}

	int lines = qCeil(textWidth / myWidth);
	int h = lines * fm.height();

	return QSize(myWidth, h);
}

//////////////////////////////////////

HtmlInfoView::HtmlInfoView(QWidget * parent) : QScrollArea(parent)
{
	this->setWidgetResizable(true);

	m_lastTitleItemBase = nullptr;
	m_lastSpiceModelPart = nullptr;
	m_lastTagsModelPart = nullptr;
	m_lastRevisionsModelPart = nullptr;
	m_lastConnectorItem = nullptr;
	m_lastIconItemBase = nullptr;
	m_lastPropsModelPart = nullptr;
	m_lastPropsItemBase = nullptr;

	m_tinyMode = false;

	m_partTitle = nullptr;
	m_partUrl = nullptr;
	m_partVersion = nullptr;
	m_lockCheckbox = nullptr;
	m_stickyCheckbox = nullptr;
	m_connDescr = nullptr;
	m_spiceTextLabel = nullptr;
	m_tagsTextLabel = nullptr;
	m_revisionsTextLabel = nullptr;
	m_lastSwappingEnabled = false;
	m_lastItemBase = nullptr;
	m_setContentTimer.setSingleShot(true);
	m_setContentTimer.setInterval(10);
	connect(&m_setContentTimer, SIGNAL(timeout()), this, SLOT(setContent()));

	m_currentItem = nullptr;
	m_currentSwappingEnabled = false;

	m_layerWidget = nullptr;

}


HtmlInfoView::~HtmlInfoView() {
	Q_FOREACH (PropThing * propThing, m_propThings) {
		delete propThing;
	}
	m_propThings.clear();
}

void HtmlInfoView::init(bool tinyMode) {
	m_tinyMode = tinyMode;

	auto * mainFrame = new QFrame(this);
	mainFrame->setObjectName("infoViewMainFrame");

	auto *vlo = new QVBoxLayout(mainFrame);
	vlo->setSpacing(0);
	vlo->setContentsMargins(0, 0, 0, 0);
	vlo->setSizeConstraint( QLayout::SetMinAndMaxSize );

	/* Part Title */

	m_titleEdit = new FLineEdit(mainFrame);
	m_titleEdit->setObjectName("instanceTitleEditor");
	m_titleEdit->setToolTip(tr("Change the part label here"));
	m_titleEdit->setAlignment(Qt::AlignLeft);

	connect(m_titleEdit, SIGNAL(editingFinished()), this, SLOT(setInstanceTitle()), Qt::QueuedConnection);
	connect(m_titleEdit, SIGNAL(mouseEnter()), this, SLOT(instanceTitleEnter()));
	connect(m_titleEdit, SIGNAL(mouseLeave()), this, SLOT(instanceTitleLeave()));
	connect(m_titleEdit, SIGNAL(editable(bool)), this, SLOT(instanceTitleEditable(bool)));

	setInstanceTitleColors(m_titleEdit, QColor(0xaf, 0xaf, 0xb4), QColor(0x00, 0x00, 0x00)); //b3b3b3, 575757
	m_titleEdit->setAutoFillBackground(true);
	// don't let the title editor swallow undo/redo it can't use (see eventFilter)
	m_titleEdit->installEventFilter(this);

	vlo->addWidget(m_titleEdit);
	if (tinyMode) m_titleEdit->setVisible(false);

	m_iconFrame = new ScaledIconFrame(this);
	vlo->addWidget(m_iconFrame);

	m_partUrl = new TagLabel(this);
	m_partUrl->setWordWrap(false);
	m_partUrl->setObjectName("infoViewPartUrl");
	m_partUrl->setOpenExternalLinks(true);
	vlo->addWidget(m_partUrl);
	if (tinyMode) m_partUrl->setVisible(false);

	m_partTitle = new TagLabel(this);
	m_partTitle->setWordWrap(true);
	m_partTitle->setObjectName("infoViewPartTitle");
	m_partTitle->setOpenExternalLinks(true);
	vlo->addWidget(m_partTitle);
	if (tinyMode) m_partTitle->setVisible(false);

	m_placementLabel = new QLabel(tr("Placement"), nullptr);
	m_placementLabel->setObjectName("expandableViewLabel");
	vlo->addWidget(m_placementLabel);

	m_placementFrame = new QFrame(this);
	m_placementFrame->setObjectName("infoViewPropertyFrame");
	m_placementLayout = new QGridLayout(m_placementFrame);
	m_placementLayout->setSpacing(0);
	m_placementLayout->setContentsMargins(0, 0, 0, 0);

	m_layerLabel = new QLabel(tr("pcb layer"), this);
	m_layerLabel->setObjectName("propNameLabel");
	m_layerLabel->setWordWrap(true);
	m_placementLayout->addWidget(m_layerLabel, 0, 0);

	m_layerFrame = new QFrame(this);
	m_layerFrame->setObjectName("propValueFrame");
	m_layerLayout = new QVBoxLayout(m_layerFrame);
	m_layerLayout->setSpacing(0);
	m_layerLayout->setContentsMargins(0, 0, 0, 0);
	m_layerFrame->setLayout(m_layerLayout);

	m_placementLayout->addWidget(m_layerFrame, 0, 1);

	makeLocationFrame();
	m_placementLayout->addWidget(m_locationLabel, 1, 0);
	m_placementLayout->addWidget(m_locationFrame, 1, 1);

	makeRotationFrame();
	m_placementLayout->addWidget(m_rotationLabel, 2, 0);
	m_placementLayout->addWidget(m_rotationFrame, 2, 1);

	makeLockFrame();
	m_placementLayout->addWidget(m_lockLabel, 3, 0);
	m_placementLayout->addWidget(m_lockFrame, 3, 1);


	m_placementFrame->setLayout(m_placementLayout);
	vlo->addWidget(m_placementFrame);

	m_propLabel = new QLabel(tr("Properties"), nullptr);
	m_propLabel->setObjectName("expandableViewLabel");
	vlo->addWidget(m_propLabel);

	m_propFrame = new QFrame(this);
	m_propFrame->setObjectName("infoViewPropertyFrame");
	m_propLayout = new QGridLayout(m_propFrame);
	m_propLayout->setSpacing(0);
	m_propLayout->setContentsMargins(0, 0, 0, 0);
	m_propFrame->setLayout(m_propLayout);
	vlo->addWidget(m_propFrame);

	m_spiceLabel = new QLabel(tr("SPICE"), NULL);
	m_spiceLabel->setObjectName("expandableViewLabel");
	vlo->addWidget(m_spiceLabel);

	m_spiceTextLabel = new TagLabel(this);
	m_spiceTextLabel->setWordWrap(true);
	m_spiceTextLabel->setObjectName("tagsValue");
	vlo->addWidget(m_spiceTextLabel);

	m_tagLabel = new QLabel(tr("Tags"), NULL);
	m_tagLabel->setObjectName("expandableViewLabel");
	vlo->addWidget(m_tagLabel);
	if (tinyMode) m_tagLabel->setVisible(false);

	m_tagsTextLabel = new TagLabel(this);
	m_tagsTextLabel->setWordWrap(true);
	m_tagsTextLabel->setObjectName("tagsValue");
	vlo->addWidget(m_tagsTextLabel);
	if (tinyMode) m_tagsTextLabel->setVisible(false);

	auto * versionFrame = new QFrame(this);
	auto * versionLayout = new QHBoxLayout();
	m_partVersion = new QLabel();
	m_partVersion->setObjectName("infoViewPartVersion");
	m_partVersion->setToolTip(tr("Part version number"));
	m_partVersion->setOpenExternalLinks(false);
	m_partVersion->setAlignment(Qt::AlignLeft | Qt::AlignTop);
	connect(m_partVersion, SIGNAL(linkActivated(const QString &)), this, SLOT(clickObsolete(const QString &)));
	versionLayout->addWidget(m_partVersion, 0, Qt::AlignLeft);
	versionLayout->addStretch(1);
	versionFrame->setLayout(versionLayout);
	vlo->addWidget(versionFrame);

	// Revisions section (the part's <history> entries), styled like the Tags/SPICE sections.
	m_revisionsLabel = new QLabel(tr("Revisions"), NULL);
	m_revisionsLabel->setObjectName("expandableViewLabel");
	m_revisionsLabel->setVisible(false);
	vlo->addWidget(m_revisionsLabel);

	m_revisionsTextLabel = new TagLabel(this);
	m_revisionsTextLabel->setWordWrap(true);
	m_revisionsTextLabel->setObjectName("tagsValue");
	m_revisionsTextLabel->setOpenExternalLinks(false);
	m_revisionsTextLabel->setVisible(false);
	vlo->addWidget(m_revisionsTextLabel);

	m_connLabel = new QLabel(tr("Connections"), nullptr);
	m_connLabel->setObjectName("expandableViewLabel");
	vlo->addWidget(m_connLabel);
	if (tinyMode) m_connLabel->setVisible(false);

	m_connFrame = new QFrame(this);
	m_connFrame->setObjectName("connectionsFrame");

	auto * connLayout = new QGridLayout(m_connFrame);
	connLayout->setSpacing(0);
	connLayout->setContentsMargins(0, 0, 0, 0);
	m_connFrame->setLayout(connLayout);

	auto * descrLabel = new QLabel(tr("connection"), this);
	descrLabel->setObjectName("connectionsLabel");
	m_connDescr = new QLabel(this);
	m_connDescr->setObjectName("connectionsValue");
	connLayout->addWidget(descrLabel, 0, 0);
	connLayout->addWidget(m_connDescr, 0, 1);

	auto * nameLabel = new QLabel(tr("name"), this);
	nameLabel->setObjectName("connectionsLabel");
	m_connName = new QLabel(this);
	m_connName->setObjectName("connectionsValue");
	connLayout->addWidget(nameLabel, 1, 0);
	connLayout->addWidget(m_connName, 1, 1);

	auto * typeLabel = new QLabel(tr("type"), this);
	typeLabel->setObjectName("connectionsLabel");
	m_connType = new QLabel(this);
	m_connType->setObjectName("connectionsValue");
	connLayout->addWidget(typeLabel, 2, 0);
	connLayout->addWidget(m_connType, 2, 1);

	vlo->addWidget(m_connFrame);

	if (m_tinyMode) m_connFrame->setVisible(false);

	vlo->addSpacerItem(new QSpacerItem(1, 1, QSizePolicy::Minimum, QSizePolicy::Expanding));

	mainFrame->setLayout(vlo);

	this->setWidget(mainFrame);
}

void HtmlInfoView::resizeEvent(QResizeEvent *event)
{
	QSize newSize = event->size();
	int widthDifference = newSize.width() - m_lastSizeWithScrollbarsAlwaysOn.width();
	int heightDifference = newSize.height() - m_lastSizeWithScrollbarsAlwaysOn.height();

	int verticalScrollbarWidth = verticalScrollBar()->sizeHint().width();
	int horizontalScrollbarHeight = horizontalScrollBar()->sizeHint().height();
	int safetyMargin = 5;

	bool horizontalScrollBarVisible = horizontalScrollBar()->isVisible();
	if (horizontalScrollBarPolicy() == Qt::ScrollBarAsNeeded &&
			horizontalScrollBarVisible) {
		setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
		m_lastSizeWithScrollbarsAlwaysOn.setWidth(newSize.width());
//		qDebug() << "Switched horizontal policy to AlwaysOn, newSize:" << newSize << ", previousSize:" << previousSize;
	}
	if (horizontalScrollBarPolicy() == Qt::ScrollBarAlwaysOn &&
			widthDifference > verticalScrollbarWidth + safetyMargin) {
		setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
//		qDebug() << "Switched horizontal policy to AsNeeded, newSize:" << newSize << ", previousSize:" << previousSize;
	}

	bool verticalScrollBarVisible = verticalScrollBar()->isVisible();
	if (verticalScrollBarPolicy() == Qt::ScrollBarAsNeeded &&
			verticalScrollBarVisible) {
		setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
		m_lastSizeWithScrollbarsAlwaysOn.setHeight(newSize.height());
//		qDebug() << "Switched vertical policy to AlwaysOn, newSize:" << newSize << ", previousSize:" << previousSize;
	}
	if (verticalScrollBarPolicy() == Qt::ScrollBarAlwaysOn &&
			heightDifference > horizontalScrollbarHeight + safetyMargin) {
		setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
//		qDebug() << "Switched vertical policy to AsNeeded, newSize:" << newSize << ", previousSize:" << previousSize;
	}

	QScrollArea::resizeEvent(event);
}


void HtmlInfoView::viewItemInfo(InfoGraphicsView *, ItemBase* item, bool swappingEnabled)
{
	m_setContentTimer.stop();
	m_lastItemBase = m_pendingItemBase = item;
	m_lastSwappingEnabled = m_pendingSwappingEnabled = swappingEnabled;
	m_setContentTimer.start();
}

void HtmlInfoView::hoverEnterItem(InfoGraphicsView *, QGraphicsSceneHoverEvent *, ItemBase * item, bool swappingEnabled) {
	m_setContentTimer.stop();
	m_pendingItemBase = item;
	m_pendingSwappingEnabled = swappingEnabled;
	m_setContentTimer.start();
}

void HtmlInfoView::hoverLeaveItem(InfoGraphicsView *, QGraphicsSceneHoverEvent *, ItemBase * itemBase) {
	Q_UNUSED(itemBase);
	//DebugDialog::debug(QString("hoverLeaveItem itembase %1").arg(itemBase ? itemBase->instanceTitle() : "NULL"));
	m_setContentTimer.stop();
	m_pendingItemBase = m_lastItemBase;
	m_pendingSwappingEnabled = m_lastSwappingEnabled;
	m_setContentTimer.start();
}

void HtmlInfoView::viewConnectorItemInfo(QGraphicsSceneHoverEvent *, ConnectorItem * connectorItem) {

	int count = connectorItem != nullptr ? connectorItem->connectionsCount() : 0;
	if (m_lastConnectorItem == connectorItem && m_lastConnectorItemCount == count) return;

	m_lastConnectorItem = connectorItem;
	m_lastConnectorItemCount = count;

	Connector * connector = nullptr;
	if (connectorItem != nullptr) {
		if (connectorItem->attachedTo() != m_lastItemBase) {
			return;
		}

		connector = connectorItem->connector();
	}

	if (m_connDescr != nullptr) {
		m_connDescr->setText(connectorItem != nullptr ? tr("connected to %n item(s)", "", connectorItem->connectionsCount()) : "");
		m_connName->setText(connector != nullptr ? connector->connectorSharedName() : "");
		m_connType->setText(connector != nullptr ? Connector::connectorNameFromType(connector->connectorType()) : "");
	}

}

void HtmlInfoView::hoverEnterConnectorItem(InfoGraphicsView *igv, QGraphicsSceneHoverEvent *event, ConnectorItem * item, bool swappingEnabled) {
	Q_UNUSED(event)
	Q_UNUSED(swappingEnabled)
	Q_UNUSED(igv)
	viewConnectorItemInfo(event, item);
}

void HtmlInfoView::hoverLeaveConnectorItem(InfoGraphicsView *igv, QGraphicsSceneHoverEvent *event, ConnectorItem *connItem) {
	Q_UNUSED(event);
	Q_UNUSED(connItem);
	Q_UNUSED(igv);
	viewConnectorItemInfo(event, nullptr);
}

void HtmlInfoView::appendStuff(ItemBase* item, bool swappingEnabled) {
	Wire *wire = qobject_cast<Wire*>(item);
	if (wire != nullptr) {
		appendWireStuff(wire, swappingEnabled);
	} else {
		appendItemStuff(item, swappingEnabled);
	}
}

// ---- group-mode title summaries ----

static QString groupNetLabelNames(const QList<SymbolPaletteItem *> & labels) {
	// e.g. "2×VCC, GND" — collapse repeats into a multiplier, first-appearance order.
	QStringList order;
	QHash<QString, int> counts;
	Q_FOREACH (SymbolPaletteItem * netLabel, labels) {
		QString title = netLabel->getInspectorTitle();
		if (!counts.contains(title)) order << title;
		counts[title] += 1;
	}
	QStringList parts;
	Q_FOREACH (const QString & title, order) {
		int n = counts.value(title);
		parts << (n > 1 ? QString("%1×%2").arg(n).arg(title) : title);
	}
	return parts.join(", ");
}

static QString groupHoleDiameters(const QList<Hole *> & holes) {
	QString minStr, maxStr;
	double minIn = 0, maxIn = 0;
	bool first = true;
	Q_FOREACH (Hole * hole, holes) {
		QStringList dt = hole->holeSize().split(",");
		if (dt.isEmpty()) continue;
		bool ok;
		double in = TextUtils::convertToInches(dt.at(0), &ok, false);
		if (!ok) continue;
		if (first || in < minIn) { minIn = in; minStr = dt.at(0); }
		if (first || in > maxIn) { maxIn = in; maxStr = dt.at(0); }
		first = false;
	}
	if (first) return QString();
	return (minStr == maxStr) ? minStr : QString("%1 – %2").arg(minStr).arg(maxStr);
}

static QString groupWireWidths(const QList<Wire *> & wires) {
	double minMils = 0, maxMils = 0;
	bool first = true;
	Q_FOREACH (Wire * wire, wires) {
		double m = wire->mils();
		if (first || m < minMils) minMils = m;
		if (first || m > maxMils) maxMils = m;
		first = false;
	}
	if (first) return QString();
	if (qAbs(minMils - maxMils) < 0.01) {
		return QObject::tr("%1 mil").arg(QString::number(qRound(minMils)));
	}
	return QObject::tr("%1 – %2 mil").arg(QString::number(qRound(minMils))).arg(QString::number(qRound(maxMils)));
}

static QString groupPartTitles(const QList<ItemBase *> & items) {
	// The list of selected parts by their instance title, e.g. "R1, R2, R3, C1".
	QStringList titles;
	Q_FOREACH (ItemBase * ib, items) {
		QString t = ib->instanceTitle();
		if (t.isEmpty()) t = ib->title();
		titles << t;
	}
	return titles.join(", ");
}

static QString groupPartTypeCounts(const QList<ItemBase *> & items) {
	// Count by part family, e.g. "3×Resistor, 2×Capacitor" (first-appearance order).
	QStringList order;
	QHash<QString, int> counts;
	Q_FOREACH (ItemBase * ib, items) {
		QString fam = (ib->modelPart() != nullptr) ? ib->modelPart()->family() : QString();
		if (fam.isEmpty()) fam = ib->title();
		if (!counts.contains(fam)) order << fam;
		counts[fam] += 1;
	}
	QStringList parts;
	Q_FOREACH (const QString & fam, order) {
		int n = counts.value(fam);
		parts << (n > 1 ? QString("%1×%2").arg(n).arg(fam) : fam);
	}
	return parts.join(", ");
}

void HtmlInfoView::showGroupTitle(const QString & summary, const QString & countText) {
	// Same-size, non-editable title (avoids the layout shift of hiding it). setUpTitle
	// re-enables it when a single item is shown again.
	m_titleEdit->setText(summary);
	m_titleEdit->setEnabled(false);
	m_titleEdit->setCursorPosition(0);
	partTitle(countText, QString(), QString(), false);
}

void HtmlInfoView::appendWireStuff(Wire* wire, bool swappingEnabled) {
	if (wire == nullptr) return;

	ModelPart *modelPart = wire->modelPart();
	if (modelPart == nullptr) return;
	if (modelPart->modelPartShared() == nullptr) return;

	// When several wires are selected, the inspector is in group mode: the shared width/
	// color controls apply to all selected wires (already handled by changeWireColor/
	// changeWireWidthMils), so here we just signpost it with an "N wires" title.
	QList<Wire *> selectedWires;
	bool wireGroup = false;
	{
		InfoGraphicsView * igv = InfoGraphicsView::getInfoGraphicsView(wire);
		if ((igv != nullptr) && igv->collectSelectedWires(selectedWires) > 1
		        && selectedWires.contains(wire)) {
			wireGroup = true;
		}
	}

	QString autoroutable = wire->getAutoroutable() ? tr("(autoroutable)") : "";
	QString nameString = tr("Wire");
	if (swappingEnabled) {
		if (wire->getRatsnest()) {
			nameString = tr("Ratsnest line", "A virtual wire, a planned connection that does not yet have a layout.");
		}
		else if(wire->getTrace()) {
			nameString = tr("Trace wire %1", "A wire routed on a PCB").arg(autoroutable);
		}
	}
	else {
		nameString = modelPart->description();
	}
	m_lockFrame->setVisible(false);
	m_lockLabel->setVisible(false);
	m_locationFrame->setVisible(false);
	m_locationLabel->setVisible(false);
	m_rotationFrame->setVisible(false);
	m_rotationLabel->setVisible(false);

	setUpTitle(wire);
	setUpIcons(wire, swappingEnabled);
	m_titleEdit->setVisible(!m_tinyMode);

	if (wireGroup) {
		// Read-only title = min/max trace width; heading = selection count.
		showGroupTitle(groupWireWidths(selectedWires), tr("%n wires", "", selectedWires.count()));
	}
	else {
		partTitle(nameString, modelPart->version(), modelPart->url(), modelPart->isObsolete());
	}

	displayProps(modelPart, wire, swappingEnabled);

	bool hasLayer = (wire->viewID() == ViewLayer::PCBView) && wire->getViewGeometry().getPCBTrace();
	m_placementFrame->setVisible(hasLayer);
	m_placementLabel->setVisible(hasLayer);

	addTags(modelPart);
	addRevisions(modelPart);
}

void HtmlInfoView::appendItemStuff(ItemBase* base, bool swappingEnabled) {
	if (base == nullptr) return;

	appendItemStuff(base, base->modelPart(), swappingEnabled);
}

void HtmlInfoView::appendItemStuff(ItemBase * itemBase, ModelPart * modelPart, bool swappingEnabled) {

	if (modelPart == nullptr) return;
	if (modelPart->modelPartShared() == nullptr) return;

	// When several net labels are selected, the inspector is in group mode: the title field
	// shows the (read-only) list of names and the heading shows the selection count.
	QList<SymbolPaletteItem *> selectedNetLabels;
	bool netLabelGroup = false;
	{
		auto * symbol = dynamic_cast<SymbolPaletteItem *>(itemBase);
		if ((symbol != nullptr) && symbol->isOnlyNetLabel()) {
			InfoGraphicsView * igv = InfoGraphicsView::getInfoGraphicsView(itemBase);
			if ((igv != nullptr) && igv->collectSelectedNetLabels(selectedNetLabels) > 1
			        && selectedNetLabels.contains(symbol)) {
				netLabelGroup = true;
			}
		}
	}

	// Likewise, several selected holes/vias form a group; the shared diameter/ring controls
	// apply to all (handled in PaletteItem), so here we just show an "N holes" title.
	QList<Hole *> selectedHoles;
	bool holeGroup = false;
	{
		auto * hole = dynamic_cast<Hole *>(itemBase);
		if (hole != nullptr) {
			InfoGraphicsView * igv = InfoGraphicsView::getInfoGraphicsView(itemBase);
			if ((igv != nullptr) && igv->collectSelectedHoles(selectedHoles) > 1
			        && selectedHoles.contains(hole)) {
				holeGroup = true;
			}
		}
	}

	// Any other multi-selection of parts is a generic group: list the part titles in the
	// (read-only) title field and a per-family count below the icons.
	QList<ItemBase *> genericGroupItems;
	bool genericGroup = false;
	if (!netLabelGroup && !holeGroup) {
		InfoGraphicsView * igv = InfoGraphicsView::getInfoGraphicsView(itemBase);
		if ((igv != nullptr) && (igv->scene() != nullptr)) {
			Q_FOREACH (QGraphicsItem * gItem, igv->scene()->selectedItems()) {
				auto * ib = dynamic_cast<ItemBase *>(gItem);
				if (ib == nullptr) continue;
				ItemBase * chief = ib->layerKinChief();
				if (!genericGroupItems.contains(chief)) genericGroupItems.append(chief);
			}
		}
		genericGroup = (itemBase != nullptr) && genericGroupItems.count() > 1
		               && genericGroupItems.contains(itemBase->layerKinChief());
	}

	setUpTitle(itemBase);
	setUpIcons(itemBase, swappingEnabled);

	m_titleEdit->setVisible(!m_tinyMode);

	if (netLabelGroup) {
		// Read-only title = the list of net names; heading = selection count.
		showGroupTitle(groupNetLabelNames(selectedNetLabels),
		               tr("%n net labels", "", selectedNetLabels.count()));
	}
	else if (holeGroup) {
		// Read-only title = min/max diameter; heading = selection count.
		showGroupTitle(groupHoleDiameters(selectedHoles),
		               tr("%n holes", "", selectedHoles.count()));
	}
	else if (genericGroup) {
		// Read-only title = list of selected parts; heading = per-family count.
		showGroupTitle(groupPartTitles(genericGroupItems), groupPartTypeCounts(genericGroupItems));
	}
	else {
		QString nameString;
		if (swappingEnabled) {
			nameString = (itemBase) != nullptr ? itemBase->getInspectorTitle() : modelPart->title();
		}
		else {
			nameString = modelPart->description();
		}
		partTitle(nameString, modelPart->version(), modelPart->url(), modelPart->isObsolete());
	}
	m_lockFrame->setVisible(swappingEnabled);
	m_lockLabel->setVisible(swappingEnabled);
	m_lockCheckbox->setChecked(itemBase->moveLock());

	m_locationFrame->setVisible(swappingEnabled);
	m_locationLabel->setVisible(swappingEnabled);
	m_locationFrame->setDisabled(itemBase->moveLock());

	m_rotationFrame->setVisible(swappingEnabled);
	m_rotationLabel->setVisible(swappingEnabled);
	m_rotationFrame->setEnabled(itemBase->rotationAllowed());

	setLocation(itemBase);
	setRotation(itemBase);

	if (itemBase->isBaseSticky()) {
		m_stickyCheckbox->setVisible(true);
		m_stickyCheckbox->setChecked(itemBase->isSticky());
	}
	else {
		m_stickyCheckbox->setVisible(false);
	}

	displayProps(modelPart, itemBase, swappingEnabled);
	addSpice(modelPart);
	addTags(modelPart);
	addRevisions(modelPart);

	m_placementLabel->setVisible(swappingEnabled);
	m_placementFrame->setVisible(swappingEnabled);

}

void HtmlInfoView::setContent()
{
	m_setContentTimer.stop();

	// Refreshing the inspector can hide or (when a plugin widget is not reusable, see
	// displayProps / ItemBase::collectExtraInfo) destroy the property widget that currently
	// holds the keyboard focus — e.g. a combo or button the user just changed a value with.
	// That leaves the application without a focus widget, so shortcuts like undo/redo are no
	// longer delivered. Remember the inspector's focus so restoreFocusAfterRefresh can hand it
	// back after the refresh. We only restore focus the inspector already had, so ordinary
	// hover/selection refreshes never steal it from the sketch.
	QWidget * focusWidget = QApplication::focusWidget();
	bool hadFocus = isAncestorOf(focusWidget);
	QPointer<QWidget> previouslyFocused = hadFocus ? focusWidget : nullptr;

	if (m_pendingItemBase == nullptr) {
		setNullContent();
		m_setContentTimer.stop();
		if (hadFocus) {
			restoreFocusAfterRefresh(previouslyFocused);
		}
		return;
	}

	m_currentSwappingEnabled = m_pendingSwappingEnabled;

	appendStuff(m_pendingItemBase, m_pendingSwappingEnabled);
	setCurrentItem(m_pendingItemBase);

	if (!m_tinyMode) {
		m_connFrame->setVisible(m_pendingSwappingEnabled);
		m_tagLabel->setVisible(true);
		m_connLabel->setVisible(m_pendingSwappingEnabled);
	}
	m_spiceLabel->setVisible(true);
	m_propLabel->setVisible(true);
	m_propFrame->setVisible(true);

	m_setContentTimer.stop();

	// See note at the top of this method.
	if (hadFocus) {
		restoreFocusAfterRefresh(previouslyFocused);
	}
	//DebugDialog::debug(QString("end   updating %1").arg(QTime::currentTime().toString("HH:mm:ss.zzz")));

}

void HtmlInfoView::restoreFocusAfterRefresh(QWidget * previouslyFocused)
{
	QWidget * focusWidget = QApplication::focusWidget();
	if ((focusWidget != nullptr) && (focusWidget == previouslyFocused)) {
		return;   // focus survived the refresh (reused plugin widget, see displayProps)
	}

	// Hiding the focused widget during the refresh handed the focus to an arbitrary next
	// widget (or to nothing, if the widget was destroyed). Prefer giving it back to the
	// widget that had it — a reused plugin survives the refresh. Fall back to the inspector
	// itself, which at least keeps shortcuts like undo/redo working.
	if ((previouslyFocused != nullptr) && previouslyFocused->isVisible() && isAncestorOf(previouslyFocused)) {
		previouslyFocused->setFocus(Qt::OtherFocusReason);
	}
	else {
		setFocus(Qt::OtherFocusReason);
	}
}

bool HtmlInfoView::eventFilter(QObject * target, QEvent * event)
{
	// Text fields consume the undo/redo shortcuts for their internal text-editing history,
	// even when that history is empty — which it always is right after a committed value
	// change, because committing syncs the field programmatically via setText. So a ctrl-z
	// immediately after changing e.g. a board width died in the field. Let the field consume
	// undo/redo only while it can actually use them (i.e. while editing); otherwise decline
	// the shortcut override so the application's undo/redo action handles the key.
	// Installed on every text field shown in the inspector (see init and displayProps).
	if (event->type() == QEvent::ShortcutOverride) {
		auto * lineEdit = qobject_cast<QLineEdit *>(target);
		if (lineEdit != nullptr) {
			auto * keyEvent = static_cast<QKeyEvent *>(event);
			if ((keyEvent->matches(QKeySequence::Undo) && !lineEdit->isUndoAvailable())
					|| (keyEvent->matches(QKeySequence::Redo) && !lineEdit->isRedoAvailable())) {
				event->ignore();
				return true;
			}
		}
	}
	return QScrollArea::eventFilter(target, event);
}

QSize HtmlInfoView::sizeHint() const {
	return QSize(DockWidthDefault, InfoViewHeightDefault);
}

void HtmlInfoView::setCurrentItem(ItemBase * item) {
	m_currentItem = item;
}

void HtmlInfoView::unregisterCurrentItem() {
	m_setContentTimer.stop();
	setCurrentItem(nullptr);
	m_pendingItemBase = nullptr;
	m_setContentTimer.start();
}

void HtmlInfoView::unregisterCurrentItemIf(long id) {
	if (m_currentItem == nullptr) {
		return;
	}
	if (m_currentItem->id() == id) {
		unregisterCurrentItem();
	}
}

ItemBase *HtmlInfoView::currentItem() {
	return m_currentItem;
}

void HtmlInfoView::reloadContent(InfoGraphicsView * infoGraphicsView) {
	if(m_currentItem != nullptr) {
		viewItemInfo(infoGraphicsView, m_currentItem, m_currentSwappingEnabled);
	}
}

void HtmlInfoView::setNullContent()
{
	setUpTitle(nullptr);
	partTitle("", "", "", false);
	setUpIcons(nullptr, false);
	displayProps(nullptr, nullptr, false);
	addSpice(NULL);
	addTags(nullptr);
	addRevisions(nullptr);
	viewConnectorItemInfo(nullptr, nullptr);
	m_connFrame->setVisible(false);
	m_propFrame->setVisible(false);
	m_propLabel->setVisible(false);
	m_placementFrame->setVisible(false);
	m_placementLabel->setVisible(false);
	m_spiceLabel->setVisible(false);
	m_tagLabel->setVisible(false);
	m_connLabel->setVisible(false);
}

void HtmlInfoView::setInstanceTitle() {
	auto * edit = qobject_cast<FLineEdit *>(sender());
	if (edit == nullptr) return;
	if (!edit->isEnabled()) return;
	if (m_currentItem == nullptr) return;

	m_currentItem->setInspectorTitle(m_partTitle->text(), edit->text());
}

void HtmlInfoView::instanceTitleEnter() {
	auto * edit = qobject_cast<FLineEdit *>(sender());
	if (edit->isEnabled()) {
		setInstanceTitleColors(edit, QColor(0xeb, 0xeb, 0xee), QColor(0x00, 0x00, 0x00)); //c8c8c8, 575757
	}
}

void HtmlInfoView::instanceTitleLeave() {
	auto * edit = qobject_cast<FLineEdit *>(sender());
	if (edit->isEnabled()) {
		setInstanceTitleColors(edit, QColor(0xd2, 0xd2, 0xd7), QColor(0x00, 0x00, 0x00)); //b3b3b3, 575757
	}
}

void HtmlInfoView::instanceTitleEditable(bool editable) {
	auto * edit = qobject_cast<FLineEdit *>(sender());
	if (editable) {
		setInstanceTitleColors(edit, QColor(0xff, 0xff, 0xff), QColor(0x00, 0x00, 0x00)); //fcfcfc, 000000
	}
	else {
		setInstanceTitleColors(edit, QColor(0xd2, 0xd2, 0xd7), QColor(0x00, 0x00, 0x00)); //b3b3b3, 57575
	}
}

void HtmlInfoView::setInstanceTitleColors(FLineEdit * edit, const QColor & base, const QColor & text) {
	edit->setStyleSheet(QString("background: rgb(%1,%2,%3); color: rgb(%4,%5,%6);")
	                    .arg(base.red()).arg(base.green()).arg(base.blue())
	                    .arg(text.red()).arg(text.green()).arg(text.blue()) );
}

void HtmlInfoView::setUpTitle(ItemBase * itemBase)
{
	if (itemBase == m_lastTitleItemBase) {
		if (itemBase == nullptr) return;
		if (itemBase->instanceTitle().compare(m_titleEdit->text()) == 0) return;
	}

	m_lastTitleItemBase = itemBase;
	bool titleEnabled = true;
	if (itemBase != nullptr) {
		QString title = itemBase->getInspectorTitle();
		if (title.isEmpty()) {
			// assumes a part with an empty title only comes from the parts bin palette
			titleEnabled = false;
			title = itemBase->title();
		}
		if (itemBase->viewID() == ViewLayer::IconView) titleEnabled = false;
		m_titleEdit->setText(title);
	}
	else {
		titleEnabled = false;
		m_titleEdit->setText("");
	}
	m_titleEdit->setEnabled(titleEnabled);
	// helps keep it left aligned?
	m_titleEdit->setCursorPosition(0);

}

// Cache inspector icon pixmaps by moduleID + viewID to avoid repeated file I/O and SVG rendering.
// Cost is in bytes (160x160 RGBA = ~102KB per entry). 200 entries ~ 20MB max.
static QCache<QString, QPixmap> & inspectorIconCache() {
	static QCache<QString, QPixmap> cache(200);
	return cache;
}

static const QPixmap * cachedIconFromSvg(ItemBase * itemBase, ViewLayer::ViewID vid, bool swappingEnabled, QSize size) {
	if (itemBase == nullptr || itemBase->modelPart() == nullptr) return nullptr;

	// Check visibility
	if (itemBase->viewID() == vid) {
		if (!itemBase->isEverVisible()) return nullptr;
	} else {
		ItemBase * vItemBase = itemBase->modelPart()->viewItem(vid);
		if (vItemBase != nullptr && !vItemBase->isEverVisible()) return nullptr;
	}

	vid = itemBase->useViewIDForPixmap(vid, swappingEnabled);
	if (vid == ViewLayer::UnknownView) return nullptr;

	QString cacheKey = itemBase->modelPart()->moduleID() + "/" + QString::number(vid);
	if (const QPixmap * cached = inspectorIconCache().object(cacheKey)) {
		// DebugDialog::debug(QString("inspector icon cache hit: %1").arg(cacheKey));
		return cached;
	}

	if (!itemBase->modelPart()->hasViewFor(vid)) return nullptr;

	QString baseName = itemBase->modelPart()->hasBaseNameFor(vid);
	if (baseName.isEmpty()) return nullptr;

	QString filename = PartFactory::getSvgFilename(itemBase->modelPart(), baseName, true, true);
	if (filename.isEmpty()) return nullptr;

	QSvgRenderer renderer(filename);
	if (!renderer.isValid()) return nullptr;

	auto * pixmap = new QPixmap(size);
	pixmap->fill(Qt::transparent);
	QPainter painter(pixmap);
	QSize def = renderer.defaultSize();
	double newW = size.width();
	double newH = newW * def.height() / def.width();
	if (newH > size.height()) {
		newH = size.height();
		newW = newH * def.width() / def.height();
	}
	QRectF bounds((size.width() - newW) / 2.0, (size.height() - newH) / 2.0, newW, newH);
	renderer.render(&painter, bounds);
	painter.end();

	// DebugDialog::debug(QString("inspector icon cache miss, loaded SVG: %1 from %2").arg(cacheKey, filename));
	inspectorIconCache().insert(cacheKey, pixmap);
	return pixmap;
}

void HtmlInfoView::setUpIcons(ItemBase * itemBase, bool swappingEnabled) {
	if (m_lastIconItemBase == itemBase) return;

	m_lastIconItemBase = itemBase;

	QSize size = QSize(ScaledIconFrame::STANDARD_ICON_IMG_WIDTH, ScaledIconFrame::STANDARD_ICON_IMG_HEIGHT);

	const QPixmap *pixmap1 = cachedIconFromSvg(itemBase, ViewLayer::BreadboardView, swappingEnabled, size);
	const QPixmap *pixmap2 = cachedIconFromSvg(itemBase, ViewLayer::SchematicView, swappingEnabled, size);
	const QPixmap *pixmap3 = cachedIconFromSvg(itemBase, ViewLayer::PCBView, swappingEnabled, size);

	m_iconFrame->setIcons(pixmap1, pixmap2, pixmap3);
}

void HtmlInfoView::addSpice(ModelPart * modelPart) {
	if (m_spiceTextLabel == NULL) return;

	if (m_lastSpiceModelPart == modelPart) return;

	m_lastSpiceModelPart = modelPart;

	if (modelPart == NULL) {
		m_spiceTextLabel->setText("");
		return;
	}

	if(modelPart->spice().isEmpty()) {
		m_spiceTextLabel->setText(tr("No SPICE information. This part will not be simulated."));
		return;
	}

	QString spiceText = modelPart->spice().trimmed();
	if(!modelPart->spiceModel().isEmpty())
		spiceText.append("\nSPICE model:\n" + modelPart->spiceModel().trimmed());
	m_spiceTextLabel->setText(spiceText);
}

void HtmlInfoView::addTags(ModelPart * modelPart) {
	if (m_tagsTextLabel == nullptr) return;

	if (m_lastTagsModelPart == modelPart) return;

	m_lastTagsModelPart = modelPart;

	if (modelPart == nullptr || modelPart->tags().isEmpty()) {
		m_tagsTextLabel->setText("");
		return;
	}

	m_tagsTextLabel->setText(modelPart->tags().join(", "));
}

void HtmlInfoView::partTitle(const QString & title, const QString & version, const QString & url, bool isObsolete) {
	if (m_partTitle == nullptr) return;

	if (m_lastPartTitle == title && m_lastPartVersion == version) return;

	m_lastPartTitle = title;
	m_lastPartVersion = version;

	if (!m_tinyMode) {
		if (url.isEmpty()) {
			m_partUrl->setVisible(false);
			m_partUrl->setText("");
		}
		else {
			m_partUrl->setText(QString("<a href=\"%1\">%1</a>").arg(url));
			m_partUrl->setVisible(true);
		}
	}

	m_partTitle->setText(title);
	if (!version.isEmpty()) {
		m_partVersion->setText(tr("v. %1 %2").arg(version).arg(isObsolete ? QString("<a href='x'>%1</a>").arg(tr("obsolete")) : ""));
	}
	else m_partVersion->setText("");
}

void HtmlInfoView::addRevisions(ModelPart * modelPart) {
	if (m_revisionsTextLabel == nullptr) return;

	if (m_lastRevisionsModelPart == modelPart) return;
	m_lastRevisionsModelPart = modelPart;

	QString html;
	if (modelPart != nullptr) {
		// History may not be loaded yet for parts that came from the parts database.
		if (!modelPart->hasHistory()) modelPart->loadHistoryFromFile();

		const QList<HistoryEntry> & history = modelPart->history();
		// newest entry first
		for (int i = history.count() - 1; i >= 0; --i) {
			const HistoryEntry & entry = history.at(i);

			QString modeTag;
			if (entry.isRecommended()) modeTag = QString(" [%1]").arg(tr("recommended"));
			else if (entry.isRequired()) modeTag = QString(" [%1]").arg(tr("automatic"));

			QString date = entry.date.trimmed();
			QString author = entry.author.trimmed();
			QString description = entry.description.trimmed();

			if (!html.isEmpty()) html += "<br/>";
			html += QString("<b>%1</b>%2%3")
			        .arg(date.toHtmlEscaped(),
			             author.isEmpty() ? QString() : QString(" (%1)").arg(author.toHtmlEscaped()),
			             modeTag);
			if (!description.isEmpty()) {
				html += QString("<br/>%1").arg(description.toHtmlEscaped());
			}
		}
	}

	bool hasRevisions = !html.isEmpty() && !m_tinyMode;
	m_revisionsTextLabel->setText(html);
	m_revisionsTextLabel->setVisible(hasRevisions);
	if (m_revisionsLabel != nullptr) m_revisionsLabel->setVisible(hasRevisions);
}

void HtmlInfoView::displayProps(ModelPart * modelPart, ItemBase * itemBase, bool swappingEnabled)
{
	bool repeatPossible = (modelPart == m_lastPropsModelPart && itemBase == m_lastPropsItemBase && swappingEnabled == m_lastPropsSwappingEnabled);
	if (repeatPossible && modelPart == nullptr && itemBase == nullptr) {
		//DebugDialog::debug("display props bail");
		return;
	}

	m_propLayout->setEnabled(false);

	if (repeatPossible) {
		DebugDialog::debug(QString("repeat possible %1").arg(static_cast<int>(repeatPossible)));
	}

	bool wantDebug = false;
#ifndef QT_NO_DEBUG
	wantDebug = DebugDialog::enabled();
#endif

	QStringList keys;
	QHash<QString, QString> properties = getPartProperties(modelPart, itemBase, wantDebug, keys);
	QString family = properties.value("family", "").toLower();

	bool sl = false;
	if (keys.contains("layer")) {
		keys.removeOne("layer");
		if (itemBase != nullptr) sl = (itemBase->viewID() == ViewLayer::PCBView);
	}

	showLayers(sl, itemBase, family, properties.value("layer", ""), swappingEnabled);

	// Generic multi-selection rule: when several items are selected, show only the
	// properties they all have in common. Value-based editors (the per-type group handlers)
	// keep applying to the whole selection; generic swap-based combos are disabled here
	// (applying a part swap to every selected item is deferred).
	QList<ItemBase *> groupItems;
	{
		InfoGraphicsView * igv = (itemBase != nullptr) ? InfoGraphicsView::getInfoGraphicsView(itemBase) : nullptr;
		if ((igv != nullptr) && (igv->scene() != nullptr)) {
			Q_FOREACH (QGraphicsItem * gItem, igv->scene()->selectedItems()) {
				auto * ib = dynamic_cast<ItemBase *>(gItem);
				if (ib == nullptr) continue;
				ItemBase * chief = ib->layerKinChief();
				if (!groupItems.contains(chief)) groupItems.append(chief);
			}
		}
	}
	bool multiSelect = (itemBase != nullptr) && groupItems.count() > 1
	                   && groupItems.contains(itemBase->layerKinChief());

	if (multiSelect) {
		QStringList commonKeys;
		Q_FOREACH (const QString & key, keys) {
			bool inAll = true;
			Q_FOREACH (ItemBase * gi, groupItems) {
				if ((gi->modelPart() == nullptr) || !gi->modelPart()->properties().contains(key)) {
					inAll = false;
					break;
				}
			}
			if (inAll) commonKeys.append(key);
		}
		keys = commonKeys;

		// Manufacturer number / part number only make sense for a group of identical parts
		// (same moduleID). For a mixed group, drop them.
		bool identicalParts = true;
		Q_FOREACH (ItemBase * gi, groupItems) {
			if (gi->moduleID() != itemBase->moduleID()) { identicalParts = false; break; }
		}
		if (!identicalParts) {
			keys.removeAll("mn");
			keys.removeAll("mpn");
			keys.removeAll("part number");
		}
	}

	// True if the selected items disagree on the value of a property (so we blank it).
	auto valuesDiffer = [&groupItems](const QString & key) -> bool {
		QString v0;
		bool first = true;
		Q_FOREACH (ItemBase * gi, groupItems) {
			QString v = gi->getProperty(key);
			if (first) { v0 = v; first = false; }
			else if (v != v0) return true;
		}
		return false;
	};

	int ix = 0;
	Q_FOREACH(QString key, keys) {
		if (ix >= m_propThings.count()) {
			//DebugDialog::debug(QString("new prop thing %1").arg(ix));
			auto * propThing = new PropThing;
			propThing->m_plugin = nullptr;
			m_propThings.append(propThing);

			auto * propNameLabel = new QLabel(this);
			propNameLabel->setObjectName("propNameLabel");
			propNameLabel->setWordWrap(true);
			propThing->m_name = propNameLabel;
			m_propLayout->addWidget(propNameLabel, ix, 0);

			auto * valueFrame = new QFrame(this);
			valueFrame->setObjectName("propValueFrame");
			auto * vlayout = new QVBoxLayout(valueFrame);
			vlayout->setSpacing(0);
			vlayout->setContentsMargins(0, 0, 0, 0);
			propThing->m_layout = vlayout;
			propThing->m_frame = valueFrame;

			auto * propValueLabel = new QLabel(valueFrame);
			propValueLabel->setObjectName("propValueLabel");
			propValueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
			vlayout->addWidget(propValueLabel);
			propThing->m_value = propValueLabel;
			m_propLayout->addWidget(valueFrame, ix, 1);
		}

		PropThing * propThing = m_propThings.at(ix++);

		QWidget * oldPlugin = propThing->m_plugin;
		propThing->m_plugin = nullptr;

		QString value = properties.value(key,"");
		QString translatedName = ItemBase::translatePropertyName(key);
		QString resultKey, resultValue;
		QWidget * resultWidget = oldPlugin;
		bool result = false;
		bool hide = false;
		if (itemBase != nullptr) {
			result = itemBase->collectExtraInfo(propThing->m_name->parentWidget(), family, key, value, swappingEnabled, resultKey, resultValue, resultWidget, hide);
		}

		QString newName;
		QString newValue;
		QWidget * newWidget = nullptr;
		if (result && !hide) {
			newName = resultKey;
			if (resultWidget != nullptr) {
				newWidget = resultWidget;
				if (resultWidget != oldPlugin) {
					propThing->m_layout->addWidget(resultWidget);
				}
				else {
					oldPlugin = nullptr;
				}
				//DebugDialog::debug(QString("adding %1 %2").arg(newName).arg((long) resultWidget, 0, 16));
				propThing->m_plugin = resultWidget;

				// don't let the plugin's text fields swallow undo/redo they can't use
				// (installing twice is harmless, so no need to skip reused plugins)
				auto * lineEdit = qobject_cast<QLineEdit *>(resultWidget);
				if (lineEdit != nullptr) {
					lineEdit->installEventFilter(this);
				}
				for (QLineEdit * childEdit : resultWidget->findChildren<QLineEdit *>()) {
					childEdit->installEventFilter(this);
				}
			}
			else {
				newValue = resultValue;
			}
		}
		else {
			newName = translatedName;
			newValue = value;
		}

		if (multiSelect && !hide) {
			auto * familyCombo = qobject_cast<FamilyPropertyComboBox *>(newWidget);
			if ((familyCombo != nullptr) && key.compare("package", Qt::CaseInsensitive) == 0) {
				// Offer the packages common to all selected parts' families; choosing one
				// swaps every selected part (handled in MainWindow::swapSelectionForProp).
				QStringList common;
				InfoGraphicsView * igv = InfoGraphicsView::getInfoGraphicsView(itemBase);
				if (igv != nullptr) common = igv->commonPropValues("package");

				QSignalBlocker blocker(familyCombo);   // repopulating must not fire swapEntry
				familyCombo->clear();
				Q_FOREACH (const QString & v, common) familyCombo->addItem(v);
				QString shared = valuesDiffer(key) ? QString() : itemBase->getProperty(key);
				familyCombo->setCurrentIndex(shared.isEmpty() ? -1 : familyCombo->findText(shared));
				familyCombo->setEnabled(true);
			}
			else if (familyCombo != nullptr) {
				// Other generic swap combos would only swap the active item, so disable them
				// in a multi-selection (swap-to-all is deferred); blank if values differ.
				familyCombo->setEnabled(false);
				if (valuesDiffer(key)) familyCombo->setCurrentIndex(-1);
			}
			else if ((newWidget == nullptr) && valuesDiffer(key)) {
				// Plain value row whose items disagree: show nothing rather than one value.
				newValue = "";
			}
		}

		if (oldPlugin != nullptr) {
			clearPropThingPlugin(propThing, oldPlugin);
		}

		if (propThing->m_name->text().compare(newName) != 0) {
			propThing->m_name->setText(newName);
		}

		propThing->m_name->setVisible(!hide);
		propThing->m_frame->setVisible(!hide);

		if (newWidget == nullptr && propThing->m_value->text().compare(newValue) != 0) {
			propThing->m_value->setText(newValue);
		}
		propThing->m_value->setVisible(newWidget == nullptr && !hide);
	}

	for (int jx = ix; jx < m_propThings.count(); jx++) {
		PropThing * propThing = m_propThings.at(jx);
		propThing->m_name->setVisible(false);
		propThing->m_value->setVisible(false);
		propThing->m_frame->setVisible(false);
		if (propThing->m_plugin != nullptr) {
			propThing->m_plugin->setVisible(false);
		}
	}

	m_propLayout->setEnabled(true);


	/*
	foreach (PropThing * propThing, m_propThings) {
		if (propThing->m_layout->count() > 1) {
			DebugDialog::debug(QString("too many %1").arg(propThing->m_layout->count()));
		}
	}
	*/
}

void HtmlInfoView::clearPropThingPlugin(PropThing * propThing)
{

	if (propThing->m_plugin != nullptr) {
		clearPropThingPlugin(propThing, propThing->m_plugin);
		propThing->m_plugin = nullptr;
	}
}

void HtmlInfoView::clearPropThingPlugin(PropThing * propThing, QWidget * plugin)
{
	//DebugDialog::debug(QString("clearing %1").arg((long) plugin, 0, 16));

	propThing->m_layout->removeWidget(plugin);
	plugin->blockSignals(true);
	plugin->setVisible(false);          // seems to trigger an unwanted focus out signal
	plugin->deleteLater();
}


QHash<QString, QString> HtmlInfoView::getPartProperties(ModelPart * modelPart, ItemBase * itemBase, bool wantDebug, QStringList & keys)
{
	QHash<QString, QString> properties;
	if ((modelPart != nullptr) && (itemBase != nullptr)) {
		properties = itemBase->prepareProps(modelPart, wantDebug, keys);
	}

	return properties;
}

void HtmlInfoView::changeLock(bool lockState)
{
	if (m_currentItem == nullptr) return;
	if (m_currentItem->itemType() == ModelPart::Wire) return;

	InfoGraphicsView * infoGraphicsView = InfoGraphicsView::getInfoGraphicsView(m_currentItem);
	if (infoGraphicsView != nullptr) {
		infoGraphicsView->changeMoveLock(m_currentItem, lockState);		// undoable
	}
	else {
		m_currentItem->setMoveLock(lockState);
	}
	m_locationFrame->setDisabled(lockState);
	m_rotationFrame->setDisabled(lockState);
}


void HtmlInfoView::changeSticky(bool lockState)
{
	if (m_currentItem == nullptr) return;
	if (m_currentItem->itemType() == ModelPart::Wire) return;

	m_currentItem->setLocalSticky(lockState);
}

void HtmlInfoView::clickObsolete(const QString &) {
	// Hand the currently inspected part to the migration flow (-1 if none is shown).
	Q_EMIT clickObsoleteSignal(m_lastItemBase ? m_lastItemBase->id() : -1);
}

void HtmlInfoView::showLayers(bool show, ItemBase * itemBase, const QString & family, const QString & value, bool swappingEnabled) {

	if (m_layerWidget != nullptr) {
		m_layerLayout->removeWidget(m_layerWidget);
		m_layerWidget->blockSignals(true);
		m_layerWidget->setVisible(false);          // seems to trigger an unwanted focus out signal
		m_layerWidget->deleteLater();
		m_layerWidget = nullptr;
	}

	if (itemBase == nullptr) show = false;
	m_layerFrame->setVisible(show);
	m_layerLabel->setVisible(show);
	if (!show) return;

	QString resultKey, resultValue;
	bool hide;
	bool result = itemBase->collectExtraInfo(m_layerLabel->parentWidget(), family, "layer", value, swappingEnabled, resultKey, resultValue, m_layerWidget, hide);
	if (result && (m_layerWidget != nullptr)) {
		m_layerLayout->addWidget(m_layerWidget);
	}
}

void HtmlInfoView::makeLockFrame() {
	m_lockLabel = new QLabel("    ", this);
	m_lockLabel->setObjectName("propNameLabel");
	m_lockLabel->setWordWrap(true);

	m_lockFrame = new QFrame(this);
	auto * lockLayout = new QHBoxLayout();
	lockLayout->setSpacing(0);
	lockLayout->setContentsMargins(0, 0, 0, 0);
	m_lockFrame->setObjectName("propValueFrame");

	m_lockCheckbox = new QCheckBox(tr("Locked"));
	m_lockCheckbox->setObjectName("infoViewLockCheckbox");
	m_lockCheckbox->setToolTip(tr("Change the locked state of the part in this view. A locked part can't be moved."));
	connect(m_lockCheckbox, SIGNAL(clicked(bool)), this, SLOT(changeLock(bool)));
	lockLayout->addWidget(m_lockCheckbox);

	lockLayout->addSpacing(10);

	m_stickyCheckbox = new QCheckBox(tr("Sticky"));
	m_stickyCheckbox->setObjectName("infoViewLockCheckbox");
	m_stickyCheckbox->setToolTip(tr("Change the \"sticky\" state of the part in this view. When a sticky part is moved, objects on top of it also move."));
	connect(m_stickyCheckbox, SIGNAL(clicked(bool)), this, SLOT(changeSticky(bool)));
	lockLayout->addWidget(m_stickyCheckbox);

	lockLayout->addSpacerItem(new QSpacerItem(1,1, QSizePolicy::Expanding));

	m_lockFrame->setLayout(lockLayout);
}

void HtmlInfoView::makeLocationFrame() {
	m_locationLabel = new QLabel(tr("location"), this);
	m_locationLabel->setObjectName("propNameLabel");
	m_locationLabel->setWordWrap(true);

	m_locationFrame = new QFrame(this);
	auto * locationLayout = new QHBoxLayout();
	locationLayout->setSpacing(0);
	locationLayout->setContentsMargins(0, 0, 0, 0);
	m_locationFrame->setObjectName("propValueFrame");

	m_xEdit = new QDoubleSpinBox;
	m_xEdit->setDecimals(3);
	m_xEdit->setRange(-99999.999, 99999.999);
	m_xEdit->setKeyboardTracking(false);
	m_xEdit->setObjectName("infoViewDoubleSpinBox");
	m_xEdit->setMaximumWidth(MaxSpinBoxWidth);
	m_xEdit->setMinimumWidth(MaxSpinBoxWidth);
	m_xEdit->setLocale(QLocale::C);
	locationLayout->addWidget(m_xEdit);

	locationLayout->addSpacing(AfterSpinBoxWidth);

	m_yEdit = new QDoubleSpinBox;
	m_yEdit->setDecimals(3);
	m_yEdit->setRange(-99999.999, 99999.999);
	m_yEdit->setKeyboardTracking(false);
	m_yEdit->setObjectName("infoViewDoubleSpinBox");
	m_yEdit->setMaximumWidth(MaxSpinBoxWidth);
	m_yEdit->setMinimumWidth(MaxSpinBoxWidth);
	m_yEdit->setLocale(QLocale::C);
	locationLayout->addWidget(m_yEdit);

	locationLayout->addSpacing(3);

	m_unitsLabel = new ClickableLabel("px", this);
	m_unitsLabel->setObjectName("infoViewSpinBoxLabel");
	m_unitsLabel->setCursor(Qt::PointingHandCursor);
	locationLayout->addWidget(m_unitsLabel);

	locationLayout->addSpacerItem(new QSpacerItem(1,1, QSizePolicy::Expanding));

	m_locationFrame->setLayout(locationLayout);

	connect(m_xEdit, SIGNAL(valueChanged(double)), this, SLOT(xyEntry()));
	connect(m_yEdit, SIGNAL(valueChanged(double)), this, SLOT(xyEntry()));
	connect(m_unitsLabel, SIGNAL(clicked()), this, SLOT(unitsClicked()));
	unitsClicked();   // increments from px so that default is inches
}

void HtmlInfoView::makeRotationFrame() {
	m_rotationLabel = new QLabel(tr("rotation"), this);
	m_rotationLabel->setObjectName("propNameLabel");
	m_rotationLabel->setWordWrap(true);

	m_rotationFrame = new QFrame(this);
	auto * rotationLayout = new QHBoxLayout();
	rotationLayout->setSpacing(0);
	rotationLayout->setContentsMargins(0, 0, 0, 0);
	m_rotationFrame->setObjectName("propValueFrame");

	m_rotEdit = new QDoubleSpinBox;
	m_rotEdit->setDecimals(1);
	m_rotEdit->setRange(-360, 360);
	m_rotEdit->setKeyboardTracking(false);
	m_rotEdit->setObjectName("infoViewDoubleSpinBox");
	m_rotEdit->setMaximumWidth(MaxSpinBoxWidth);
	m_rotEdit->setMinimumWidth(MaxSpinBoxWidth);
	m_rotEdit->setLocale(QLocale::C);
	rotationLayout->addWidget(m_rotEdit);

	rotationLayout->addSpacing(AfterSpinBoxWidth);

	auto * label = new QLabel(tr("degrees"), this);
	label->setObjectName("infoViewSpinBoxLabel");
	rotationLayout->addWidget(label);

	rotationLayout->addSpacerItem(new QSpacerItem(1,1, QSizePolicy::Expanding));

	m_rotationFrame->setLayout(rotationLayout);

	connect(m_rotEdit, SIGNAL(valueChanged(double)), this, SLOT(rotEntry()));
}

void HtmlInfoView::unitsClicked() {
	QString units = m_unitsLabel->text();

	QString xs = m_xEdit->text();
	QString ys = m_yEdit->text();
	double x = TextUtils::convertToInches(xs + units);
	double y = TextUtils::convertToInches(ys + units);

	double dpi = 1;
	double step = 1;
	if (units.compare("mm") == 0) {
		units = "px";
		dpi = 90;
		step = 1;
	}
	else if (units.compare("px") == 0) {
		units = "in";
		dpi = 1;
		step = 0.1;
	}
	else if (units.compare("in") == 0) {
		units = "mm";
		dpi = 25.4;
		step = 1;
	}
	else {
		units = "in";
		dpi = 1;
		step = 0.1;
	}
	m_unitsLabel->setText(units);

	m_xEdit->blockSignals(true);
	m_yEdit->blockSignals(true);
	m_xEdit->setSingleStep(step);
	m_yEdit->setSingleStep(step);
	if (!xs.isEmpty()) m_xEdit->setValue(x * dpi);
	if (!ys.isEmpty()) m_yEdit->setValue(y * dpi);
	m_xEdit->blockSignals(false);
	m_yEdit->blockSignals(false);
}

void HtmlInfoView::setLocation(ItemBase * itemBase) {
	if (itemBase == nullptr) {
		m_xEdit->blockSignals(true);
		m_yEdit->blockSignals(true);
		m_xEdit->setEnabled(false);
		m_yEdit->setEnabled(false);
		m_xEdit->setValue(0);
		m_yEdit->setValue(0);
		m_xEdit->blockSignals(false);
		m_yEdit->blockSignals(false);
		return;
	}

	m_xEdit->setEnabled(!itemBase->moveLock());
	m_yEdit->setEnabled(!itemBase->moveLock());

	QString units = m_unitsLabel->text();
	QPointF loc = itemBase->pos();
	if (units == "px") {
	}
	else if (units == "in") {
		loc /= 90;
	}
	else if (units == "mm") {
		loc /= (90 / 25.4);
	}

	m_xEdit->blockSignals(true);
	m_yEdit->blockSignals(true);
	m_xEdit->setValue(loc.x());
	m_yEdit->setValue(loc.y());
	m_xEdit->blockSignals(false);
	m_yEdit->blockSignals(false);
}

void HtmlInfoView::setRotation(ItemBase * itemBase) {
	if (itemBase == nullptr) {
		m_rotEdit->blockSignals(true);
		m_rotEdit->setEnabled(false);
		m_rotEdit->setValue(0);
		m_rotEdit->blockSignals(false);
		return;
	}

	double step = 90;
	bool enabled = true;
	if (itemBase->moveLock()) enabled = false;
	else if (!itemBase->rotationAllowed()) enabled = false;
	else {
		if (itemBase->rotation45Allowed()) step = 45;
		if (itemBase->freeRotationAllowed()) step = 1;
	}

	m_rotEdit->setEnabled(enabled);
	m_rotEdit->setSingleStep(step);

	QTransform transform = itemBase->transform();
	double angle = atan2(transform.m12(), transform.m11()) * 180 / M_PI;

	m_rotEdit->blockSignals(true);
	m_rotEdit->setValue(angle);
	m_rotEdit->blockSignals(false);
}

void HtmlInfoView::updateLocation(ItemBase * itemBase) {
	if (itemBase == nullptr) return;
	if (itemBase != m_lastItemBase) return;

	setLocation(itemBase);
}

void HtmlInfoView::updateRotation(ItemBase * itemBase) {
	if (itemBase == nullptr) return;
	if (itemBase != m_lastItemBase) return;

	setRotation(itemBase);

	// Some property widgets show transform-dependent values (e.g. a net label's actual
	// text alignment); rebuild the inspector so they reflect the new transform.
	if (itemBase->inspectorRefreshOnTransform()) {
		reloadContent(InfoGraphicsView::getInfoGraphicsView(itemBase));
	}
}

void HtmlInfoView::xyEntry() {
	InfoGraphicsView * infoGraphicsView = InfoGraphicsView::getInfoGraphicsView(m_lastItemBase);
	if (infoGraphicsView == nullptr) return;

	DebugDialog::debug(QString("xedit %1 %2 %3").arg(m_xEdit->text()).arg(m_yEdit->text()).arg(static_cast<int>(sender() == m_xEdit)));
	double x = TextUtils::convertToInches(m_xEdit->text() + m_unitsLabel->text());
	double y = TextUtils::convertToInches(m_yEdit->text() + m_unitsLabel->text());
	if ((infoGraphicsView != nullptr) && (m_lastItemBase != nullptr)) {
		infoGraphicsView->moveItem(m_lastItemBase, x * 90, y * 90);
	}
}

void HtmlInfoView::rotEntry() {
	if (m_lastItemBase != nullptr) {
		InfoGraphicsView * infoGraphicsView = InfoGraphicsView::getInfoGraphicsView(m_lastItemBase);
		if (infoGraphicsView == nullptr) return;

		double newAngle = m_rotEdit->value();

		if (m_rotEdit->singleStep() == 1) {
		}
		else {
			newAngle = qFloor(newAngle / m_rotEdit->singleStep()) * m_rotEdit->singleStep();
		}

		QTransform transform = m_lastItemBase->transform();
		double angle = atan2(transform.m12(), transform.m11()) * 180 / M_PI;
		infoGraphicsView->rotateX(newAngle - angle, false, m_lastItemBase);
	}
}
