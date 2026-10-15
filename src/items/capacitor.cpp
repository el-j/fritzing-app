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

#include "capacitor.h"


#include "../utils/textutils.h"
#include "../utils/focusoutcombobox.h"
#include "../utils/boundedregexpvalidator.h"
#include "../utils/fmessagebox.h"
#include "../sketch/infographicsview.h"
#include "partlabel.h"

// TODO
//	save into parts bin

static bool isFaradCapacitance(const QString & prop, const QString & symbol)
{
	return symbol == "F" && prop.contains("capacitance", Qt::CaseInsensitive);
}

static QString formatPropertyDefValue(PropertyDef * propertyDef, double value)
{
	if (isFaradCapacitance(propertyDef->name, propertyDef->symbol)) {
		return TextUtils::convertToPowerPrefixByThousands(value) + propertyDef->symbol;
	}

	return TextUtils::convertToPowerPrefix(value) + propertyDef->symbol;
}

static QString normalizeCapacitanceValue(const QString & value, const QString & symbol)
{
	QString temp = value.trimmed();
	if (temp.isEmpty()) return temp;

	temp.replace(TextUtils::AltMicroSymbol, TextUtils::MicroSymbol);

	double q = TextUtils::convertFromPowerPrefixU(temp, symbol);
	if (q == 0) return value;   // not a parseable magnitude; leave it unchanged
	return TextUtils::convertToPowerPrefixByThousands(q) + symbol;
}

// Returns the standard (E-series) table value closest to `value` by ratio, or -1 if the
// table is empty. Sets `matches` when `value` already equals a table value (within a small
// tolerance that absorbs floating-point noise but stays well below E-series spacing).
static double nearestStandardValue(const QList<double> & items, double value, bool & matches)
{
	matches = false;
	if (value <= 0) return -1;

	double best = -1;
	double bestRatio = 0;
	for (double item : items) {
		if (item <= 0) continue;
		double ratio = (value > item) ? value / item : item / value;
		if (best < 0 || ratio < bestRatio) {
			bestRatio = ratio;
			best = item;
		}
	}
	if (best > 0 && bestRatio <= 1.0001) matches = true;
	return best;
}

Capacitor::Capacitor( ModelPart * modelPart, ViewLayer::ViewID viewID, const ViewGeometry & viewGeometry, long id, QMenu * itemMenu, bool doLabel)
	: PaletteItem(modelPart, viewID, viewGeometry, id, itemMenu, doLabel)
{
	PropertyDefMaster::initPropertyDefs(modelPart, m_propertyDefs);
	// Normalize stored farad values (including any loaded from older sketches)
	Q_FOREACH (PropertyDef * propertyDef, m_propertyDefs.keys()) {
		if (isFaradCapacitance(propertyDef->name, propertyDef->symbol)) {
			setProp(propertyDef->name, m_propertyDefs.value(propertyDef));
		}
	}
}

Capacitor::~Capacitor() {
}

ItemBase::PluralType Capacitor::isPlural() {
	return Plural;
}

