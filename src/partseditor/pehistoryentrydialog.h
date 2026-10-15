/*******************************************************************

Part of the Fritzing project - http://fritzing.org
Copyright (c) 2026 Fritzing

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


#ifndef PEHISTORYENTRYDIALOG_H
#define PEHISTORYENTRYDIALOG_H

#include <QDialog>
#include <QString>

#include "../model/modelpartshared.h"   // HistoryEntry

class QLabel;
class QDateEdit;
class QLineEdit;
class QComboBox;
class QPlainTextEdit;
class QCheckBox;
class QDialogButtonBox;
class QPushButton;

// Shared editor for a single part-revision <history> entry. One dialog serves the three
// callers described in fritzing-monkey/docs/parts-editor-history-plan.md:
//   - Add ("+")         : allowEmpty=false, no bump  -> a fresh entry, OK gated on non-empty text
//   - Edit (<=3 mo row) : allowEmpty=false, no bump  -> seeded from the clicked entry
//   - Save gate         : allowEmpty=true,  bumpToVersion=X -> skippable, shows the bump checkbox
class PEHistoryEntryDialog : public QDialog
{
	Q_OBJECT

public:
	// `entry` seeds the fields (date falls back to today when empty/invalid; the mode combo is
	// preset alias-aware via HistoryEntry::isRequired/isRecommended/isOptional, defaulting to
	// "optional"). When `allowEmpty` is false, OK stays disabled until the change text is
	// non-empty. When `bumpToVersion` is non-empty, an unchecked "Bump version to <X>" checkbox
	// is shown.
	PEHistoryEntryDialog(const HistoryEntry & entry, bool allowEmpty,
	                     const QString & bumpToVersion = QString(), QWidget * parent = nullptr);
	~PEHistoryEntryDialog();

	HistoryEntry entry() const;   // current field values (ISO date, author, canonical mode token, description)
	bool bumpVersion() const;     // bump checkbox state; false when no checkbox is shown
	bool hasText() const;         // change text is non-empty (lets the save gate tell "add" from "skip")

	// Optional warning/explanation strip above the fields (used by the save gate).
	void setHeaderText(const QString & text);

	// Reconfigure the buttons for the save gate: "Add entry && save" (requires text) / "Cancel" /
	// "Save without an entry" (sets skipped()). Pair with allowEmpty=false and a bumpToVersion.
	void setSaveGateButtons();
	bool skipped() const;         // true when the user chose "Save without an entry"

protected Q_SLOTS:
	void updateOkState();

protected:
	QLabel * m_headerLabel;
	QDateEdit * m_dateEdit;
	QLineEdit * m_authorEdit;
	QComboBox * m_modeEdit;
	QPlainTextEdit * m_textEdit;
	QCheckBox * m_bumpEdit;            // nullptr unless a bump version was supplied
	QDialogButtonBox * m_buttonBox;
	QPushButton * m_acceptButton;      // primary accept button (Ok, or "Add entry && save")
	bool m_allowEmpty;
	bool m_skipped;
};

#endif
