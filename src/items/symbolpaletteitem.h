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


#ifndef SYMBOLPALETTEITEM_H
#define SYMBOLPALETTEITEM_H

#include "paletteitem.h"

#include <QPixmap>
#include <QSize>

/*
#include <QTime>

class FocusBugLineEdit : public QLineEdit {
    Q_OBJECT

public:
	FocusBugLineEdit(QWidget * parent = NULL);
	~FocusBugLineEdit();

signals:
	void safeEditingFinished();

protected slots:
	void editingFinishedSlot();

protected:
	QTime m_lastEditingFinishedEmit;

};
*/

class SymbolPaletteItem : public PaletteItem
{
	Q_OBJECT

public:
	explicit SymbolPaletteItem(ModelPart *, ViewLayer::ViewID, const ViewGeometry & viewGeometry, long id, QMenu * itemMenu, bool doLabel);
	~SymbolPaletteItem();

	ConnectorItem* newConnectorItem(class Connector *connector);
	bool busConnectorItems(ConnectorItem *, QList<ConnectorItem *> & items);
	double voltage();
	void setProp(const QString & prop, const QString & value);
	void setVoltage(double);
	QString retrieveSvg(ViewLayer::ViewLayerID, QHash<QString, QString> & svgHash, bool blackOnly, double dpi, double & factor);
	bool collectExtraInfo(QWidget * parent, const QString & family, const QString & prop, const QString & value, bool swappingEnabled, QString & returnProp, QString & returnValue, QWidget * & returnWidget, bool & hide);
	QString getProperty(const QString & key);
	ConnectorItem * connector0();
	ConnectorItem * connector1();
	PluralType isPlural();
	void addedToScene(bool temporary);
	bool hasPartNumberProperty();
	virtual bool isOnlyNetLabel();
	bool inspectorRefreshOnTransform();
	bool hasPartLabel();
	bool getAutoroutable();
	void setAutoroutable(bool);
	void setLabel(const QString &);
	QString getLabel();
	QString getDirection();
	void setStyle(const QString &);
	QString effectiveAlign();
	void refreshNetLabelStyleFromDefault();

public:
	static double DefaultVoltage;
	static QString defaultNetLabelStyle();
	static void refreshDefaultNetLabelStyle();
	// Map between the orientation-independent policy ("outside"/"connector") and the stored
	// local alignment ("left"/"right"), given the label's arrow side (goLeft).
	static QString alignForPolicy(const QString & policy, bool goLeft);
	static QString policyForAlign(const QString & align, bool goLeft);
	// Resolve a net label's target for an Inspector style choice ("left"/"right"/"legacy"
	// single, "outside"/"connector" group): returns the moduleID to swap to (empty => no
	// swap), and sets newStyle to the alignment local-prop to apply. Used by MainWindow.
	static QString resolveStyleSwap(ItemBase * item, const QString & picked, QString & newStyle);

public Q_SLOTS:
	void voltageEntry(int index);
	void labelEntry();
	void styleEntry(int index);
	void groupStyleEntry(int index);
	void swapEntry(int index);

protected:
	void removeMeFromBus(double voltage);
	double useVoltage(ConnectorItem * connectorItem);
	virtual QString makeSvg(ViewLayer::ViewLayerID);
	QString replaceTextElement(QString svg);
	ViewLayer::ViewID useViewIDForPixmap(ViewLayer::ViewID, bool swappingEnabled);
	void resetLayerKin();
	// Route a net-label style choice to the swap machinery (see MainWindow::swapSelectedMap).
	void requestNetLabelStyle(const QString & picked);

protected:
	double m_voltage;
	QPointer<ConnectorItem> m_connector0;
	QPointer<ConnectorItem> m_connector1;
	bool m_voltageReference;
	bool m_isNetLabel;
	QString m_label;
};


class NetLabel : public SymbolPaletteItem
{
	Q_OBJECT

public:
	NetLabel(ModelPart *, ViewLayer::ViewID, const ViewGeometry & viewGeometry, long id, QMenu * itemMenu, bool doLabel);
	~NetLabel();

	void addedToScene(bool temporary);
	QString retrieveSvg(ViewLayer::ViewLayerID, QHash<QString, QString> & svgHash, bool blackOnly, double dpi, double & factor);
	PluralType isPlural();
	bool isOnlyNetLabel();
	QString getInspectorTitle();
	void setInspectorTitle(const QString & oldText, const QString & newText);
	QString getVersion(); // Only used in NetLabel currenty, but a candiate for PartBase or similar, to support future migrations

	// Renders a small preview of a net label for the given orientation-independent policy
	// ("outside"/"connector") and orientation (goLeft), used by the Schematic preferences.
	static QPixmap stylePreviewPixmap(const QString & policy, bool goLeft, const QSize & size);

protected:
	QString makeSvg(ViewLayer::ViewLayerID);

};

#endif