bool Capacitor::collectExtraInfo(QWidget * parent, const QString & family, const QString & prop, const QString & value, bool swappingEnabled, QString & returnProp, QString & returnValue, QWidget * & returnWidget, bool & hide)
{
	Q_FOREACH (PropertyDef * propertyDef, m_propertyDefs.keys()) {
		if (prop.compare(propertyDef->name, Qt::CaseInsensitive) == 0) {
			returnProp = TranslatedPropertyNames.value(prop);
			if (returnProp.isEmpty()) {
				returnProp = propertyDef->name;
			}

			auto * focusOutComboBox = new FocusOutComboBox();
			focusOutComboBox->setEnabled(swappingEnabled);
			focusOutComboBox->setEditable(propertyDef->editable);
			focusOutComboBox->setObjectName("infoViewComboBox");
			// Tag with the property so test probes can target this specific editor
			// (many inspector editors share the objectName "infoViewComboBox").
			focusOutComboBox->setProperty("fProbeProperty", propertyDef->name);
			QString current = m_propertyDefs.value(propertyDef);
			if (current.isEmpty() && !propertyDef->defaultValue.isEmpty()) {
				current = propertyDef->defaultValue + propertyDef->symbol;
				setProp(propertyDef->name, current);
			}
			if (propertyDef->editable) {
				focusOutComboBox->setToolTip(tr("Select from the dropdown, or type in a %1 value").arg(returnProp));
			}

			if (propertyDef->numeric) {
				focusOutComboBox->setToolTip(tr("Select from the dropdown, or type in a %1 value\n"
												"Range: [%2 - %3] %4\n"
												"Background: Green = ok, Red = incorrect value, Grey = current value").
											 arg(returnProp).
											 arg(TextUtils::convertToPowerPrefix(propertyDef->minValue)).
											 arg(TextUtils::convertToPowerPrefix(propertyDef->maxValue)).
											 arg(propertyDef->symbol));
				if (!current.isEmpty()) {
					double val = TextUtils::convertFromPowerPrefixU(current, propertyDef->symbol);
					if (!propertyDef->menuItems.contains(val)) {
						propertyDef->menuItems.append(val);
					}
				}
				Q_FOREACH(double q, propertyDef->menuItems) {
					QString s = formatPropertyDefValue(propertyDef, q);
					focusOutComboBox->addItem(s);
				}
			}
			else {
				if (!current.isEmpty()) {
					if (!propertyDef->sMenuItems.contains(current)) {
						propertyDef->sMenuItems.append(current);
					}
				}
				focusOutComboBox->addItems(propertyDef->sMenuItems);
			}
			if (!current.isEmpty()) {
				int ix = focusOutComboBox->findText(current);
				if (ix < 0) {
					focusOutComboBox->addItem(current);
					ix = focusOutComboBox->findText(current);
				}
				focusOutComboBox->setCurrentIndex(ix);
			}

			if (propertyDef->editable) {
				auto * validator = new BoundedRegExpValidator(focusOutComboBox);
				validator->setSymbol(propertyDef->symbol);
				validator->setConverter(TextUtils::convertFromPowerPrefix);
				if (propertyDef->maxValue > propertyDef->minValue) {
					validator->setBounds(propertyDef->minValue, propertyDef->maxValue);
				}
                QString symbolRegExp = propertyDef->symbol.isEmpty() ? "" : QString("[%1]{0,1}").arg(propertyDef->symbol);

    //			QString pattern = QString("((\\d{0,10})|(\\d{0,10}\\.)|(\\d{0,10}\\.\\d{1,10}))[%1]{0,1}%2")
                QString pattern = QString("(-?(?:\\d{1,7}(?:[.,]\\d{0,3})?|[.,]\\d{1,3}))[%1]{0,1}%2").arg(
    //			QString pattern = QString("((\\d{0,3})|(\\d{0,3}\\.)|(\\d{0,3}\\.\\d{1,3}))[%1]{0,1}%2")
					TextUtils::PowerPrefixesString,
                    symbolRegExp
                );
				validator->setRegularExpression(QRegularExpression(pattern));
				focusOutComboBox->setValidator(validator);
				connect(focusOutComboBox->validator(), SIGNAL(sendState(QValidator::State)), this, SLOT(textModified(QValidator::State)));
				connect(focusOutComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(propertyEntry(int)));
			}
			else {
				connect(focusOutComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(simplePropertyEntry(int)));
			}

			this->m_comboBoxes.insert(propertyDef, focusOutComboBox);

			returnValue = focusOutComboBox->currentText();
			returnWidget = focusOutComboBox;

			return true;
		}
	}

	return PaletteItem::collectExtraInfo(parent, family, prop, value, swappingEnabled, returnProp, returnValue, returnWidget, hide);
}

/**
 * Sets the appropriate background colour in the combo box when the text is modified. When a user changes the text of an editable
 * property in a combo box, this function is called. The function checks the validator of the combo box and sets a red background if the
 * state is Intermediate (the value does is not within the limits of the property) or green if the state is Acceptable. Invalid states
 * are not possible as the characters that make the string invalid are deleted if introduced.
 * not changed in this function.
 * @param[in] text The new string in the combo box.
 */
void Capacitor::textModified(QValidator::State state) {
	BoundedRegExpValidator * validator = qobject_cast<BoundedRegExpValidator *>(sender());
	if (validator == NULL) return;
	FocusOutComboBox * focusOutComboBox = qobject_cast<FocusOutComboBox *>(validator->parent());
	if (focusOutComboBox == NULL) return;

	if (state == QValidator::Acceptable) {
		QColor backColor = QColor(210, 246, 210);
		QLineEdit *lineEditor = focusOutComboBox->lineEdit();
		QPalette pal = lineEditor->palette();
		pal.setColor(QPalette::Base, backColor);
		lineEditor->setPalette(pal);
	} else if (state == QValidator::Intermediate) {
		QColor backColor = QColor(246, 210, 210);
		QLineEdit *lineEditor = focusOutComboBox->lineEdit();
		QPalette pal = lineEditor->palette();
		pal.setColor(QPalette::Base, backColor);
		lineEditor->setPalette(pal);
	}
}

