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

#include "migrationhandler.h"
#include "sketchwidget.h"
#include "../items/itembase.h"
#include "../items/moduleidnames.h"
#include "../model/modelpart.h"
#include "../mainwindow/mainwindow.h"
#include "../debugdialog.h"
#include "../waitpushundostack.h"
#include "../commands.h"
#include "../viewlayer.h"

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QButtonGroup>
#include <QAbstractButton>
#include <QGroupBox>
#include <QSignalBlocker>
#include <QTimer>
#include <QGraphicsScene>
#include <QFont>
#include <QPalette>
#include <QScrollArea>
#include <QAction>
#include <QUndoStack>

#include <algorithm>

MigrationHandler::MigrationHandler(SketchWidget* sketchWidget, QObject* parent)
	: QObject(parent)
	, m_sketchWidget(sketchWidget)
	, m_currentIndex(0)
	, m_dialog(nullptr)
	, m_counterLabel(nullptr)
	, m_titleLabel(nullptr)
	, m_reasonLabel(nullptr)
	, m_historyLabel(nullptr)
	, m_versionGroup(nullptr)
	, m_oldRadio(nullptr)
	, m_keepRadio(nullptr)
	, m_newRadio(nullptr)
	, m_keepNote(nullptr)
	, m_doneButton(nullptr)
	, m_updateAllButton(nullptr)
	, m_prevButton(nullptr)
	, m_nextButton(nullptr)
{
}

void MigrationHandler::queueMigration(ItemBase* itemBase, ModelPart* oldPart,
                                      ModelPart* newPart, const QList<HistoryEntry>& history,
                                      const QString& reason, bool hasFurtherRevision)
{
	// Identify the part by its cross-view chief id (shared across views; tracked across swaps).
	qint64 chiefId = itemBase->layerKinChief()->id();

	// Remember which part this request is about, so an already-open dialog can focus it.
	m_pendingFocusId = chiefId;

	// Keep the set unique: a part already queued must not be added a second time -- e.g. the user
	// clicked its badge again, or the same logical part was collected from several views.
	if (indexForItem(chiefId) >= 0) return;

	MigrationInfo info;
	info.itemId = chiefId;
	info.originalItemId = chiefId;
	info.oldModuleID = oldPart->moduleID();
	info.newModuleID = newPart->moduleID();
	info.instanceTitle = itemBase->instanceTitle();
	info.oldTitle = oldPart->title();
	info.oldVersion = oldPart->version();
	if (newPart != nullptr) {
		info.newTitle = newPart->title();
		info.newVersion = newPart->version();
	}
	info.relevantHistory = history;
	// Classify from the full history (the part's nature), not just the unseen subset:
	// an all-"required" part with no unseen entries must still resolve to "required", and a
	// history-less part to "recommended".
	info.effectiveMode = computeEffectiveMode(newPart != nullptr ? newPart->history() : history);
	info.isSwapped = false;
	info.reason = reason;
	info.hasFurtherRevision = hasFurtherRevision;

	m_pendingMigrations.append(info);
}

ItemBase* MigrationHandler::currentItem(int index) const
{
	if (index < 0 || index >= m_pendingMigrations.size()) return nullptr;
	// The part may live in any view (it isn't necessarily present in PCB), so
	// resolve it across all views by its shared cross-view id.
	MainWindow* mainWindow = findMainWindow();
	if (mainWindow == nullptr) return nullptr;
	return mainWindow->findItemInAnyView(m_pendingMigrations[index].itemId);
}

ItemBase* MigrationHandler::currentItem() const
{
	return currentItem(m_currentIndex);
}

void MigrationHandler::processMigrations()
{
	if (m_pendingMigrations.isEmpty()) return;

	// A dialog is already showing: queueMigration() just appended (and deduped) parts into the live
	// session. Don't rebuild/reorder/reopen it -- simply focus the part the request was about (the
	// most recently queued one, whether newly added or already present). A part reached here mid-
	// session via a badge/Inspector click, which only fires on a still-obsolete (non-silent) part,
	// so there is nothing to auto-apply. A direct jump to any index is safe: each part's swap state
	// is independent and navigation never reverts.
	if (!m_dialog.isNull()) {
		int idx = indexForItem(m_pendingFocusId);
		if (idx >= 0) {
			m_currentIndex = idx;
			updateDialogForCurrentMigration();
		}
		m_dialog->raise();
		m_dialog->activateWindow();
		return;
	}

	// Collect "required" migrations to auto-apply now; keep the rest for the dialog.
	QList<MigrationInfo> nonSilentMigrations;
	QList<ItemBase*> silentItems;
	for (const MigrationInfo& info : m_pendingMigrations) {
		if (info.effectiveMode == "required") {
			ItemBase* item = m_sketchWidget->findItem(info.itemId);
			if (item != nullptr) {
				silentItems.append(item);
			}
		} else {
			nonSilentMigrations.append(info);
		}
	}

	// Auto-apply all silent migrations as a single undoable command, with no dialog and no
	// feedback popup. Use swapObsoleteDirect() (not swapObsolete()) so we don't re-enter
	// routeHistoryMigrations and re-queue these same items. It ports the special-cased
	// properties (resistance, LED colour), reusing the legacy obsolete-part flow.
	if (!silentItems.isEmpty()) {
		MainWindow* mainWindow = findMainWindow();
		if (mainWindow != nullptr) {
			mainWindow->swapObsoleteDirect(silentItems, false);
			// Silent swaps change the sketch without a dialog; tell the user non-modally.
			// (The new part's history remains visible in the Inspector "Revisions" section.)
			mainWindow->statusMessage(
			    tr("%n part(s) were automatically updated to a newer version", "", silentItems.count()),
			    5000);
		} else {
			DebugDialog::debug("MigrationHandler: Could not find MainWindow for silent migration");
		}
	}

	m_pendingMigrations = nonSilentMigrations;

	// Present the dialog in a deterministic order: group by part type (the obsolete module id),
	// then by instance title within each group. The collection order is otherwise undefined
	// (it follows scene/view iteration), which made navigation and tests order-dependent.
	std::sort(m_pendingMigrations.begin(), m_pendingMigrations.end(),
	          [](const MigrationInfo& a, const MigrationInfo& b) {
		if (a.oldModuleID != b.oldModuleID) return a.oldModuleID < b.oldModuleID;
		return a.instanceTitle < b.instanceTitle;
	});

	if (!m_pendingMigrations.isEmpty()) {
		m_currentIndex = 0;
		QTimer::singleShot(100, this, &MigrationHandler::handlePendingMigrationDialog);
	}
}

