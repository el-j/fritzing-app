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

#ifndef BOMPDFGENERATOR_H
#define BOMPDFGENERATOR_H

#include <QCoreApplication>
#include <QString>
#include <QStringList>
#include <QList>

class ItemBase;
class SketchWidget;
class QPainter;
class QPageLayout;

struct BomPdfRow {
	QString label;
	QString value;
	QString partType;
	QString package;
	QString properties;
	int quantity;
};

class BomPdfGenerator
{
	Q_DECLARE_TR_FUNCTIONS(BomPdfGenerator)

public:
	static bool exportToPdf(
		const QString & fileName,
		const QString & sketchFileName,
		QList<ItemBase*> & partList,
		SketchWidget * schematicView
	);

private:
	static QString getPackage(ItemBase * itemBase);
	static QString getProperties(ItemBase * itemBase);
	static void collectMetadata(SketchWidget * schematicView,
	                            QString & projectName,
	                            QString & projectDescr,
	                            QString & projectVersion);
	static void drawHeader(QPainter & painter, const QPageLayout & pageLayout,
	                       const QString & projectName, const QString & projectDescr,
	                       const QString & sketchFileName);
	static int drawTableSection(QPainter & painter, const QPageLayout & pageLayout,
	                            const QString & sectionTitle,
	                            const QStringList & headers,
	                            const QList<QStringList> & rows,
	                            int startY,
	                            const QString & projectName, const QString & projectVersion,
	                            const QString & sketchFileName, int & pageNumber,
	                            int totalPages);
	static void drawFooter(QPainter & painter, const QPageLayout & pageLayout,
	                       int pageNumber, int totalPages,
	                       const QString & projectName, const QString & projectVersion,
	                       const QString & sketchFileName);
	static void drawCheckbox(QPainter & painter, int x, int y, int size);
	static int measureTotalPages(int assemblyRowCount, int shoppingRowCount,
	                             int firstSectionStartY, int pageHeight,
	                             int footerHeight, int marginTop,
	                             int rowHeight, int tableHeaderHeight,
	                             int sectionHeaderHeight, int continuationHeaderHeight,
	                             int sectionSpacing, int minSectionHeight);
};

#endif // BOMPDFGENERATOR_H