void Capacitor::propertyEntry(int index) {
	auto * focusOutComboBox = qobject_cast<FocusOutComboBox *>(sender());
	if (focusOutComboBox == nullptr) return;
	QString text = focusOutComboBox->itemText(index);

	Q_FOREACH (PropertyDef * propertyDef, m_comboBoxes.keys()) {
		if (m_comboBoxes.value(propertyDef) == focusOutComboBox) {
			QString utext = text;
			if (propertyDef->numeric) {
				double val = TextUtils::convertFromPowerPrefixU(utext, propertyDef->symbol);

				if (isFaradCapacitance(propertyDef->name, propertyDef->symbol) && val > 0) {
					// Prompt for any positive value, including out-of-range ones: fixup no longer
					// rescales the input, so the dialog can show what the user actually typed and
					// offer the nearest standard value.
					// What the user typed (with the unit) and how that value will actually be shown.
					QString entered = utext;
					if (!entered.endsWith(propertyDef->symbol)) entered.append(propertyDef->symbol);
					QString canonical = formatPropertyDefValue(propertyDef, val);

					bool matches = false;
					double nearest = nearestStandardValue(propertyDef->menuItems, val, matches);
					if (matches) {
						// Standard value: snap to the exact table value (avoids floating-point
						// near-duplicates), and note any change of representation (e.g. 0.022mF -> 22µF).
						val = nearest;
						utext = canonical;
						if (entered != canonical) {
							FMessageBox::information(nullptr, tr("Capacitance", "dialog title"),
								tr("%1 will be displayed as %2.").arg(entered, canonical));
						}
					}
					else if (nearest > 0) {
						// Not a standard value: offer the nearest one, showing what the user typed.
						QString nearestStr = formatPropertyDefValue(propertyDef, nearest);
						auto answer = FMessageBox::question(nullptr, tr("Capacitance", "dialog title"),
							tr("Replace %1 with the nearest standard value %2?").arg(entered, nearestStr),
							FMessageBox::Yes | FMessageBox::No, FMessageBox::Yes);
						if (answer == FMessageBox::Yes) {
							val = nearest;
							utext = nearestStr;
						}
						else if (entered != canonical) {
							// Kept their value: still note how it will be shown (e.g. 0.02mF -> 20µF).
							FMessageBox::information(nullptr, tr("Capacitance", "dialog title"),
								tr("%1 will be displayed as %2.").arg(entered, canonical));
						}
					}
				}

				if (!propertyDef->menuItems.contains(val)) {
					// info view is redrawn, so combobox is recreated, so the new item is added to the combo box menu
					propertyDef->menuItems.append(val);
				}
			}
			else {
				if (!propertyDef->sMenuItems.contains(text)) {
					// info view is redrawn, so combobox is recreated, so the new item is added to the combo box menu
					propertyDef->sMenuItems.append(text);
				}
			}

			InfoGraphicsView * infoGraphicsView = InfoGraphicsView::getInfoGraphicsView(this);
			if (infoGraphicsView != nullptr) {
				// Apply to every selected part that has this property, not just this one
				// (mirrors Resistor::resistanceEntry -> setResistance).
				infoGraphicsView->setPropForSelection(propertyDef->name, utext);
			}
			break;
		}
	}
}

void Capacitor::setProp(const QString & prop, const QString & value) {
	Q_FOREACH (PropertyDef * propertyDef, m_propertyDefs.keys()) {
		if (prop.compare(propertyDef->name, Qt::CaseInsensitive) == 0) {
			QString normalized = value;
			if (isFaradCapacitance(propertyDef->name, propertyDef->symbol)) {
				normalized = normalizeCapacitanceValue(value, propertyDef->symbol);
			}
			m_propertyDefs.insert(propertyDef, normalized);
			modelPart()->setLocalProp(propertyDef->name, normalized);
			if (m_partLabel != nullptr) m_partLabel->displayTextsIf();
			return;
		}
	}

	PaletteItem::setProp(prop, value);
}

void Capacitor::simplePropertyEntry(int index) {

	auto * focusOutComboBox = qobject_cast<FocusOutComboBox *>(sender());
	if (focusOutComboBox == nullptr) return;
	QString text = focusOutComboBox->itemText(index);

	Q_FOREACH (PropertyDef * propertyDef, m_comboBoxes.keys()) {
		if (m_comboBoxes.value(propertyDef) == focusOutComboBox) {
			InfoGraphicsView * infoGraphicsView = InfoGraphicsView::getInfoGraphicsView(this);
			if (infoGraphicsView != nullptr) {
				// Apply to every selected part that has this property, not just this one.
				infoGraphicsView->setPropForSelection(propertyDef->name, text);
			}
			break;
		}
	}
}

void Capacitor::getProperties(QHash<QString, QString> & hash) {
	Q_FOREACH (PropertyDef * propertyDef, m_propertyDefs.keys()) {
		hash.insert(propertyDef->name, m_propertyDefs.value(propertyDef));
	}
}

QHash<QString, QString> Capacitor::prepareProps(ModelPart * modelPart, bool wantDebug, QStringList & keys)
{
	QHash<QString, QString> props = ItemBase::prepareProps(modelPart, wantDebug, keys);

	// ensure capacitance and other properties are after family, if it is a capacitor;
	if (keys.removeOne("capacitance")) {
		keys.insert(1, "capacitance");
		if (keys.removeOne("voltage")) keys.insert(2, "voltage");
	}

	return props;
}