bool MigrationHandler::hasPendingMigrations() const
{
	return !m_pendingMigrations.isEmpty();
}

void MigrationHandler::clearPendingMigrations()
{
	m_pendingMigrations.clear();
}

QList<HistoryEntry> MigrationHandler::getRelevantHistory(ModelPart* instancePart,
                                                         const QList<HistoryEntry>& allHistory)
{
	// Baseline = the part's stored silence date if present, else the part's own date. Guard the
	// parse: an unparseable silencedDate must fall back to the part date, not leave the baseline
	// invalid (which would make every entry look unseen and re-prompt forever).
	QString silencedDateStr = instancePart->localProp("silencedDate").toString();
	QDate baselineDate;
	if (!silencedDateStr.isEmpty()) {
		baselineDate = QDate::fromString(silencedDateStr, Qt::ISODate);
	}
	if (!baselineDate.isValid()) {
		baselineDate = instancePart->date();
	}

	QList<HistoryEntry> relevant;
	for (const HistoryEntry& entry : allHistory) {
		QDate entryDate = entry.parsedDate();
		if (!baselineDate.isValid() || entryDate > baselineDate) {
			relevant.append(entry);
		}
	}

	return relevant;
}

QString MigrationHandler::computeEffectiveMode(const QList<HistoryEntry>& history)
{
	// No migration notes at all → classic obsoletion: "recommended" (prompt on every load).
	// (Only an explicit mode="required" history entry opts a part into auto-swap.)
	if (history.isEmpty()) return "recommended";

	// Most insistent mode wins: recommended > optional > required. A change that needs review
	// (recommended) must not be auto-applied even if another entry is "required".
	bool hasRecommended = false;
	bool hasOptional = false;

	for (const HistoryEntry& entry : history) {
		if (entry.isRecommended()) hasRecommended = true;
		if (entry.isOptional()) hasOptional = true;
	}

	if (hasRecommended) return "recommended";
	if (hasOptional) return "optional";
	return "required";
}

// Port properties that don't carry over by a module-ID swap alone, reusing the legacy obsolete-part
// special cases (resistance, LED colour). Migration-specific knowledge shared by the direct swap
// and the Part Migration dialog; static and view-parameterised so MainWindow's swap helpers can
// call it without this handler needing any extra state.
void MigrationHandler::portObsoleteSpecialProps(SketchWidget* view, ItemBase* oldItem,
                                                ModelPart* newModelPart, long newID,
                                                QUndoCommand* parentCommand)
{
	if (view == nullptr || oldItem == nullptr || oldItem->modelPart() == nullptr || newModelPart == nullptr) return;

	// Read the OLD part's *instance* values, not the shared part defaults. A user-chosen resistance
	// or LED colour is stored as a local prop on the instance; modelPart()->properties() only holds
	// the part's FZP defaults. Reading the default discarded the user's choice -- e.g. a Yellow LED
	// migrated to the LED part's default Red. ItemBase::getProperty() prefers the instance's local
	// prop and falls back to the default, so an unedited part still ports its default (the obsolete
	// fixed-value resistors carry their resistance this way).

	// special case for swapping old resistors.
	QString resistance = oldItem->getProperty("resistance");
	if (!resistance.isEmpty()) {
		QChar r = resistance.at(resistance.length() - 1);
		ushort ohm = r.unicode();
		if (ohm == 8486) {
			// ends with the ohm symbol
			resistance.chop(1);
		}
	}
	QString footprint = oldItem->getProperty("footprint");
	if (!resistance.isEmpty() && !footprint.isEmpty()) {
		new SetResistanceCommand(view, newID, resistance, resistance, footprint, footprint, parentCommand);
	}

	// special case for swapping LEDs
	if (newModelPart->moduleID().contains(ModuleIDNames::ColorLEDModuleIDName)) {
		QString oldColor = oldItem->getProperty("color");
		QString newColor;
		// The obsolete and replacement LEDs share the same colour menu (both end in
		// ColorLEDModuleID), and modern menu values carry a wavelength/spec in parentheses --
		// "Yellow (592nm)", "Red (633nm)", "White (4500K)". If the instance already holds such a
		// value, preserve it verbatim; collapsing every "yellow" to one hard-coded wavelength changed
		// e.g. Yellow (592nm) -> Yellow (585nm). Only the legacy bare names ("yellow", "red", ...),
		// which are not menu values, still need mapping.
		if (oldColor.contains('(')) {
			newColor = oldColor;
		}
		else if (oldColor.contains("red", Qt::CaseInsensitive)) {
			newColor = "Red (633nm)";
		}
		else if (oldColor.contains("blue", Qt::CaseInsensitive)) {
			newColor = "Blue (430nm)";
		}
		else if (oldColor.contains("yellow", Qt::CaseInsensitive)) {
			newColor = "Yellow (585nm)";
		}
		else if (oldColor.contains("green", Qt::CaseInsensitive)) {
			newColor = "Green (555nm)";
		}
		else if (oldColor.contains("white", Qt::CaseInsensitive)) {
			newColor = "White (4500K)";
		}

		if (newColor.length() > 0) {
			new SetPropCommand(view, newID, "color", newColor, newColor, true, parentCommand);
		}
	}
}

