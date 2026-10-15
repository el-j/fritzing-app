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

#include "FProbeCurrentSketchXml.h"

#include "fapplication.h"
#include "mainwindow.h"
#include "model/sketchmodel.h"

#include <QByteArray>
#include <QString>
#include <QXmlStreamWriter>

FProbeCurrentSketchXml::FProbeCurrentSketchXml() : FProbe("CurrentSketchXml") {
}

FProbeCurrentSketchXml::~FProbeCurrentSketchXml() {
}

QVariant FProbeCurrentSketchXml::read() {
	// Resolve the current sketch window at read time instead of binding to one
	// SketchModel at construction. Probes are leaked and registered by name
	// (last window wins); after a failed Revert the replacement window is
	// destroyed while its probe stays in the map, so reading a captured model
	// was a use-after-free (issue #1158). Resolving the current window each read
	// targets a live window for the harness's normal (main-thread-idle) reads.
	MainWindow * current = static_cast<FApplication *>(qApp)->currentMainWindow();
	if (current == nullptr) {
		return QVariant("Error: no current sketch window");
	}

	SketchModel * sketchModel = current->sketchModel();
	if (sketchModel == nullptr) {
		return QVariant("Error: SketchModel is null");
	}

	try {
		// Create a QByteArray to capture the XML output
		QByteArray xmlData;
		QXmlStreamWriter streamWriter(&xmlData);

		// Use the same method as ModelBase::save() to generate XML
		sketchModel->save("", streamWriter, false);

		// Convert to string and return
		QString xmlString = QString::fromUtf8(xmlData);
		return QVariant(xmlString);

	} catch (...) {
		return QVariant("Error: Failed to generate sketch XML");
	}
}