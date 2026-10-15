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

#ifndef TEXTMETRICS_H
#define TEXTMETRICS_H

#include <QDomElement>
#include <QFont>
#include <QFontMetricsF>

class TextMetrics
{
public:
	TextMetrics();
	explicit TextMetrics(const QDomElement & element);
	
	TextMetrics derive(const QDomElement & element) const;
	
	QFont getFont() const;
	QFontMetricsF getFontMetrics() const;
	double parseCoordinate(const QString & value, bool * ok = nullptr) const;
	double horizontalAdvance(const QString & text) const;
	double width(const QString & text) const;
	double getFontScalingFactor() const;
	
	bool equals(const TextMetrics & other) const;

private:
	void lookupAttributes(const QDomElement & element);
	
	QString m_fontFamily;
	double m_fontSize;
	int m_fontWeight;
	QFont::Style m_fontStyle;
	
	mutable double m_sizeCorrectionFactor;
	mutable QFont m_font;
	mutable QFontMetricsF m_fontMetrics;
	mutable bool m_fontValid;
	mutable bool m_metricsValid;
};

#endif