void MigrationHandler::handlePendingMigrationDialog()
{
	if (m_pendingMigrations.isEmpty()) return;

	createMigrationDialog();
	updateDialogForCurrentMigration();

	if (m_dialog) {
		m_dialog->show();
		m_dialog->raise();
		m_dialog->activateWindow();

		// Park the dialog at the top-right of the main window. Navigating the dialog centres and
		// zooms each part in the view, so a centred dialog would sit right on top of that part.
		MainWindow* mainWindow = findMainWindow();
		if (mainWindow != nullptr) {
			const int margin = 16;
			const QRect g = mainWindow->geometry();
			m_dialog->move(g.right() - m_dialog->width() - margin, g.top() + margin);
		}
	}
}

void MigrationHandler::createMigrationDialog()
{
	// Defensive: if a previous dialog somehow lingers, tear it down without triggering our
	// finished handler (which would wipe the fresh migration session we're about to show).
	if (m_dialog) {
		disconnect(m_dialog, nullptr, this, nullptr);
		m_dialog->close();
	}

	// New session: zoom afresh on the first part shown, and treat it as a fresh focus.
	m_focusPartSize = -1.0;
	m_focusIndex = -1;

	// Parent to the MainWindow (not the sketch view): Fritzing sets its stylesheet per-MainWindow
	// rather than app-wide, so this makes the dialog inherit the app styling, and it centres on
	// the main window. Fall back to the sketch widget if the window can't be resolved.
	MainWindow* parentWindow = findMainWindow();
	m_dialog = new QDialog(parentWindow != nullptr ? static_cast<QWidget*>(parentWindow)
	                                               : qobject_cast<QWidget*>(m_sketchWidget));
	m_dialog->setWindowTitle(tr("Part Migration"));
	m_dialog->setObjectName("partMigrationDialog");   // for GUI test probes
	m_dialog->setModal(false);
	m_dialog->resize(500, 400);
	m_dialog->setAttribute(Qt::WA_DeleteOnClose);
	// Closing by any means (ESC, window close, or completion) runs cleanup. m_dialog is a
	// QPointer, so it auto-nulls when the dialog is deleted — no dangling reuse on the next open.
	connect(m_dialog, &QDialog::finished, this, &MigrationHandler::onDialogClosed);

	// Let the sketch's Undo/Redo keyboard shortcuts keep working while this non-modal dialog has
	// focus (so a preview swap can be undone/redone without first clicking back into the main
	// window). The actions live on the MainWindow and act on the shared, active undo stack; adding
	// them to the dialog just extends their WindowShortcut context to it. The same QAction may be
	// attached to several widgets, so this doesn't disturb the main window's own shortcuts.
	if (parentWindow != nullptr) {
		if (QAction* undoAct = parentWindow->undoAction()) m_dialog->addAction(undoAct);
		if (QAction* redoAct = parentWindow->redoAction()) m_dialog->addAction(redoAct);
	}

	// Watch the (shared) undo stack so an external Undo/Redo that flips the current part old<->new is
	// reflected in the radios live. UniqueConnection keeps re-opening the dialog from stacking up
	// duplicate connections; the slot is a no-op while the dialog is closed.
	if (m_sketchWidget != nullptr && m_sketchWidget->undoStack() != nullptr) {
		connect(m_sketchWidget->undoStack(), &QUndoStack::indexChanged,
		        this, &MigrationHandler::onUndoStackChanged, Qt::UniqueConnection);
	}

	QVBoxLayout* layout = new QVBoxLayout(m_dialog);

	// Counter label
	m_counterLabel = new QLabel(m_dialog);
	m_counterLabel->setObjectName("migrationCounterLabel");
	m_counterLabel->setAlignment(Qt::AlignCenter);
	QFont titleFont = m_counterLabel->font();
	titleFont.setBold(true);
	titleFont.setPointSize(titleFont.pointSize() + 2);
	m_counterLabel->setFont(titleFont);
	layout->addWidget(m_counterLabel);

	// Title/part name label
	m_titleLabel = new QLabel(m_dialog);
	m_titleLabel->setObjectName("migrationTitleLabel");
	m_titleLabel->setWordWrap(true);
	m_titleLabel->setAlignment(Qt::AlignTop);
	layout->addWidget(m_titleLabel);

	// Reason / explanation label (why this dialog is being shown)
	m_reasonLabel = new QLabel(m_dialog);
	m_reasonLabel->setObjectName("migrationReasonLabel");
	m_reasonLabel->setWordWrap(true);
	m_reasonLabel->setAlignment(Qt::AlignTop);
	layout->addWidget(m_reasonLabel);

	// History entries (scrollable)
	QScrollArea* scrollArea = new QScrollArea(m_dialog);
	scrollArea->setWidgetResizable(true);
	scrollArea->setMinimumHeight(150);
	m_historyLabel = new QLabel();
	m_historyLabel->setObjectName("migrationHistoryLabel");
	m_historyLabel->setWordWrap(true);
	m_historyLabel->setAlignment(Qt::AlignTop);
	m_historyLabel->setTextFormat(Qt::RichText);
	scrollArea->setWidget(m_historyLabel);
	layout->addWidget(scrollArea);

	layout->addStretch();

	// Version chooser: three radios (old / keep-and-don't-ask / new), each labelled with the part
	// name + version. Picking one swaps the part live; it is preset to the currently active version
	// per part. The middle "keep and don't ask again" radio replaces the former Silence button -- it
	// keeps the old version and persists a silence so this change won't prompt again. Silence is only
	// meaningful for "optional" parts, so that radio (and its note) is hidden for "recommended" parts.
	auto* versionBox = new QGroupBox(tr("Version"), m_dialog);
	auto* versionLayout = new QVBoxLayout(versionBox);
	m_oldRadio = new QRadioButton(versionBox);
	m_keepRadio = new QRadioButton(versionBox);
	m_newRadio = new QRadioButton(versionBox);
	m_oldRadio->setObjectName("migrationOldRadio");
	m_keepRadio->setObjectName("migrationKeepRadio");
	m_newRadio->setObjectName("migrationNewRadio");
	m_keepRadio->setToolTip(tr("Keep the old version and don't ask about these changes again"));
	m_versionGroup = new QButtonGroup(versionBox);   // owned by the dialog, dies with it
	m_versionGroup->setExclusive(true);
	m_versionGroup->addButton(m_oldRadio);
	m_versionGroup->addButton(m_keepRadio);
	m_versionGroup->addButton(m_newRadio);

	// QRadioButton doesn't word-wrap, so the keep option's explanatory sentence lives in a separate
	// wrapped, indented note beneath it (the radio carries the short "Keep … and don't ask again.").
	m_keepNote = new QLabel(versionBox);
	m_keepNote->setObjectName("migrationKeepNote");
	m_keepNote->setWordWrap(true);
	m_keepNote->setText(tr("The part can still be migrated via the Inspector later."));
	m_keepNote->setContentsMargins(24, 0, 0, 0);
	QPalette notePalette = m_keepNote->palette();
	notePalette.setColor(QPalette::WindowText, notePalette.color(QPalette::Disabled, QPalette::WindowText));
	m_keepNote->setPalette(notePalette);

	versionLayout->addWidget(m_oldRadio);
	versionLayout->addWidget(m_keepRadio);
	versionLayout->addWidget(m_keepNote);
	versionLayout->addWidget(m_newRadio);
	layout->addWidget(versionBox);

	// Action + navigation buttons, all on a single row: [Close] [Update all] ..... [Previous] [Next].
	QHBoxLayout* buttonLayout = new QHBoxLayout();

	// "Close" simply closes the dialog (each part keeps whatever version it is showing). On macOS the
	// window's title-bar close button / Cmd-W is the platform-native way to dismiss it, so the
	// explicit button is shown only on Windows and Linux.
#ifndef Q_OS_MACOS
	m_doneButton = new QPushButton(tr("Close"), m_dialog);
	m_doneButton->setObjectName("migrationDoneButton");
	m_doneButton->setToolTip(tr("Close this dialog; your choices are kept"));
	buttonLayout->addWidget(m_doneButton);
#endif

	m_updateAllButton = new QPushButton(tr("Update all"), m_dialog);
	m_updateAllButton->setObjectName("migrationUpdateAllButton");
	m_updateAllButton->setToolTip(tr("Update every outdated part in this list to its newest version"));
	buttonLayout->addWidget(m_updateAllButton);

	buttonLayout->addStretch();

	m_prevButton = new QPushButton(tr("Previous"), m_dialog);
	m_nextButton = new QPushButton(tr("Next"), m_dialog);
	m_prevButton->setObjectName("migrationPrevButton");
	m_nextButton->setObjectName("migrationNextButton");
	m_prevButton->setToolTip(tr("Go back to the previous part"));
	m_nextButton->setToolTip(tr("Go to the next part"));
	buttonLayout->addWidget(m_prevButton);
	buttonLayout->addWidget(m_nextButton);
	layout->addLayout(buttonLayout);

	// Connect controls. buttonClicked fires only on real user interaction, so the programmatic
	// setChecked() in updateDialogForCurrentMigration() never re-triggers a swap.
	connect(m_versionGroup, &QButtonGroup::buttonClicked, this, &MigrationHandler::onVersionChosen);
#ifndef Q_OS_MACOS
	connect(m_doneButton, &QPushButton::clicked, this, &MigrationHandler::closeDialog);
#endif
	connect(m_updateAllButton, &QPushButton::clicked, this, &MigrationHandler::updateAllMigrations);
	connect(m_prevButton, &QPushButton::clicked, this, &MigrationHandler::goToPreviousMigration);
	connect(m_nextButton, &QPushButton::clicked, this, &MigrationHandler::goToNextMigration);
}

