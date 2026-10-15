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

#include "textmetrics.h"
#include "textutils.h"
#include <QDebug>

TextMetrics::TextMetrics()
	: m_fontFamily("Arial")
	, m_fontSize(12.0)
	, m_fontWeight(QFont::Light)
	, m_fontStyle(QFont::StyleNormal)
	, m_sizeCorrectionFactor(1.0)
	, m_font()
	, m_fontMetrics(m_font)
	, m_fontValid(false)
	, m_metricsValid(false)
{
}

TextMetrics::TextMetrics(const QDomElement & element)
	: m_fontFamily("Arial")
	, m_fontSize(12.0)
	, m_fontWeight(QFont::Light)
	, m_fontStyle(QFont::StyleNormal)
	, m_sizeCorrectionFactor(1.0)
	, m_font()
	, m_fontMetrics(m_font)
	, m_fontValid(false)
	, m_metricsValid(false)
{
	lookupAttributes(element);
}

TextMetrics TextMetrics::derive(const QDomElement & element) const
{
	TextMetrics derived = *this;
	
	// Only look at the specific element's attributes, don't traverse parents
	QString family = element.attribute("font-family");
	if (!family.isEmpty()) {
		derived.m_fontFamily = family;
		derived.m_fontValid = false;
		derived.m_metricsValid = false;
	}
	
	QString size = element.attribute("font-size");
	if (!size.isEmpty()) {
		bool ok;
		double sizeValue = size.toDouble(&ok);
		if (ok && sizeValue > 0) {
			derived.m_fontSize = sizeValue;
			derived.m_fontValid = false;
			derived.m_metricsValid = false;
		}
	}
	
	QString weight = element.attribute("font-weight");
	if (!weight.isEmpty()) {
		if (weight.toLower() == "bold" || weight == "700" || weight == "800" || weight == "900") {
			derived.m_fontWeight = QFont::Bold;
		} else if (weight.toLower() == "normal" || weight == "400") {
			derived.m_fontWeight = QFont::Light;
		}
		derived.m_fontValid = false;
		derived.m_metricsValid = false;
	}
	
	QString style = element.attribute("font-style");
	if (!style.isEmpty()) {
		if (style.toLower() == "italic") {
			derived.m_fontStyle = QFont::StyleItalic;
		} else if (style.toLower() == "oblique") {
			derived.m_fontStyle = QFont::StyleOblique;
		} else if (style.toLower() == "normal") {
			derived.m_fontStyle = QFont::StyleNormal;
		}
		derived.m_fontValid = false;
		derived.m_metricsValid = false;
	}
	
	return derived;
}

QFont TextMetrics::getFont() const
{
	if (!m_fontValid) {
		// Always use font size 10 for stability, calculate correction factor for all sizes
		// ! Does this really work for all font sizes? It seems required to get symetric advance and width calculations
		// This is especially true for font sizes < 1.0 (here, qfontmetrics is completely wrong). But also, the alignment for
		// fonts of sizes up to 20 seems to be jumpy without this.
		double actualFontSize = m_fontSize;
		double fontSizeToUse = 10.0;
		m_sizeCorrectionFactor = actualFontSize / 10.0;
		
		m_font = QFont(m_fontFamily, (int)fontSizeToUse, m_fontWeight, m_fontStyle == QFont::StyleItalic);
		m_font.setStyle(m_fontStyle);
		m_fontValid = true;
		m_metricsValid = false;
	}
	return m_font;
}

QFontMetricsF TextMetrics::getFontMetrics() const
{
	if (!m_metricsValid) {
		getFont();
		m_fontMetrics = QFontMetricsF(m_font);
		m_metricsValid = true;
	}
	return m_fontMetrics;
}

double TextMetrics::parseCoordinate(const QString & value, bool * ok) const
{
	if (ok) *ok = false;
	
	if (value.isEmpty()) {
		return 0.0;
	}
	
	QString trimmedValue = value.trimmed();
	if (trimmedValue.endsWith("em")) {
		QString numberStr = trimmedValue.left(trimmedValue.length() - 2);
		bool conversionOk;
		double factor = numberStr.toDouble(&conversionOk);
		if (conversionOk) {
			if (ok) *ok = true;
			// getFontMetrics() ensures font is created and sets correction factor
			return factor * getFontMetrics().height() * getFontScalingFactor() * m_sizeCorrectionFactor;
		}
	} else if (trimmedValue.endsWith("ex")) {
		QString numberStr = trimmedValue.left(trimmedValue.length() - 2);
		bool conversionOk;
		double factor = numberStr.toDouble(&conversionOk);
		if (conversionOk) {
			if (ok) *ok = true;
			// getFontMetrics() ensures font is created and sets correction factor
			return factor * getFontMetrics().xHeight() * getFontScalingFactor() * m_sizeCorrectionFactor;
		}
	} else {
		// Try unit conversion first (handles px, in, mm, etc.)
		if (auto result = TextUtils::convertToInches(trimmedValue, false)) {
			if (ok) *ok = true;
			return *result * 90.0; // Convert inches to SVG units (90 DPI)
		}
		
		// Fallback to plain number
		bool conversionOk;
		double result = trimmedValue.toDouble(&conversionOk);
		if (conversionOk) {
			if (ok) *ok = true;
			return result;
		}
	}
	
	return 0.0;
}

