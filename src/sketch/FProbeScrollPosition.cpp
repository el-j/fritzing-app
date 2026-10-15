/*******************************************************************

Part of the Fritzing project - http://fritzing.org
Copyright (c) 2025 Fritzing GmbH

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

#include "FProbeScrollPosition.h"

#include "../debugdialog.h"

#include <QApplication>
#include <QScrollBar>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

FProbeScrollPosition::FProbeScrollPosition(SketchWidget * sketchWidget, const QString& viewSuffix) :
	FProbe(viewSuffix.isEmpty() ? "ScrollPosition" : ("ScrollPosition_" + viewSuffix).toStdString()),
	m_sketchWidget(sketchWidget) {
	QString probeName = viewSuffix.isEmpty() ? "ScrollPosition" : ("ScrollPosition_" + viewSuffix);
	DebugDialog::debug(QString("FProbeScrollPosition registered: %1").arg(probeName));
}

FProbeScrollPosition::~FProbeScrollPosition() {
}

QVariant FProbeScrollPosition::read() {
	if (!m_sketchWidget) {
		DebugDialog::debug("ScrollPosition: No sketch widget");
		QJsonObject error;
		error["error"] = "No sketch widget";
		return QVariant(QString(QJsonDocument(error).toJson(QJsonDocument::Compact)));
	}

	QJsonObject result;

	// Check if this view is currently active (visible)
	if (!m_sketchWidget->isVisible()) {
		QString viewType;
		switch (m_sketchWidget->viewID()) {
		case ViewLayer::BreadboardView: viewType = "breadboard"; break;
		case ViewLayer::SchematicView: viewType = "schematic"; break;
		case ViewLayer::PCBView: viewType = "pcb"; break;
		default: viewType = "unknown"; break;
		}
		result["error"] = QString("View '%1' is not active").arg(viewType);
		result["viewType"] = viewType;
		DebugDialog::debug(QString("ScrollPosition: %1").arg(result["error"].toString()));
		return QVariant(QString(QJsonDocument(result).toJson(QJsonDocument::Compact)));
	}

	// Get scrollbar values
	QScrollBar* hBar = m_sketchWidget->horizontalScrollBar();
	QScrollBar* vBar = m_sketchWidget->verticalScrollBar();

	if (hBar) {
		result["hValue"] = hBar->value();
		result["hMin"] = hBar->minimum();
		result["hMax"] = hBar->maximum();
		result["hPageStep"] = hBar->pageStep();
		result["hVisible"] = hBar->isVisible();
	}

	if (vBar) {
		result["vValue"] = vBar->value();
		result["vMin"] = vBar->minimum();
		result["vMax"] = vBar->maximum();
		result["vPageStep"] = vBar->pageStep();
		result["vVisible"] = vBar->isVisible();
	}

	// Get zoom level
	result["zoom"] = m_sketchWidget->currentZoom();

	// Get view type
	QString viewType;
	switch (m_sketchWidget->viewID()) {
	case ViewLayer::BreadboardView:
		viewType = "breadboard";
		break;
	case ViewLayer::SchematicView:
		viewType = "schematic";
		break;
	case ViewLayer::PCBView:
		viewType = "pcb";
		break;
	default:
		viewType = "unknown";
		break;
	}
	result["viewType"] = viewType;

	// Get scene rect
	if (m_sketchWidget->scene()) {
		QRectF sceneRect = m_sketchWidget->scene()->sceneRect();
		QJsonObject sceneRectObj;
		sceneRectObj["x"] = sceneRect.x();
		sceneRectObj["y"] = sceneRect.y();
		sceneRectObj["width"] = sceneRect.width();
		sceneRectObj["height"] = sceneRect.height();
		result["sceneRect"] = sceneRectObj;
	}

	// Get viewport rect in scene coordinates
	QRectF viewportRect = m_sketchWidget->mapToScene(m_sketchWidget->viewport()->rect()).boundingRect();
	QJsonObject viewportRectObj;
	viewportRectObj["x"] = viewportRect.x();
	viewportRectObj["y"] = viewportRect.y();
	viewportRectObj["width"] = viewportRect.width();
	viewportRectObj["height"] = viewportRect.height();
	result["viewportRect"] = viewportRectObj;

	// Get transform matrix diagonal (scale factors)
	QTransform transform = m_sketchWidget->transform();
	result["scaleX"] = transform.m11();
	result["scaleY"] = transform.m22();

	// Serialize to JSON string
	QJsonDocument doc(result);
	QString jsonString = QString(doc.toJson(QJsonDocument::Compact));

	DebugDialog::debug(QString("ScrollPosition: %1").arg(jsonString));

	return QVariant(jsonString);
}

void FProbeScrollPosition::write(QVariant value) {
	if (!m_sketchWidget) {
		DebugDialog::debug("ScrollPosition write: No sketch widget");
		return;
	}

	QString jsonString = value.toString();
	DebugDialog::debug(QString("ScrollPosition write: %1").arg(jsonString));

	QJsonParseError parseError;
	QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8(), &parseError);
	if (parseError.error != QJsonParseError::NoError) {
		DebugDialog::debug(QString("ScrollPosition write: JSON parse error: %1").arg(parseError.errorString()));
		return;
	}

	QJsonObject obj = doc.object();

	// Set zoom first (if provided), as it affects scroll ranges
	if (obj.contains("zoom")) {
		double zoom = obj["zoom"].toDouble();
		DebugDialog::debug(QString("ScrollPosition write: Setting zoom to %1").arg(zoom));
		m_sketchWidget->absoluteZoom(zoom);
		// Process events to allow layout updates after zoom change
		QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
	}

	// Set scroll positions (if provided)
	QScrollBar* hBar = m_sketchWidget->horizontalScrollBar();
	QScrollBar* vBar = m_sketchWidget->verticalScrollBar();

	if (obj.contains("hValue") && hBar) {
		int hValue = obj["hValue"].toInt();
		DebugDialog::debug(QString("ScrollPosition write: Setting hValue to %1").arg(hValue));
		hBar->setValue(hValue);
	}

	if (obj.contains("vValue") && vBar) {
		int vValue = obj["vValue"].toInt();
		DebugDialog::debug(QString("ScrollPosition write: Setting vValue to %1").arg(vValue));
		vBar->setValue(vValue);
	}

	// Final event flush to ensure positions are applied
	QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

	DebugDialog::debug("ScrollPosition write: Complete");
}
