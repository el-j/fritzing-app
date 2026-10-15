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

#ifndef FPROBEBUGANNOTATIONS_H
#define FPROBEBUGANNOTATIONS_H

#include "FProbe.h"

#include <QObject>
#include <QVariant>

class MainWindow;

// Reports the bug annotations (the red error markers ItemBase::hasBug() drives) currently shown
// across all views, so tests can detect stray/duplicated annotations -- e.g. an artifact left
// behind by a part swap-back.
//   read()  -> JSON: { "count": N, "items": [ {id, title, module, view}, ... ] }
//   write() -> action "click:<instanceTitle>" simulates clicking that part's obsolete badge
//             (the shortcut to the Part Migration dialog); result JSON: { ok, [reason] }.
class FProbeBugAnnotations : public QObject, public FProbe {
	Q_OBJECT
public:
	FProbeBugAnnotations(MainWindow *mainWindow);
	~FProbeBugAnnotations() {}

	QVariant read() override;
	void write(QVariant var) override;

Q_SIGNALS:
	void requestInfo();
	void requestAction(const QString &action);

private:
	MainWindow *m_mainWindow;
	QVariant m_lastResult;
};

#endif // FPROBEBUGANNOTATIONS_H
