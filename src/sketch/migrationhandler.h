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

#ifndef MIGRATIONHANDLER_H
#define MIGRATIONHANDLER_H

#include <QObject>
#include <QList>
#include <QString>
#include <QDate>
#include <QPointer>

#include "../model/modelpartshared.h"

class ItemBase;
class ModelPart;
class SketchWidget;
class MainWindow;
class QUndoCommand;
class QDialog;
class QLabel;
class QPushButton;
class QRadioButton;
class QButtonGroup;
class QAbstractButton;

struct MigrationInfo {
	qint64 itemId;          // current item ID (changes after swap)
	qint64 originalItemId;  // original item ID (for restoring after undo)
	qint64 newItemId = 0;   // most recent swapped-in (new) item ID; lets the dialog re-find the part
	                        // after a manual undo/redo flips it old<->new behind the handler's back
	QString oldModuleID;    // module ID to swap back to old
	QString newModuleID;    // module ID to swap to new
	QString instanceTitle;  // for display purposes
	QString oldTitle;       // old part's title (for the version chooser)
	QString oldVersion;     // old part's version
	QString newTitle;       // new part's title
	QString newVersion;     // new part's version
	bool hasFurtherRevision = false; // the new target is itself replaced by a later revision (chain)
	QList<HistoryEntry> relevantHistory;
	QString effectiveMode;  // "required", "recommended", or "optional"
	bool isSwapped;         // currently showing new part?
	QString reason;         // why the dialog is shown (e.g. mixed versions)
	bool decided = false;   // user has explicitly chosen for this part
	bool silenced = false;  // user chose "keep and don't ask again"
	int swapStackIndex = -1; // undo-stack index right after this part's preview swap (-1 = none)
};

class MigrationHandler : public QObject
{
	Q_OBJECT

public:
	explicit MigrationHandler(SketchWidget* sketchWidget, QObject* parent = nullptr);

	// Main interface
	void queueMigration(ItemBase* itemBase, ModelPart* oldPart,
	                    ModelPart* newPart, const QList<HistoryEntry>& history,
	                    const QString& reason = QString(), bool hasFurtherRevision = false);
	void processMigrations();
	bool hasPendingMigrations() const;
	void clearPendingMigrations();

	// Get relevant history entries based on date filtering
	static QList<HistoryEntry> getRelevantHistory(ModelPart* instancePart,
	                                              const QList<HistoryEntry>& allHistory);
	static QString computeEffectiveMode(const QList<HistoryEntry>& history);

	// Port the special-cased properties an obsolete part needs carried onto its replacement
	// (resistance, LED colour). Migration-specific knowledge, shared by the direct swap and the
	// Part Migration dialog; static and view-parameterised so MainWindow's swap helpers can call it
	// without this handler holding extra state.
	static void portObsoleteSpecialProps(SketchWidget* view, ItemBase* oldItem,
	                                      ModelPart* newModelPart, long newID,
	                                      QUndoCommand* parentCommand);

	// Get current item for a migration (may change after swaps)
	ItemBase* currentItem(int index) const;
	ItemBase* currentItem() const;  // for current migration

private Q_SLOTS:
	void handlePendingMigrationDialog();
	void onDialogClosed();
	void onVersionChosen(QAbstractButton* button);
	void onUndoStackChanged();

private:
	void createMigrationDialog();
	void updateDialogForCurrentMigration();
	void centerAndZoomOnItem(ItemBase* item);

	void swapToNew(MigrationInfo& info);
	void revertToOld(MigrationInfo& info);
	bool canUndoOwnSwap(int swapStackIndex) const;
	// Bring the tracked swap/silence state back in line with the part actually present in the
	// sketch (it can change behind our back via a manual Undo/Redo), so the radios reflect reality.
	void reconcileStateFromSketch(MigrationInfo& info);
	bool isSilenceActive(ItemBase* item, const MigrationInfo& info) const;
	// Index of the pending migration for a part (by its cross-view chief id, across swaps), or -1.
	int indexForItem(qint64 chiefId) const;
	void applySilence(MigrationInfo& info);
	void clearSilence(MigrationInfo& info);
	void refreshObsoleteAnnotation(const MigrationInfo& info);
	void updateAllMigrations();
	void goToPreviousMigration();
	void goToNextMigration();
	void processNextMigration();
	void closeDialog();

	MainWindow* findMainWindow() const;

	SketchWidget* m_sketchWidget;
	QList<MigrationInfo> m_pendingMigrations;
	int m_currentIndex;
	// Part size (max bound dimension) at which the view was last zoomed; -1 until the dialog opens.
	// Drives "zoom once, then only re-zoom when a part is >=2x / <=0.5x that size".
	qreal m_focusPartSize = -1.0;
	// Queue position the view is currently focused on; -1 until the dialog opens. The view is left
	// untouched while this stays the same (i.e. when only the old/new version of a part changes).
	int m_focusIndex = -1;
	// True while the handler is applying its own swap/silence (which push undo commands), so the
	// undo-stack listener ignores those self-inflicted changes and only reacts to external Undo/Redo.
	bool m_applyingChange = false;
	// Chief id of the part the most recent queueMigration() was about; lets an already-open dialog
	// jump straight to (focus) that part instead of rebuilding the session.
	qint64 m_pendingFocusId = 0;

	// Dialog widgets
	QPointer<QDialog> m_dialog;
	QLabel* m_counterLabel;
	QLabel* m_titleLabel;
	QLabel* m_reasonLabel;
	QLabel* m_historyLabel;
	QButtonGroup* m_versionGroup;
	QRadioButton* m_oldRadio;
	QRadioButton* m_keepRadio;
	QRadioButton* m_newRadio;
	QLabel* m_keepNote;
	QPushButton* m_doneButton;
	QPushButton* m_updateAllButton;
	QPushButton* m_prevButton;
	QPushButton* m_nextButton;
};

#endif // MIGRATIONHANDLER_H
