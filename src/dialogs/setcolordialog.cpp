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

#include "setcolordialog.h"
#include "../utils/clickablelabel.h"

#include <QLabel>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QPalette>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>

static constexpr int MARGIN = 5;
static constexpr int BUTTON_WIDTH = 150;

/////////////////////////////////////

SetColorDialog::SetColorDialog(const QString & message, QColor & currentColor, QColor & standardColor, bool askPrefs, QWidget *parent) : QDialog(parent),
    m_message(message),
    m_currentColor(currentColor),
    m_standardColor(standardColor),
    m_selectedColor(currentColor),
    m_currentColorLabel(nullptr),
    m_standardColorLabel(nullptr),
    m_customColorLabel(nullptr),
    m_selectedColorLabel(nullptr),
    m_prefsCheckBox(nullptr)
{
	this->setWindowTitle(tr("%1 Color...").arg(message));

	auto * vLayout = new QVBoxLayout(this);

	auto * label = new QLabel(tr("Choose %1 color:").arg(message.toLower()));
	vLayout->addWidget(label);

	auto * f4 = new QFrame();
	auto * hLayout4 = new QHBoxLayout(f4);
	m_selectedColorLabel = new QLabel();
	setColor(currentColor);
	hLayout4->addWidget(m_selectedColorLabel);
	m_selectedColorLabel->setContentsMargins(MARGIN, MARGIN, MARGIN, MARGIN);
	vLayout->addWidget(f4);

	auto * f2 = new QFrame();
	auto * hLayout2 = new QHBoxLayout(f2);
	auto *button2 = new QPushButton(tr("Reset to default"));
	button2->setFixedWidth(BUTTON_WIDTH);
	connect(button2, SIGNAL(clicked()), this, SLOT(selectStandard()));
	hLayout2->addWidget(button2);
	m_standardColorLabel = new ClickableLabel(tr("Default color (%1)").arg(standardColor.name()), this);
	connect(m_standardColorLabel, SIGNAL(clicked()), this, SLOT(selectStandard()));
	m_standardColorLabel->setPalette(QPalette(standardColor));
	m_standardColorLabel->setAutoFillBackground(true);
	m_standardColorLabel->setContentsMargins(MARGIN, MARGIN, MARGIN, MARGIN);
	hLayout2->addWidget(m_standardColorLabel);
	vLayout->addWidget(f2);

	auto * f3 = new QFrame();
	auto * hLayout3 = new QHBoxLayout(f3);
	auto *button3 = new QPushButton(tr("Pick custom color ..."));
	connect(button3, SIGNAL(clicked()), this, SLOT(selectCustom()));
	hLayout3->addWidget(button3);
	setCustomColor(currentColor);
	vLayout->addWidget(f3);

	auto * spacerItem = new QSpacerItem( 0, 10 );
	vLayout->addSpacerItem(spacerItem);


	if (askPrefs) {
		auto * f5 = new QFrame();
		auto * hLayout5 = new QHBoxLayout(f5);
		m_prefsCheckBox = new QCheckBox(tr("Make this the default %1 color").arg(message.toLower()));
		hLayout5->addWidget(m_prefsCheckBox);
		vLayout->addWidget(f5);
	}

	auto * buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
	buttonBox->button(QDialogButtonBox::Ok)->setText(tr("OK"));
	connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
	connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

	vLayout->addWidget(buttonBox);

	this->setLayout(vLayout);
}

void SetColorDialog::selectCurrent() {
	setColor(m_currentColor);
}

void SetColorDialog::selectStandard() {
	setColor(m_standardColor);
}

void SetColorDialog::selectLastCustom() {
	setColor(m_customColor);
}

void SetColorDialog::selectCustom() {
	QColor color = QColorDialog::getColor ( m_selectedColor, this, tr("Pick custom %1 color").arg(m_message.toLower()));
	if (!color.isValid()) return;

	setColor(color);
	setCustomColor(color);
}

bool SetColorDialog::isPrefsColor() {
	if (m_prefsCheckBox == nullptr) return false;

	return m_prefsCheckBox->isChecked();
}

void SetColorDialog::setCustomColor(const QColor & color) {
	m_customColor = color;
}

void SetColorDialog::setColor(const QColor & color) {
	m_selectedColor = color;
	m_selectedColorLabel->setText(tr("Current color (%1)").arg(color.name()));
	m_selectedColorLabel->setPalette(QPalette(color));
	m_selectedColorLabel->setAutoFillBackground(true);
}