double TextMetrics::horizontalAdvance(const QString & text) const
{
	if (text.isEmpty()) {
		return 0.0;
	}
	
	bool hasTrailingWhitespace = text.length() > 0 && text.at(text.length() - 1).isSpace();
	QString compactedText = text.simplified();
	
	if (compactedText.isEmpty()) {
		return 0.0;
	}
	
	if (hasTrailingWhitespace) {
		compactedText += " ";
	}
	
	// getFontMetrics() ensures font is created and sets correction factor
	return getFontMetrics().horizontalAdvance(compactedText) * getFontScalingFactor() * m_sizeCorrectionFactor;
}

double TextMetrics::width(const QString & text) const
{
	if (text.isEmpty()) {
		return 0.0;
	}

	bool hasTrailingWhitespace = text.length() > 0 && text.at(text.length() - 1).isSpace();
	QString compactedText = text.simplified();

	if (compactedText.isEmpty()) {
		return 0.0;
	}

	if (hasTrailingWhitespace) {
		compactedText += " ";
	}

	// getFontMetrics() ensures font is created and sets correction factor
	return getFontMetrics().size(0, compactedText).width() * getFontScalingFactor() * m_sizeCorrectionFactor;
}


void TextMetrics::lookupAttributes(const QDomElement & element)
{
	QDomElement current = element;
	QSet<QString> foundAttributes;
	
	while (!current.isNull() && foundAttributes.size() < 4) {
		if (!foundAttributes.contains("font-family")) {
			QString family = current.attribute("font-family");
			if (!family.isEmpty()) {
				m_fontFamily = family;
				foundAttributes.insert("font-family");
			}
		}
		
		if (!foundAttributes.contains("font-size")) {
			QString size = current.attribute("font-size");
			if (!size.isEmpty()) {
				bool ok;
				double sizeValue = size.toDouble(&ok);
				if (ok && sizeValue > 0) {
					m_fontSize = sizeValue;
					foundAttributes.insert("font-size");
				}
			}
		}
		
		if (!foundAttributes.contains("font-weight")) {
			QString weight = current.attribute("font-weight");
			if (!weight.isEmpty()) {
				if (weight.toLower() == "bold" || weight == "700" || weight == "800" || weight == "900") {
					m_fontWeight = QFont::Bold;
				} else if (weight.toLower() == "normal" || weight == "400") {
					m_fontWeight = QFont::Normal;
				}
				foundAttributes.insert("font-weight");
			}
		}
		
		if (!foundAttributes.contains("font-style")) {
			QString style = current.attribute("font-style");
			if (!style.isEmpty()) {
				if (style.toLower() == "italic") {
					m_fontStyle = QFont::StyleItalic;
				} else if (style.toLower() == "oblique") {
					m_fontStyle = QFont::StyleOblique;
				} else if (style.toLower() == "normal") {
					m_fontStyle = QFont::StyleNormal;
				}
				foundAttributes.insert("font-style");
			}
		}
		
		QDomNode parent = current.parentNode();
		if (parent.nodeType() == QDomNode::ElementNode) {
			current = parent.toElement();
		} else {
			break;
		}
	}
	
	m_fontValid = false;
	m_metricsValid = false;
}

double TextMetrics::getFontScalingFactor() const
{
	// Font-specific scaling factors based on platform and font characteristics
	// Default factor discovered by bisecting until text alignment works with OCR-Fritzing-mono
	double scalingFactor = 0.77;
	
#ifdef Q_OS_MAC
	// macOS doesn't need correction for most fonts
	scalingFactor = 1.0;

// The size calculation seems to be correct. With the factor it results in roughly the same
// x-advance and width as Chrome of Firefox.
// However, on windows and macOS, QSvg seems to render fonts bigger than firefox or chrome would
// Until this is fixed, fine tuning size calculation doesn't make sense.
//
// #elif defined(Q_OS_WIN)
// 	// Windows has font-specific scaling requirements
// 	if (m_fontFamily.contains("OCR", Qt::CaseInsensitive) ||
// 		m_fontFamily.contains("OCR-Fritzing", Qt::CaseInsensitive)) {
// 		scalingFactor = 0.77;
// 	} else if (m_fontFamily.contains("Droid Sans", Qt::CaseInsensitive)) {
// 		scalingFactor = 0.77;
// 	} else if (m_fontFamily.contains("Noto Sans", Qt::CaseInsensitive)) {
// 		scalingFactor = 0.77;
// 	} else {
// 		scalingFactor = 0.77;
// 	}
// #else
// 	// Linux and other platforms
// 	if (m_fontFamily.contains("OCR", Qt::CaseInsensitive) ||
// 		m_fontFamily.contains("OCR-Fritzing", Qt::CaseInsensitive)) {
// 		scalingFactor = 0.77;
// 	} else if (m_fontFamily.contains("Droid Sans", Qt::CaseInsensitive)) {
// 		scalingFactor = 0.77;
// 	} else if (m_fontFamily.contains("Noto Sans", Qt::CaseInsensitive)) {
// 		scalingFactor = 0.77;
// 	} else {
// 		scalingFactor = 0.77;  // Default for Linux
// 	}

#endif

	return scalingFactor;
}

bool TextMetrics::equals(const TextMetrics & other) const
{
	return m_fontFamily == other.m_fontFamily &&
		   m_fontSize == other.m_fontSize &&
		   m_fontWeight == other.m_fontWeight &&
		   m_fontStyle == other.m_fontStyle;
}
