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


#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QDateEdit>
#include <QLineEdit>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QDate>

#include "pehistoryentrydialog.h"

PEHistoryEntryDialog::PEHistoryEntryDialog(const HistoryEntry & entry, bool allowEmpty,
        const QString & bumpToVersion, QWidget * parent)
	: QDialog(parent)
	, m_bumpEdit(nullptr)
	, m_acceptButton(nullptr)
	, m_allowEmpty(allowEmpty)
	, m_skipped(false)
{
	setWindowTitle(tr("Revision"));

	auto * layout = new QVBoxLayout(this);

	m_headerLabel = new QLabel(this);
	m_headerLabel->setWordWrap(true);
	m_headerLabel->setObjectName("PEHistoryEntryHeader");
	m_headerLabel->hide();   // shown only when setHeaderText() is given non-empty text
	layout->addWidget(m_headerLabel);

	auto * form = new QFormLayout();

	// Date -- ISO yyyy-MM-dd, defaults to the entry's date or today.
	QDate date = QDate::fromString(entry.date, Qt::ISODate);
	if (!date.isValid()) date = QDate::currentDate();
	m_dateEdit = new QDateEdit(date, this);
	m_dateEdit->setDisplayFormat("yyyy-MM-dd");
	m_dateEdit->setCalendarPopup(true);
	m_dateEdit->setStatusTip(tr("Date of this revision"));
	form->addRow(tr("Date"), m_dateEdit);

	m_authorEdit = new QLineEdit(entry.author, this);
	m_authorEdit->setStatusTip(tr("Who made this revision"));
	form->addRow(tr("Author"), m_authorEdit);

	// Mode -- the canonical token is stored as item data; the visible text is translatable.
	// Preset alias-aware so a part written with legacy silent/forced/ask still selects correctly.
	m_modeEdit = new QComboBox(this);
	m_modeEdit->addItem(tr("required"), "required");
	m_modeEdit->addItem(tr("recommended"), "recommended");
	m_modeEdit->addItem(tr("optional"), "optional");
	int modeIndex = 2;   // optional by default (matches parseHistory's default)
	if (entry.isRequired()) modeIndex = 0;
	else if (entry.isRecommended()) modeIndex = 1;
	m_modeEdit->setCurrentIndex(modeIndex);
	m_modeEdit->setStatusTip(tr("How insistently this revision is offered when an older part is loaded"));
	form->addRow(tr("Mode"), m_modeEdit);

	// Changes -- the revision description. Plain text in v1 (see plan: rich text is a future).
	m_textEdit = new QPlainTextEdit(entry.description, this);
	m_textEdit->setTabChangesFocus(true);
	m_textEdit->setStatusTip(tr("Describe what changed in this revision"));
	form->addRow(tr("Changes"), m_textEdit);

	layout->addLayout(form);

	if (!bumpToVersion.isEmpty()) {
		m_bumpEdit = new QCheckBox(tr("Bump version to %1").arg(bumpToVersion), this);
		m_bumpEdit->setChecked(false);
		m_bumpEdit->setStatusTip(tr("Raise the part's version number when saving"));
		layout->addWidget(m_bumpEdit);
	}

	m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	m_acceptButton = m_buttonBox->button(QDialogButtonBox::Ok);
	layout->addWidget(m_buttonBox);
	connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

	connect(m_textEdit, &QPlainTextEdit::textChanged, this, &PEHistoryEntryDialog::updateOkState);
	updateOkState();
}

PEHistoryEntryDialog::~PEHistoryEntryDialog() {
}

void PEHistoryEntryDialog::setHeaderText(const QString & text) {
	m_headerLabel->setText(text);
	m_headerLabel->setVisible(!text.isEmpty());
}

void PEHistoryEntryDialog::setSaveGateButtons() {
	m_buttonBox->clear();
	m_acceptButton = m_buttonBox->addButton(tr("Add entry && save"), QDialogButtonBox::AcceptRole);
	m_buttonBox->addButton(QDialogButtonBox::Cancel);
	QPushButton * skip = m_buttonBox->addButton(tr("Save without an entry"), QDialogButtonBox::ActionRole);
	connect(skip, &QPushButton::clicked, this, [this]() { m_skipped = true; accept(); });
	updateOkState();
}

bool PEHistoryEntryDialog::skipped() const {
	return m_skipped;
}

bool PEHistoryEntryDialog::hasText() const {
	return !m_textEdit->toPlainText().trimmed().isEmpty();
}

void PEHistoryEntryDialog::updateOkState() {
	if (m_acceptButton == nullptr) return;
	m_acceptButton->setEnabled(m_allowEmpty || hasText());
}

HistoryEntry PEHistoryEntryDialog::entry() const {
	HistoryEntry e;
	e.date = m_dateEdit->date().toString(Qt::ISODate);
	e.author = m_authorEdit->text().trimmed();
	e.mode = m_modeEdit->currentData().toString();
	e.description = m_textEdit->toPlainText().trimmed();
	return e;
}

bool PEHistoryEntryDialog::bumpVersion() const {
	return m_bumpEdit != nullptr && m_bumpEdit->isChecked();
}
