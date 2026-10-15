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

#ifndef FPROBEWIRE_H
#define FPROBEWIRE_H

#include "FProbe.h"

#include <QObject>
#include <QVariant>
#include <QJsonObject>

class MainWindow;
class SketchWidget;
class ItemBase;
class ConnectorItem;
class Wire;

class FProbeWire : public QObject, public FProbe {
	Q_OBJECT
public:
	FProbeWire(MainWindow *mainWindow);
	~FProbeWire() {}

	QVariant read() override;
	void write(QVariant var) override;

Q_SIGNALS:
	void requestOperation(const QString &jsonCommand);

private:
	SketchWidget * currentView();
	ItemBase * findPartByTitle(SketchWidget *view, const QString &title);
	ConnectorItem * findConnector(ItemBase *part, const QString &connectorId);

	QJsonObject handleGetWires(SketchWidget *view, const QJsonObject &params);
	QJsonObject handleGetConnections(SketchWidget *view, const QJsonObject &params);
	QJsonObject handleGetWireInfo(SketchWidget *view, const QJsonObject &params);
	QJsonObject handleGetWirePos(SketchWidget *view, const QJsonObject &params, bool start);
	QJsonObject handleGetConnectorScenePos(SketchWidget *view, const QJsonObject &params);
	QJsonObject handleSceneToScreen(SketchWidget *view, const QJsonObject &params);
	QJsonObject handleIsBigDot(SketchWidget *view, const QJsonObject &params, bool start);
	QJsonObject handleMoveWireEndpoint(SketchWidget *view, const QJsonObject &params, bool start);
	QJsonObject handleMoveWireEndpointRelative(SketchWidget *view, const QJsonObject &params, bool start);
	QJsonObject handleMoveWireNear(SketchWidget *view, const QJsonObject &params, bool start);
	QJsonObject handleDeleteWire(SketchWidget *view, const QJsonObject &params);
	QJsonObject handleDeleteUpToBendpoint(SketchWidget *view, const QJsonObject &params);

	MainWindow *m_mainWindow;
	QVariant m_lastResult;
};

#endif // FPROBEWIRE_H
