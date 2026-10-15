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

#ifndef FPROBESELECTEDPARTLABEL_H
#define FPROBESELECTEDPARTLABEL_H

#include "FProbe.h"

#include <QObject>
#include <QVariant>
#include <QJsonObject>

class MainWindow;
class SketchWidget;
class ItemBase;
class PartLabel;

// Reports geometry of the selected part's label in the current view and
// triggers the label's context menu rotate/flip actions by name.
// The label is looked up on the fly, so the probe has no cost when unused.
class FProbeSelectedPartLabel : public QObject, public FProbe {
	Q_OBJECT
public:
	FProbeSelectedPartLabel(MainWindow *mainWindow);
	~FProbeSelectedPartLabel() {}

	QVariant read() override;
	void write(QVariant var) override;

Q_SIGNALS:
	void requestInfo();
	void requestAction(const QString &actionName);

private:
	PartLabel * findSelectedPartLabel(SketchWidget * & view, QJsonObject &result);

	MainWindow *m_mainWindow;
	QVariant m_lastResult;
};

#endif // FPROBESELECTEDPARTLABEL_H