void MigrationHandler::updateDialogForCurrentMigration()
{
	if (m_currentIndex < 0 || m_currentIndex >= m_pendingMigrations.size()) {
		closeDialog();
		return;
	}

	MigrationInfo& info = m_pendingMigrations[m_currentIndex];

	// Default is "Old": a part keeps the version it loaded with until the user picks a radio here.
	// (No first-visit preview swap; navigating between parts never changes anything on its own.)

	// Sync our tracked state with the part actually in the sketch before reading it: a manual
	// Undo/Redo may have flipped it old<->new (and changed its item id) behind our back.
	reconcileStateFromSketch(info);

	ItemBase* item = currentItem();
	if (!item) {
		DebugDialog::debug("MigrationHandler: Could not find item for migration");
		// Mark it decided so the wrap-around advance doesn't loop back to it.
		info.decided = true;
		processNextMigration();
		return;
	}

	// Update counter. Don't repeat the window title ("Part Migration") in the content;
	// only show a counter when there are several parts to step through.
	if (m_pendingMigrations.size() == 1) {
		m_counterLabel->setVisible(false);
	} else {
		m_counterLabel->setVisible(true);
		m_counterLabel->setText(tr("Part %1 of %2")
		                            .arg(m_currentIndex + 1)
		                            .arg(m_pendingMigrations.size()));
	}

	// Update title
	QString title = QString("<b>%1</b>").arg(info.instanceTitle);
	if (info.decided) {
		QString outcome = info.silenced ? tr("keeping old version — won't ask again")
		                  : info.isSwapped ? tr("updated to new version")
		                  : tr("keeping old version");
		title += QString(" — <i>%1</i>").arg(outcome);
	} else if (info.isSwapped) {
		title += QString(" <i>(%1)</i>").arg(tr("showing new version"));
	} else {
		title += QString(" <i>(%1)</i>").arg(tr("showing old version"));
	}
	m_titleLabel->setText(title);
	// Expose the raw instance title (the <b>…</b> markup makes the label text awkward to parse)
	// so tests can address the part with the Wire/Part probes by its exact title.
	if (m_dialog != nullptr) {
		m_dialog->setProperty("currentInstanceTitle", info.instanceTitle);
		// Number of parts in this session, exposed for GUI tests (the visible "Part X of Y" counter
		// is hidden for a single-part dialog, so a label can't be parsed in that case).
		m_dialog->setProperty("migrationPartCount", m_pendingMigrations.size());
	}

	// Explanation of why this dialog is being shown (e.g. mixed versions)
	m_reasonLabel->setText(info.reason);
	m_reasonLabel->setVisible(!info.reason.isEmpty());

	// Content area: the change notes, or — when there are none — the names (if they differ)
	// plus a hint to compare the two versions visually across the views.
	QString contentHtml;
	if (!info.relevantHistory.isEmpty()) {
		contentHtml = QString("<p><b>%1</b></p>").arg(tr("Changes since your version:"));
		for (const HistoryEntry& entry : info.relevantHistory) {
			QString modeTag;
			if (entry.isRecommended()) {
				modeTag = QString(" <span style='color: red;'>[%1]</span>").arg(tr("RECOMMENDED"));
			} else if (entry.isRequired()) {
				modeTag = QString(" <span style='color: gray;'>[%1]</span>").arg(tr("AUTOMATIC"));
			}

			contentHtml += QString("<p><b>%1</b> (%2):%3<br/>%4</p>")
			                   .arg(entry.date.toHtmlEscaped())
			                   .arg(entry.author.toHtmlEscaped())
			                   .arg(modeTag)
			                   .arg(entry.description.toHtmlEscaped());
		}
	} else {
		if (!info.newTitle.isEmpty() && info.newTitle != info.oldTitle) {
			contentHtml += QString("<p>%1 &rarr; %2</p>")
			                   .arg(info.oldTitle.toHtmlEscaped(), info.newTitle.toHtmlEscaped());
		}
		contentHtml += QString("<p><i>%1</i></p>").arg(
		    tr("No change notes are available. Compare the old and new version visually in the "
		       "Breadboard, Schematic and PCB views before deciding."));
	}
	// Stepwise migration: this part is replaced one version at a time. If the new target is itself
	// outdated, tell the user the next revision will be offered after they take this step.
	if (info.hasFurtherRevision) {
		contentHtml += QString("<p><i>%1</i></p>").arg(
		    tr("A further revision is available; it will be offered after you update to this one."));
	}
	m_historyLabel->setText(contentHtml);

	// Version chooser: label each radio with the part name + version, and preset (without
	// triggering a swap) to whichever choice currently applies for this part.
	{
		QSignalBlocker blockOld(m_oldRadio);
		QSignalBlocker blockKeep(m_keepRadio);
		QSignalBlocker blockNew(m_newRadio);
		m_oldRadio->setText(info.oldVersion.isEmpty()
		                    ? tr("Old: %1").arg(info.oldTitle)
		                    : tr("Old: %1 (v. %2)").arg(info.oldTitle, info.oldVersion));
		m_keepRadio->setText(info.oldVersion.isEmpty()
		                    ? tr("Keep this version and don't ask again.")
		                    : tr("Keep v. %1 and don't ask again.").arg(info.oldVersion));
		m_newRadio->setText(info.newVersion.isEmpty()
		                    ? tr("New: %1").arg(info.newTitle)
		                    : tr("New: %1 (v. %2)").arg(info.newTitle, info.newVersion));
		if (info.isSwapped) m_newRadio->setChecked(true);
		else if (info.silenced) m_keepRadio->setChecked(true);
		else m_oldRadio->setChecked(true);
	}

	// "Keep … and don't ask again" (persistent silence) is offered only for "optional" migrations;
	// "recommended" parts must keep prompting, so they get Old / New only.
	bool canSilence = (info.effectiveMode == "optional");
	m_keepRadio->setVisible(canSilence);
	m_keepNote->setVisible(canSilence);

	// Navigation between parts: only when there are several to step through. Hide Previous on the
	// first part and Next on the last, rather than leaving a dead greyed-out button there. Keep the
	// enabled state in sync too: it's moot for a hidden button, but it drives the test probe's
	// prev/nextEnabled (which tests use to know when to stop stepping).
	int count = m_pendingMigrations.size();
	bool hasPrev = m_currentIndex > 0;
	bool hasNext = m_currentIndex < count - 1;
	m_updateAllButton->setVisible(count > 1);
	m_prevButton->setVisible(count > 1 && hasPrev);
	m_nextButton->setVisible(count > 1 && hasNext);
	m_prevButton->setEnabled(hasPrev);
	m_nextButton->setEnabled(hasNext);

	// Focus on the part
	centerAndZoomOnItem(item);
}

