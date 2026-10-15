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

#ifndef DIPV2_H
#define DIPV2_H

#include "dip.h"

class DipV2 : public Dip
{
	Q_OBJECT

public:
	explicit DipV2(ModelPart *, ViewLayer::ViewID, const ViewGeometry & viewGeometry, long id, QMenu * itemMenu, bool doLabel);
	~DipV2();

public:
	static QString genDipV2FZP(const QString & moduleid);
	static QString genModuleIDV2(QMap<QString, QString> & currPropsMap);
	static QString makeSchematicV2Svg(const QString & expectedFileName);
	static QString makeSchematicV2SvgWithLabel(const QStringList & labels, const QString & chipLabel);
	static QString makeSchematicSipV2Svg(const QStringList & labels);
	static QString makeSchematicSipV2SvgWithLabel(const QStringList & labels, const QString & chipLabel);
	static QString makeBreadboardDipV2Svg(const QString & expectedFileName);

public:
	bool changePinLabels(bool sip) override;

public Q_SLOTS:
	void swapEntry(const QString & text) override;

protected:
	QString retrieveSchematicSvg(const QString & svg) override;
	QString makeSvg(const QString & chipLabel, bool replace) override;

private:
	static QString processTextWithNewlines(const QString & text);
};

#endif
