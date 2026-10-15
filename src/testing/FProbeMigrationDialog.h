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

#ifndef FPROBEMIGRATIONDIALOG_H
#define FPROBEMIGRATIONDIALOG_H

#include "FProbe.h"

#include <QObject>
#include <QVariant>

class MainWindow;

// Drives the "Part Migration" dialog for GUI tests. The dialog and its controls are found by
// object name (partMigrationDialog, migrationOldRadio, migrationKeepRadio, migrationNewRadio,
// migrationDoneButton, migrationUpdateAllButton, migrationPrevButton, migrationNextButton, plus the
// labels), so the probe needs no access to MigrationHandler internals.
//   read()  -> JSON describing the current dialog state (open, selected version, labels, buttons).
//              "selected" is "old" | "keep" | "new"; "keepVisible" (alias "silenceVisible") reports
//              the persistent "keep and don't ask again" radio, offered only for "optional" parts.
//   write() -> an action string: "new", "old", "keep" (alias "silence"), "updateAll", "next",
//              "previous", "done", "close", "activate" (focus the dialog), or "undo"/"redo"
//              (trigger the app's Undo/Redo while the dialog is open -- a focus-independent
//              stand-in for Ctrl+Z/Ctrl+Y that exercises the dialog's reconcile-on-undo).
class FProbeMigrationDialog : public QObject, public FProbe {
	Q_OBJECT
public:
	FProbeMigrationDialog(MainWindow *mainWindow);
	~FProbeMigrationDialog() {}

	QVariant read() override;
	void write(QVariant var) override;

Q_SIGNALS:
	void requestInfo();
	void requestAction(const QString &action);

private:
	MainWindow *m_mainWindow;
	QVariant m_lastResult;
};

#endif // FPROBEMIGRATIONDIALOG_H