void MigrationHandler::centerAndZoomOnItem(ItemBase* item)
{
	if (!item) return;

	MainWindow* mainWindow = findMainWindow();
	if (mainWindow == nullptr) return;
	SketchWidget* view = mainWindow->sketchWidgetForView(item->viewID());
	if (view == nullptr) return;

	// Only move/zoom the view when stepping to a DIFFERENT part. Toggling the current part
	// old<->new (same queue position) must not disturb the view at all -- the swap itself keeps the
	// connectors in place (see SketchWidget::computeSwapAlignOffset), and the user shouldn't see the
	// canvas jump just because they compared the two versions.
	if (m_currentIndex != m_focusIndex) {
		// Bring the view that actually contains this part to the front so the user sees it.
		mainWindow->setCurrentView(item->viewID());

		QRectF bounds = item->sceneBoundingRect();
		if (bounds.isEmpty()) bounds = QRectF(item->pos(), QSizeF(1, 1));

		// Zoom once (the first part shown), then keep that zoom while navigating -- only re-zoom
		// when the next/previous part differs in size by >= 2x (or <= 0.5x) so very different parts
		// still fit. Otherwise just recentre (pan) on the part, which is far less jarring.
		qreal partSize = qMax(bounds.width(), bounds.height());
		bool rezoom = (m_focusPartSize <= 0.0)
		           || (partSize >= m_focusPartSize * 2.0)
		           || (partSize <= m_focusPartSize * 0.5);
		if (rezoom) {
			// Fit the part's bounds plus a margin proportional to its size, so small and large parts
			// both fill a consistent fraction of the view.
			qreal margin = partSize * 2.5;
			view->fitInView(bounds.adjusted(-margin, -margin, margin, margin), Qt::KeepAspectRatio);
			view->updateZoomFromCurrentTransform();
			m_focusPartSize = partSize;
		}
		else {
			view->centerOn(bounds.center());
		}
		m_focusIndex = m_currentIndex;
	}

	view->scene()->clearSelection();
	item->setSelected(true);

	// Refresh the Inspector for the now-current part. A programmatic setSelected() does not update it
	// (selectionChangedSlot only calls viewItemInfo during a rubber-band drag), and reverting via undo
	// ("Keep old") deletes the previewed new part without re-showing the restored old one -- so the
	// Inspector would go blank. A forward swap happens to refresh it; do it explicitly here so swap,
	// revert and navigation all keep the Inspector in sync.
	view->viewItemInfo(item);
}

