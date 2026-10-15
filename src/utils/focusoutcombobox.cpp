/*******************************************************************

Part of the Fritzing project - http://fritzing.org
Copyright (c) 2007-2019 Fritzing

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

#include "focusoutcombobox.h"
#include "../debugdialog.h"

#include <QTimer>

FocusOutComboBox::FocusOutComboBox(QWidget * parent) : QComboBox(parent) {
	this->setFocusPolicy(Qt::StrongFocus);
	setEditable(true);
	// The default editable-combo completer does inline completion: it snaps
	// typed text to the nearest preset and commits that preset on Enter, which
	// makes entering a value that is not in the dropdown (e.g. an E3/E6
	// capacitance) feel constrained. Disable auto-completion; the preset
	// dropdown and the validator are unaffected.
	setCompleter(nullptr);
	m_wasOut = true;
	lineEdit()->installEventFilter( this );

	// After a value is chosen from the dropdown, the line edit keeps focus with the
	// cursor at the end and nothing selected. Validators anchor their pattern, so the
	// next keystroke appends to a complete value (e.g. "100nF" -> "100nF2"), which is
	// rejected character by character and makes the field feel frozen. Select the text
	// on activation so the next keystroke replaces it instead of appending. Queued so it
	// runs after Qt's own post-selection cursor handling, which would otherwise put the
	// cursor at the end and clear the selection again.
	connect(this, &QComboBox::activated, this, [this](int) { checkSelectAll(); }, Qt::QueuedConnection);
}

FocusOutComboBox::~FocusOutComboBox() {
}

void FocusOutComboBox::focusInEvent(QFocusEvent * e) {
	//DebugDialog::debug("focus in");
	QComboBox::focusInEvent(e);
	// Select the whole value on focus so typing replaces it. Deferred to the next event-loop
	// pass so it runs after the click that gave us focus has positioned the cursor; a direct
	// selectAll() here gets cleared by that click, which is why a committed value sometimes
	// could not be typed over without selecting it (mark + delete) by hand first.
	QTimer::singleShot(0, this, [this]() { checkSelectAll(); });
}

void FocusOutComboBox::focusOutEvent(QFocusEvent * e) {
	//DebugDialog::debug("focus out");
	m_wasOut = true;
	QComboBox::focusOutEvent(e);
	QString t = this->currentText();
	QString it = this->itemText(this->currentIndex());
	if (t.compare(it) != 0) {
		int ix = findText(t);
		if (ix == -1) {
			addItem(t);
			ix = count() - 1;
		}
		setCurrentIndex(ix);
	}
}

void FocusOutComboBox::wheelEvent(QWheelEvent* e)
{
	if (!this->hasFocus()) {
	  e->ignore();
	}
}

bool FocusOutComboBox::eventFilter( QObject *target, QEvent *event ) {
	// subclassing mouseReleaseEvent doesn't seem to work so use eventfilter instead
	if( target == lineEdit() && event->type() == QEvent::MouseButtonRelease ) {
		if (m_wasOut) {
			// only select all the first time the focused lineEdit is clicked, not every time,
			// otherwise you can't move the selection point with the mouse
			checkSelectAll();
			m_wasOut = false;
		}
	}
	return false;
}

void FocusOutComboBox::checkSelectAll() {
	if((lineEdit() != nullptr) && !lineEdit()->hasSelectedText() && isEnabled()) {
		lineEdit()->selectAll();
	}
}
