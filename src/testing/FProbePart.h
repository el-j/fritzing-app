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

#ifndef FPROBEPART_H
#define FPROBEPART_H

#include "FProbe.h"

#include <QObject>
#include <QVariant>
#include <QJsonObject>

class MainWindow;
class SketchWidget;
class ItemBase;

class FProbePart : public QObject, public FProbe {
	Q_OBJECT
public:
	FProbePart(MainWindow *mainWindow);
	~FProbePart() {}

	QVariant read() override;
	void write(QVariant var) override;

Q_SIGNALS:
	void requestOperation(const QString &jsonCommand);

private:
	SketchWidget * currentView();
	ItemBase * findPartByTitle(SketchWidget *view, const QString &title);
	ItemBase * resolvePart(SketchWidget *view, const QJsonObject &params);

	QJsonObject handleGetPosition(SketchWidget *view, const QJsonObject &params);
	QJsonObject handleMovePart(SketchWidget *view, const QJsonObject &params);
	QJsonObject handleMovePartRelative(SketchWidget *view, const QJsonObject &params);
	QJsonObject handleGetSize(SketchWidget *view, const QJsonObject &params);
	QJsonObject handleGetResizeHandlePos(SketchWidget *view, const QJsonObject &params);
	QJsonObject handleSceneToScreen(SketchWidget *view, const QJsonObject &params);
	QJsonObject handleGetGridSize(SketchWidget *view, const QJsonObject &params);

	MainWindow *m_mainWindow;
	QVariant m_lastResult;
};

#endif // FPROBEPART_H