void MigrationHandler::onVersionChosen(QAbstractButton* button)
{
	// The only place a part is swapped/reverted or (un)silenced, so navigating between parts never
	// changes anything until the user actually picks a radio here. Old and "keep … don't ask again"
	// both show the old version; they differ only in whether a persistent silence is written.
	if (m_currentIndex < 0 || m_currentIndex >= m_pendingMigrations.size()) return;

	MigrationInfo& info = m_pendingMigrations[m_currentIndex];

	// Mark these as our own changes so the undo-stack listener ignores the commands they push.
	m_applyingChange = true;
	if (button == m_newRadio) {
		if (!info.isSwapped) swapToNew(info);
		clearSilence(info);
		info.silenced = false;
	}
	else if (button == m_keepRadio) {
		if (info.isSwapped) revertToOld(info);
		applySilence(info);
		info.silenced = true;
	}
	else {   // m_oldRadio
		if (info.isSwapped) revertToOld(info);
		clearSilence(info);
		info.silenced = false;
	}
	info.decided = true;
	m_applyingChange = false;

	updateDialogForCurrentMigration();
}

void MigrationHandler::swapToNew(MigrationInfo& info)
{
	MainWindow* mainWindow = findMainWindow();
	if (!mainWindow) {
		DebugDialog::debug("MigrationHandler: Could not find MainWindow");
		return;
	}
	ItemBase* item = currentItem();
	if (!item) {
		DebugDialog::debug("MigrationHandler: Could not find item for swap");
		return;
	}

	WaitPushUndoStack* stack = m_sketchWidget->undoStack();

	// Remember the old item (its chief id) so a clean undo can restore exactly it.
	info.originalItemId = item->layerKinChief()->id();

	// Reuse the obsolete-swap machinery (swapSelectedAuxAux + portObsoleteSpecialProps). It pushes
	// a single undoable command and returns the new item's id directly, so we never have to
	// rediscover it from the (non-deterministically ordered) selection.
	long newID = mainWindow->swapPartForMigration(item, info.newModuleID);
	if (newID != 0) {
		info.itemId = newID;
		info.newItemId = newID;   // anchor for re-finding the part after a manual undo/redo
	}

	// One top-level command was pushed; remember where, so we can tell later whether our swap
	// is still safely undoable or whether real commands have been pushed on top of it.
	info.swapStackIndex = stack ? stack->index() : -1;
	info.isSwapped = true;

	// The replacement part's *default* title was captured when the migration was queued, but that
	// can misrepresent the migrated instance: the modern resistor defaults to "220 Ω Resistor",
	// yet porting keeps the old 330k. Re-read the actual swapped-in part's title (which reflects the
	// ported resistance/colour) so the "Use new" radio and the change-notes line show the real
	// post-migration value, not the replacement's default.
	if (newID != 0) {
		ItemBase* newItem = mainWindow->findItemInAnyView(newID);
		if (newItem != nullptr && newItem->modelPart() != nullptr) {
			QString actualTitle = newItem->modelPart()->title();
			if (!actualTitle.isEmpty()) info.newTitle = actualTitle;
		}
	}
}

bool MigrationHandler::canUndoOwnSwap(int swapStackIndex) const
{
	// Our swap is safe to undo only if it's still effectively on top: nothing was pushed since,
	// or the only commands pushed since are selection changes (no real edit). Anything else means
	// the user made edits we must not rewind, so the caller should swap forward instead.
	if (swapStackIndex < 0) return false;
	WaitPushUndoStack* stack = m_sketchWidget->undoStack();
	if (stack == nullptr) return false;
	int idx = stack->index();
	if (idx < swapStackIndex) {
		return false;   // user already undid past our swap
	}
	for (int i = swapStackIndex; i < idx; ++i) {
		const QUndoCommand* cmd = stack->command(i);
		if (dynamic_cast<const SelectItemCommand*>(cmd) == nullptr) return false;
	}
	return true;
}

