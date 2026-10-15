/*******************************************************************

Part of the Fritzing project - http://fritzing.org
Copyright (c) 2025 Fritzing GmbH

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

#include "utils/familypropertycombobox.h"
#include "utils/schematicrectconstants.h"
#include "utils/textutils.h"
#include "version/version.h"
#include "../version/version.h"
#include "dipv2.h"
#include "pinheader.h"
#include "qdir.h"

#include <QFontMetricsF>

//////////////////////////////////////////////////

DipV2::DipV2( ModelPart * modelPart, ViewLayer::ViewID viewID, const ViewGeometry & viewGeometry, long id, QMenu * itemMenu, bool doLabel)
	: Dip(modelPart, viewID, viewGeometry, id, itemMenu, doLabel)
{
}

DipV2::~DipV2() {
}

QString DipV2::genDipV2FZP(const QString & moduleid)
{
	QString spacingString;
	int pins = TextUtils::getPinsAndSpacing(moduleid, spacingString);
	
	if (pins < 4 || pins > 128) {
		return "";
	}
	
	QString holeSize;
	int hsix = moduleid.lastIndexOf(HoleSizePrefix);
	if (hsix >= 0) {
		QStringList strings = moduleid.mid(hsix).split("_");
		if (strings.size() >= 4) {
			holeSize = strings.at(2) + "," + strings.at(3);
		}
	}
	
	QString templateString;
	QFile file(":/resources/templates/generic_dip_v2_fzpTemplate.txt");
	if (file.open(QIODevice::ReadOnly)) {
		templateString = QString::fromUtf8(file.readAll());
		file.close();
	}
	
	if (templateString.isEmpty()) {
		return "";
	}
	
	QString connectorTemplate;
	QFile connectorFile(":/resources/templates/generic_tht_male_connector_fzpTemplate.txt");
	if (connectorFile.open(QIODevice::ReadOnly)) {
		connectorTemplate = QString::fromUtf8(connectorFile.readAll());
		connectorFile.close();
	}
	
	QString connectors;
	for (int i = 0; i < pins; i++) {
		connectors += connectorTemplate.arg(i).arg(i + 1);
		if (i < pins - 1) connectors += "\n";
	}
	
	// Template parameters: %1=pins, %2=connectors, %3=version, %4=holeSize, %5=moduleId, %6=spacing
	QString result = templateString.arg(pins).arg(connectors).arg(Version::versionString()).arg(holeSize).arg(moduleid).arg(spacingString);
	
	return result;
}

QString DipV2::genModuleIDV2(QMap<QString, QString> & currPropsMap)
{
	QString spacing = currPropsMap.value("pin spacing", "300mil");
	QString pins = currPropsMap.value("pins");
	if (pins.isEmpty()) pins = "16";

	int p = pins.toInt();
	if (p < 4) p = 4;
	if (p % 2 == 1) p--;
	return QString("generic_ic_dip_v2_%1_%2").arg(QString::number(p), spacing);
}

QString DipV2::makeSchematicV2Svg(const QString & expectedFileName)
{
	QStringList pieces = expectedFileName.split("_");
	if (pieces.count() != 6) return "";

	QString spacing;
	int pins = TextUtils::getPinsAndSpacing(expectedFileName, spacing);
	QStringList labels;
	for (int i = 0; i < pins; i++) {
		labels << QString::number(i + 1);
	}

	return makeSchematicV2SvgWithLabel(labels, "IC");
}

QString DipV2::makeSchematicV2SvgWithLabel(const QStringList & labels, const QString & chipLabel)
{
	QDomDocument fakeDoc;

	QList<QDomElement> lefts;
	QList<QDomElement> rights;
	for (int i = 0; i < labels.count() / 2; i++) {
		QDomElement element = fakeDoc.createElement("contact");
		element.setAttribute("connectorIndex", i);
		element.setAttribute("name", labels.at(i));
		lefts.append(element);
		int j = labels.count() - i - 1;
		element = fakeDoc.createElement("contact");
		element.setAttribute("connectorIndex", j);
		element.setAttribute("name", labels.at(j));
		rights.append(element);
	}
	QList<QDomElement> empty;
	QString ic = chipLabel;
	
	QString svg = SchematicRectConstants::genSchematicDIPv2(empty, empty, lefts, rights, ic, false, false, SchematicRectConstants::simpleGetConnectorName);
	
	return svg;
}

QString DipV2::makeSchematicSipV2Svg(const QStringList & labels)
{
	QString ic("IC");
	return makeSchematicSipV2SvgWithLabel(labels, ic);
}

QString DipV2::makeSchematicSipV2SvgWithLabel(const QStringList & labels, const QString & chipLabel)
{
	QDomDocument fakeDoc;

	QList<QDomElement> lefts;
	for (int i = 0; i < labels.count(); i++) {
		QDomElement element = fakeDoc.createElement("contact");
		element.setAttribute("connectorIndex", i);
		element.setAttribute("name", labels.at(i));
		lefts.append(element);
	}
	QList<QDomElement> empty;
	QString ic = chipLabel;
	
	QString svg = SchematicRectConstants::genSchematicDIPv2(empty, empty, lefts, empty, ic, false, false, SchematicRectConstants::simpleGetConnectorName);
	
	return svg;
}

QString DipV2::makeSvg(const QString & chipLabel, bool replace) {
	QString svg = MysteryPart::makeSvg(chipLabel, false);
	if (svg.isEmpty() || !replace) return svg;
	
	QStringList words = chipLabel.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
	QHash<QString, QString> replacements;
	
	for (int i = 0; i < words.size(); i++) {
		QString labelId = (i == 0) ? "label" : QString("label%1").arg(i + 1);
		replacements[labelId] = words[i];
	}
	
	return TextUtils::replaceTextElements(svg, replacements);
}


QString DipV2::makeBreadboardDipV2Svg(const QString & expectedFileName)
{
	QStringList pieces = expectedFileName.split("_");
	if (pieces.count() != 7) return "";

	int pins = pieces.at(4).toInt();
	int increment = 10;
	double spacing = TextUtils::convertToInches(pieces.at(5)) * 100;
	
	int spacingMils = TextUtils::convertToInches(pieces.at(5)) * 1000;
	int maxLines = spacingMils / 100 - 1;
	
	QString textElements;
	double baseY = 14.0;
	double lineSpacing = 10.0;
	for (int i = 0; i < maxLines; i++) {
		double y = baseY + (i * lineSpacing);
		QString labelId = (i == 0) ? "label" : QString("label%1").arg(i + 1);
		QString labelText = (i == 0) ? "IC" : "";
		textElements += QString("<text id='%1' x='7' y='%2' fill='#e6e6e6' stroke='none' font-family='OCR-Fritzing-mono' text-anchor='start' font-size='7.5' >%3</text>\n")
		               .arg(labelId)
		               .arg(y)
		               .arg(labelText);
	}

	QString repeatB("<rect id='connector%1pin' x='{13.5}' y='[28.66]' fill='#8C8C8C' width='3' height='4.34'/>\n"
	                "<polygon fill='#8C8C8C' points='{11.5},[28.66] {11.5},[29.74] {13.5},[30.66] {16.5},[30.66] {18.5},[29.74] {18.5},[28.66]'/> \n");

	QString repeatT("<rect id='connector%1pin' x='[13.5]' y='0' fill='#8C8C8C' width='3' height='4.34'/>\n"
	                "<polygon fill='#8C8C8C' points='[18.5],4.34 [18.5],3.26 [16.5],2.34 [13.5],2.34 [11.5],3.26 [11.5],4.34'/>\n");

	QString header("<?xml version='1.0' encoding='utf-8'?>\n"
	               "<svg version='1.2' baseProfile='tiny' id='svg2' xmlns='http://www.w3.org/2000/svg' \n"
	               "width='.percent.1in' height='%1in' viewBox='0 0 {20.0} [33.0]' >\n"
	               "<g id='breadboard'>\n"
	               "<rect id='middle' x='0' y='3' fill='#303030' width='{20.0}' height='[27.0]' />\n"
	               "<rect id='top' x='0' y='3.0' fill='#3D3D3D' width='{20.0}' height='2.46'/>\n"
	               "<rect id='bottom' x='0' y='[27.54]' fill='#000000' width='{20.0}' height='2.46'/>\n"
	               "<polygon id='right' fill='#141414' points='{20.0},3 {19.25},5.46 {19.25},[27.54] {20.0},[30.0]'/> \n"
	               "<polygon id='left' fill='#1F1F1F' points='0,3 0.75,5.46 0.75,[27.54] 0,[30.0]'/>\n"
	               "<polygon id='left-upper-rect' fill='#1C1C1C' points='5,{{11.5}} 0.75,{{11.46}} 0.56,{{13.58}} 0.56,{{16.5}} 5,{{16.5}}'/>\n"
	               "<polygon id='left-lower-rect' fill='#383838' points='0.75,{{21.55}} 5,{{21.55}} 5,{{16.5}} 0.56,{{16.5}} 0.56,{{19.42}}'/>\n"
	               "<path id='slot' fill='#262626' d='M0.56,{{13.58}}v5.83c1.47-0.17,2.62-1.4,2.62-2.92C3.18,{{14.97}},2.04,{{13.75}},0.56,{{13.58}}z'/>\n"
	               "<path id='cover' fill='#303030' d='M0.75,5.46V{{11.45}}c2.38,0.45,4.19,2.53,4.19,5.04c0,2.51-1.8,4.6-4.19,5.05V{{21.55}}h5.0V5.46H0.75z'/>\n"
	               "<circle fill='#212121' cx='6.5' cy='[23.47]' r='2.06'/>\n"
				   "%5\n"
	               "<rect id='connector0pin' x='3.5' y='[28.66]' fill='#8C8C8C' width='3' height='4.34'/>\n"
	               "<polygon fill='#8C8C8C' points='3.5,[28.66] 3.5,[30.66] 6.5,[30.66] 8.5,[29.74] 8.5,[28.66] '/>\n"

	               "<rect id='connector%2pin' x='3.5' fill='#8C8C8C' width='3' height='4.34'/>\n"
	               "<polygon fill='#8C8C8C' points='8.5,4.34 8.5,3.26 6.5,2.34 3.5,2.34 3.5,4.34'/>\n"

	               "<rect id='connector%3pin' x='{13.5}' y='[28.66]' fill='#8C8C8C' width='3' height='4.34'/>\n"
	               "<polygon fill='#8C8C8C' points='{11.5},[28.66] {11.5},[29.74] {13.5},[30.66] {16.5},[30.66] {16.5},[28.66]'/>\n"

	               "<rect id='connector%4pin' x='{13.5}' y='0' fill='#8C8C8C' width='3' height='4.34'/>\n"
	               "<polygon fill='#8C8C8C' points='{16.5},4.34 {16.5},2.34 {13.5},2.34 {11.5},3.26 {11.5},4.34 '/>\n"

	               ".percent.2\n"
	               ".percent.3\n"

	               "</g>\n"
	               "</svg>\n");

	// Process the template similar to the original but without terminal elements
	header = TextUtils::incrementTemplateString(header, 1, spacing - (increment * 3), TextUtils::incMultiplyPinFunction, TextUtils::noCopyPinFunction, nullptr);
	header = header
	         .arg(TextUtils::getViewBoxCoord(header, 3) / 100.0)
	         .arg(pins - 1)
	         .arg((pins / 2) - 1)
	         .arg(pins / 2)
	         .arg(textElements);
	header.replace("{{", "[");
	header.replace("}}", "]");
	header = TextUtils::incrementTemplateString(header, 1, (spacing - (increment * 3)) / 2, TextUtils::incMultiplyPinFunction, TextUtils::noCopyPinFunction, nullptr);
	header.replace("{", "[");
	header.replace("}", "]");
	header.replace(".percent.", "%");

	QString svg = TextUtils::incrementTemplateString(header, 1, increment * ((pins - 4) / 2), TextUtils::incMultiplyPinFunction, TextUtils::noCopyPinFunction, nullptr);

	qDebug() << "DipV2 makeBreadboardDipV2Svg intermediate SVG:" << svg;

	repeatB = TextUtils::incrementTemplateString(repeatB, 1, spacing - (increment * 3), TextUtils::incMultiplyPinFunction, TextUtils::noCopyPinFunction, nullptr);
	repeatB.replace("{", "[");
	repeatB.replace("}", "]");

	int userData[2];
	userData[0] = pins - 1;
	userData[1] = 1;
	QString repeatTs = TextUtils::incrementTemplateString(repeatT, (pins - 4) / 2, increment, TextUtils::standardMultiplyPinFunction, TextUtils::negIncCopyPinFunction, userData);
	QString repeatBs = TextUtils::incrementTemplateString(repeatB, (pins - 4) / 2, increment, TextUtils::standardMultiplyPinFunction, TextUtils::incCopyPinFunction, nullptr);

	QString finalSvg = svg.arg(TextUtils::getViewBoxCoord(svg, 2) / 100.0).arg(repeatTs, repeatBs);
	qDebug() << "DipV2 makeBreadboardDipV2Svg final SVG:" << finalSvg;
	return finalSvg;
}

QString DipV2::retrieveSchematicSvg(const QString & svg) {
	bool hasLocal = false;
	QStringList labels = getPinLabels(hasLocal);

	QString chipLabel = m_chipLabel;
	if (chipLabel.contains("\\n")) {
		chipLabel.replace("\\n", " ");
	}
	
	if (this->isDIP()) {
		return makeSchematicV2SvgWithLabel(labels, chipLabel);
	}
	else {
		return makeSchematicSipV2SvgWithLabel(labels, chipLabel);
	}
}


bool DipV2::changePinLabels(bool sip) {
	if (m_viewID != ViewLayer::SchematicView) return true;

	bool hasLocal = false;
	QStringList labels = getPinLabels(hasLocal);
	if (labels.count() == 0) return true;

	QTransform transform = untransform();

	QString svg;
	svg = retrieveSchematicSvg(svg);

	resetLayerKin(svg);
	resetConnectors();

	retransform(transform);

	return true;
}

void DipV2::swapEntry(const QString & text) {
	auto * comboBox = qobject_cast<FamilyPropertyComboBox *>(sender());
	if (comboBox == nullptr) return;

	bool sip = moduleID().contains("sip");
	bool dip = moduleID().contains("dip");

	if (comboBox->prop().contains("package", Qt::CaseInsensitive)) {
		sip = text.contains("sip", Qt::CaseInsensitive);
		dip = text.contains("dip", Qt::CaseInsensitive);
		if (!dip && !sip) {
			PaletteItem::swapEntry(text);
			return;
		}
	}

	if (sip) {
		generateSwap(text, genModuleID, genSipFZP, makeBreadboardSvg, makeSchematicSvg, PinHeader::makePcbSvg);
	}
	else if (dip) {
		generateSwap(text, genModuleIDV2, genDipV2FZP, makeBreadboardDipV2Svg, makeSchematicV2Svg, makePcbDipSvg);
	}
	PaletteItem::swapEntry(text);
}