void MigrationHandler::revertToOld(MigrationInfo& info)
{
	WaitPushUndoStack* stack = m_sketchWidget->undoStack();

	bool canUndo = canUndoOwnSwap(info.swapStackIndex);

	if (canUndo && stack != nullptr) {
		// Undo any selection-only commands stacked on top of our swap, then the swap itself.
		int steps = stack->index() - info.swapStackIndex + 1;
		for (int i = 0; i < steps; ++i) stack->undo();
		info.itemId = info.originalItemId;   // undo restores the original old item (same id)
	}
	else {
		// Our swap isn't on top (later parts' preview swaps sit above it), so we can't simply undo it.
		// Swap the part forward from new back to old as a fresh, independent command.
		MainWindow* mainWindow = findMainWindow();
		ItemBase* item = currentItem();
		if (item != nullptr) item = item->layerKinChief();
		if (mainWindow != nullptr && item != nullptr) {
			long newID = mainWindow->swapPartForMigration(item, info.oldModuleID);
			if (newID != 0) info.itemId = newID;
		}
	}

	info.swapStackIndex = -1;
	info.isSwapped = false;
}

void MigrationHandler::applySilence(MigrationInfo& info)
{
	// Persist a "don't ask again" silence for this instance. Only "optional" parts can be silenced;
	// "recommended" parts never expose the keep radio. The caller has already reverted to the old version.
	if (info.effectiveMode != "optional") return;

	// Baseline the silence at the newest relevant entry, stored ISO-normalised so it round-trips
	// and parses back reliably in getRelevantHistory.
	QDate latest;
	for (const HistoryEntry& entry : info.relevantHistory) {
		QDate d = entry.parsedDate();
		if (d.isValid() && (!latest.isValid() || d > latest)) latest = d;
	}
	if (!latest.isValid()) return;

	ItemBase* item = currentItem();
	if (item == nullptr || item->modelPart() == nullptr) return;

	QString isoDate = latest.toString(Qt::ISODate);
	QString oldDate = item->modelPart()->localProp("silencedDate").toString();
	if (oldDate == isoDate) return;   // already silenced at this baseline

	MainWindow* mainWindow = findMainWindow();
	SketchWidget* view = (mainWindow != nullptr) ? mainWindow->sketchWidgetForView(item->viewID()) : nullptr;
	if (view != nullptr && view->undoStack() != nullptr) {
		// Push as a command so the sketch is marked modified (and prompts to save) and the choice is
		// undoable. SetPropCommand routes to the same modelPart localProp.
		auto* cmd = new SetPropCommand(view, item->id(), "silencedDate", oldDate, isoDate, false, nullptr);
		cmd->setText(tr("Silence update reminder for %1").arg(info.instanceTitle));
		view->undoStack()->push(cmd);
	} else {
		item->modelPart()->setLocalProp("silencedDate", isoDate);
	}

	// The "outdated part" badge should disappear now this instance is silenced.
	refreshObsoleteAnnotation(info);
}

void MigrationHandler::clearSilence(MigrationInfo& info)
{
	// Undo a previously-applied silence (the user moved off "keep … don't ask again" back to Old or
	// New), so the update reminder applies again. No-op when nothing was silenced.
	ItemBase* item = currentItem();
	if (item == nullptr || item->modelPart() == nullptr) return;

	QString oldDate = item->modelPart()->localProp("silencedDate").toString();
	if (oldDate.isEmpty()) return;

	MainWindow* mainWindow = findMainWindow();
	SketchWidget* view = (mainWindow != nullptr) ? mainWindow->sketchWidgetForView(item->viewID()) : nullptr;
	if (view != nullptr && view->undoStack() != nullptr) {
		auto* cmd = new SetPropCommand(view, item->id(), "silencedDate", oldDate, QString(), false, nullptr);
		cmd->setText(tr("Re-enable update reminder for %1").arg(info.instanceTitle));
		view->undoStack()->push(cmd);
	} else {
		item->modelPart()->setLocalProp("silencedDate", QString());
	}

	// The "outdated part" badge should reappear now this instance is no longer silenced.
	refreshObsoleteAnnotation(info);
}

void MigrationHandler::refreshObsoleteAnnotation(const MigrationInfo& info)
{
	// Refresh the "outdated part" badge in every view that holds the part.
	MainWindow* mainWindow = findMainWindow();
	if (mainWindow == nullptr) return;
	const ViewLayer::ViewID viewIDs[] = { ViewLayer::BreadboardView, ViewLayer::SchematicView, ViewLayer::PCBView };
	for (ViewLayer::ViewID viewID : viewIDs) {
		SketchWidget* view = mainWindow->sketchWidgetForView(viewID);
		if (view == nullptr) continue;
		ItemBase* viewItem = view->findItem(info.itemId);
		if (viewItem != nullptr) viewItem->layerKinChief()->updateObsoleteAnnotation();
	}
}

int MigrationHandler::indexForItem(qint64 chiefId) const
{
	// Match a part across swaps: itemId is the live id, originalItemId/newItemId are the old/new
	// anchors. The clicked badge sits on the currently-obsolete (old) instance, so any of these
	// catches an already-queued part regardless of its preview state.
	if (chiefId == 0) return -1;
	for (int i = 0; i < m_pendingMigrations.size(); ++i) {
		const MigrationInfo& info = m_pendingMigrations[i];
		if (info.itemId == chiefId || info.originalItemId == chiefId || info.newItemId == chiefId)
			return i;
	}
	return -1;
}

void MigrationHandler::reconcileStateFromSketch(MigrationInfo& info)
{
	// A manual Undo/Redo (reachable while the dialog is focused) can swap the current part old<->new
	// behind our back, which also changes its item id. Probe the ids this migration has used and
	// adopt whichever still resolves, deriving isSwapped/silenced from the live part so the radios
	// match reality. If none resolve (e.g. the part was deleted) leave the tracked state untouched;
	// callers already guard on a null currentItem().
	MainWindow* mainWindow = findMainWindow();
	if (mainWindow == nullptr) return;

	const qint64 candidates[] = { info.itemId, info.newItemId, info.originalItemId };
	for (qint64 id : candidates) {
		if (id == 0) continue;
		ItemBase* item = mainWindow->findItemInAnyView(id);
		if (item == nullptr || item->modelPart() == nullptr) continue;

		QString liveModule = item->modelPart()->moduleID();
		if (liveModule == info.newModuleID) {
			info.isSwapped = true;
			info.itemId = id;
			info.newItemId = id;
			info.silenced = false;        // the new version is never "silenced"
			return;
		}
		if (liveModule == info.oldModuleID) {
			info.isSwapped = false;
			info.itemId = id;
			info.originalItemId = id;
			info.silenced = isSilenceActive(item, info);
			return;
		}
		// Some unrelated module at this id -- not this migration's part; keep looking.
	}
}

bool MigrationHandler::isSilenceActive(ItemBase* item, const MigrationInfo& info) const
{
	// "Keep … and don't ask again" writes the newest relevant entry's date as silencedDate (see
	// applySilence). The silence is in effect iff that exact baseline is still stored -- so undoing
	// the silence command (now possible via Ctrl+Z) flips the keep radio back off.
	if (item == nullptr || item->modelPart() == nullptr) return false;
	QDate latest;
	for (const HistoryEntry& entry : info.relevantHistory) {
		QDate d = entry.parsedDate();
		if (d.isValid() && (!latest.isValid() || d > latest)) latest = d;
	}
	if (!latest.isValid()) return false;
	return item->modelPart()->localProp("silencedDate").toString() == latest.toString(Qt::ISODate);
}

void MigrationHandler::onUndoStackChanged()
{
	// The shared undo stack moved. Ignore our own swap/silence commands (m_applyingChange) and any
	// change while the dialog is closed; otherwise re-sync the dialog to the part now in the sketch
	// so an external Undo/Redo updates the radios (and the focused part) live.
	if (m_applyingChange || m_dialog.isNull()) return;
	if (m_currentIndex < 0 || m_currentIndex >= m_pendingMigrations.size()) return;
	updateDialogForCurrentMigration();
}

void MigrationHandler::processNextMigration()
{
	// Advance to the next part the user hasn't decided yet (wrapping around); close
	// the dialog once every part has a decision.
	int count = m_pendingMigrations.size();
	for (int step = 1; step <= count; ++step) {
		int idx = (m_currentIndex + step) % count;
		if (!m_pendingMigrations[idx].decided) {
			m_currentIndex = idx;
			updateDialogForCurrentMigration();
			return;
		}
	}
	closeDialog();
}

void MigrationHandler::goToPreviousMigration()
{
	// Navigation never reverts; the current part keeps whichever version is showing. The only
	// way to switch a part back to old is the version toggle (or Skip/Silence).
	if (m_currentIndex <= 0) return;
	m_currentIndex--;
	updateDialogForCurrentMigration();
}

void MigrationHandler::goToNextMigration()
{
	if (m_currentIndex >= m_pendingMigrations.size() - 1) return;
	m_currentIndex++;
	updateDialogForCurrentMigration();
}

void MigrationHandler::updateAllMigrations()
{
	MainWindow* mainWindow = findMainWindow();
	if (mainWindow == nullptr) { closeDialog(); return; }

	// Bring every part to its new version: swap the ones still showing old (toggled back, or never
	// swapped) in a single undoable command. Parts already on the new version are left as-is.
	QList<ItemBase*> toSwap;
	for (int i = 0; i < m_pendingMigrations.size(); ++i) {
		ItemBase* item = currentItem(i);
		if (item != nullptr && item->isObsolete()) toSwap << item;
	}

	if (!toSwap.isEmpty()) {
		// Our own change; keep the undo-stack listener from reacting to the command it pushes.
		m_applyingChange = true;
		mainWindow->swapObsoleteDirect(toSwap, false);
		m_applyingChange = false;
	}
	closeDialog();
}

void MigrationHandler::closeDialog()
{
	// Programmatic close (completion / Update all). A non-modal QDialog::close() doesn't reliably
	// emit finished(), so disconnect our handler and run the cleanup ourselves. WA_DeleteOnClose
	// still deletes the dialog; m_dialog (QPointer) auto-nulls afterwards.
	if (m_dialog) {
		disconnect(m_dialog, nullptr, this, nullptr);
		m_dialog->close();
	}
	onDialogClosed();
}

void MigrationHandler::onDialogClosed()
{
	// The dialog closed (ESC, window close, "Update all", or normal completion). Closing does NOT
	// revert anything — whichever version each part is showing is the user's choice and is kept.
	// Just drop the session state. The dialog deletes itself (WA_DeleteOnClose) and m_dialog
	// (a QPointer) auto-nulls, so a later open starts clean.
	m_pendingMigrations.clear();
	m_currentIndex = 0;
	m_focusPartSize = -1.0;
	m_focusIndex = -1;
	m_pendingFocusId = 0;

	// These widgets are children of the dialog and are being destroyed with it; drop the
	// dangling raw pointers so nothing touches them before the next createMigrationDialog().
	m_counterLabel = nullptr;
	m_titleLabel = nullptr;
	m_reasonLabel = nullptr;
	m_historyLabel = nullptr;
	m_versionGroup = nullptr;
	m_oldRadio = nullptr;
	m_keepRadio = nullptr;
	m_newRadio = nullptr;
	m_keepNote = nullptr;
	m_doneButton = nullptr;
	m_updateAllButton = nullptr;
	m_prevButton = nullptr;
	m_nextButton = nullptr;
}

MainWindow* MigrationHandler::findMainWindow() const
{
	QWidget* widget = qobject_cast<QWidget*>(m_sketchWidget);
	while (widget) {
		MainWindow* mainWindow = qobject_cast<MainWindow*>(widget);
		if (mainWindow) return mainWindow;
		widget = widget->parentWidget();
	}
	return nullptr;
}
