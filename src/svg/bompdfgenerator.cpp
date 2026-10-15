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

#include "bompdfgenerator.h"

#include <QPainter>
#include <QPrinter>
#include <QPageLayout>
#include <QMarginsF>
#include <QFileInfo>
#include <QDateTime>
#include <QLocale>
#include <QFont>
#include <QSvgRenderer>
#include <QMap>
#include <QDesktopServices>
#include <QUrl>

#include "../items/itembase.h"
#include "../model/modelpart.h"
#include "../model/modelpartshared.h"
#include "../sketch/sketchwidget.h"
#include "../items/moduleidnames.h"
#include "../version/version.h"
#include "../debugdialog.h"

// Column width percentages (total = 100%)
static const double CheckboxColPercent = 0.04;
static const double LabelQtyColPercent = 0.10;
static const double ValueColPercent = 0.10;      // reduced ~30%
static const double PartTypeColPercent = 0.26;   // increased ~30%
static const double PackageColPercent = 0.14;
static const double PropertiesColPercent = 0.36; // reduced

// Layout constants
static const int MarginMM = 15;
static const int HeaderHeightMM = 45;
static const int RowHeightPt = 18;
static const int TableHeaderHeightPt = 22;
static const int FooterHeightPt = 30;
static const int SectionSpacingPt = 25;
// Vertical padding (in points) added below a section title before the table starts.
// Used in exportToPdf, drawTableSection, and measureTotalPages — must match.
static const int SectionTitleSpacingPt = 8;

// Font sizes in points
static const int TitleFontSize = 18;
static const int SectionHeaderFontSize = 14;
static const int TableHeaderFontSize = 10;
static const int TableBodyFontSize = 9;
static const int FooterFontSize = 8;
static const int MetadataFontSize = 10;

bool BomPdfGenerator::exportToPdf(
	const QString & fileName,
	const QString & sketchFileName,
	QList<ItemBase*> & partList,
	SketchWidget * schematicView)
{
	// Collect project metadata from schematic frame
	QString projectName;
	QString projectDescr;
	QString projectVersion;
	collectMetadata(schematicView, projectName, projectDescr, projectVersion);

	// Build assembly list and shopping list data
	QList<BomPdfRow> assemblyList;
	QMap<QString, BomPdfRow> shoppingMap;

	for (ItemBase * itemBase : partList) {
		if (!itemBase->isBomItem()) continue;

		BomPdfRow row;
		row.label = itemBase->instanceTitle();
		row.value = itemBase->electricalValue();
		row.partType = itemBase->title();
		row.package = getPackage(itemBase);
		row.properties = getProperties(itemBase);
		row.quantity = 1;

		assemblyList.append(row);

		// For shopping list, group by value + partType + package + properties
		QString key = row.value + "|" + row.partType + "|" + row.package + "|" + row.properties;
		auto it = shoppingMap.find(key);
		if (it != shoppingMap.end()) {
			it.value().quantity++;
		} else {
			shoppingMap.insert(key, row);
		}
	}

	// Convert shopping map to list
	QList<BomPdfRow> shoppingList = shoppingMap.values();

	// Set up printer
	QPrinter printer(QPrinter::HighResolution);
	printer.setOutputFormat(QPrinter::PdfFormat);
	printer.setOutputFileName(fileName);
	printer.setFontEmbeddingEnabled(true);

	// Set PDF metadata
	QString docTitle = projectName.isEmpty()
		? tr("%1 - Bill of Materials").arg(QFileInfo(sketchFileName).completeBaseName())
		: tr("%1 - Bill of Materials").arg(projectName);
	printer.setDocName(docTitle);
	printer.setCreator(QString("Fritzing %1.%2.%3 - https://fritzing.org")
		.arg(Version::majorVersion())
		.arg(Version::minorVersion())
		.arg(Version::minorSubVersion()));

	// Use A4 or Letter based on locale
	QLocale locale;
	if (locale.measurementSystem() == QLocale::ImperialSystem) {
		printer.setPageSize(QPageSize(QPageSize::Letter));
	} else {
		printer.setPageSize(QPageSize(QPageSize::A4));
	}

	QMarginsF margins(MarginMM, MarginMM, MarginMM, MarginMM);
	QPageLayout pageLayout(printer.pageLayout().pageSize(), QPageLayout::Portrait, margins, QPageLayout::Millimeter);
	printer.setPageLayout(pageLayout);

	QPainter painter;
	if (!painter.begin(&printer)) {
		return false;
	}

	// Get page dimensions in device pixels
	QRectF pageRect = printer.pageRect(QPrinter::DevicePixel);
	int pageHeight = static_cast<int>(pageRect.height());

	// Convert layout heights from points/mm to device pixels
	int headerHeight = static_cast<int>(HeaderHeightMM * printer.resolution() / 25.4);
	int footerHeight = FooterHeightPt * printer.resolution() / 72;
	int rowHeight = RowHeightPt * printer.resolution() / 72;
	int tableHeaderHeight = TableHeaderHeightPt * printer.resolution() / 72;
	int sectionHeaderHeight = (SectionHeaderFontSize + SectionTitleSpacingPt) * printer.resolution() / 72;
	int continuationHeaderHeight = SectionHeaderFontSize * printer.resolution() / 72;
	int sectionSpacing = SectionSpacingPt * printer.resolution() / 72;
	int marginTop = MarginMM * printer.resolution() / 25.4;
	int minSectionHeight = (SectionHeaderFontSize + TableHeaderHeightPt + RowHeightPt * 2) * printer.resolution() / 72;

	// Calculate total pages by simulating the same pagination decisions
	// drawTableSection makes — keeping these two in sync is what guarantees
	// the footer's Page X / Y agrees with reality.
	int totalPages = measureTotalPages(
		assemblyList.size(), shoppingList.size(),
		headerHeight, pageHeight, footerHeight, marginTop,
		rowHeight, tableHeaderHeight, sectionHeaderHeight,
		continuationHeaderHeight, sectionSpacing, minSectionHeight);

	// Draw header on first page
	drawHeader(painter, pageLayout, projectName, projectDescr, sketchFileName);

	int currentY = headerHeight;
	int pageNumber = 1;

	// Prepare assembly list table data
	QStringList assemblyHeaders;
	assemblyHeaders << "" << tr("Label") << tr("Value") << tr("Part Type") << tr("Package") << tr("Properties");

	QList<QStringList> assemblyRows;
	for (const BomPdfRow & row : assemblyList) {
		QStringList rowData;
		rowData << "" << row.label << row.value << row.partType << row.package << row.properties;
		assemblyRows.append(rowData);
	}

	// Draw assembly list section
	currentY = drawTableSection(painter, pageLayout, tr("Assembly List"), assemblyHeaders, assemblyRows,
	                            currentY, projectName, projectVersion, sketchFileName, pageNumber,
	                            totalPages);

	// Add spacing between sections
	currentY += sectionSpacing;

	// Check if we need a new page for shopping list
	int availableHeight = pageHeight - currentY - footerHeight;

	if (availableHeight < minSectionHeight) {
		drawFooter(painter, pageLayout, pageNumber, totalPages, projectName, projectVersion, sketchFileName);
		printer.newPage();
		pageNumber++;
		currentY = marginTop;
	}

	// Prepare shopping list table data
	QStringList shoppingHeaders;
	shoppingHeaders << "" << tr("Qty") << tr("Value") << tr("Part Type") << tr("Package") << tr("Properties");

	QList<QStringList> shoppingRows;
	for (const BomPdfRow & row : shoppingList) {
		QStringList rowData;
		rowData << "" << QString::number(row.quantity) << row.value << row.partType << row.package << row.properties;
		shoppingRows.append(rowData);
	}

	// Draw shopping list section
	currentY = drawTableSection(painter, pageLayout, tr("Shopping List"), shoppingHeaders, shoppingRows,
	                            currentY, projectName, projectVersion, sketchFileName, pageNumber,
	                            totalPages);

	// Draw footer on last page
	drawFooter(painter, pageLayout, pageNumber, totalPages, projectName, projectVersion, sketchFileName);

	painter.end();

	// In debug builds, auto-open the generated PDF for quick visual inspection.
	if (DebugDialog::enabled()) {
		QDesktopServices::openUrl(QUrl::fromLocalFile(fileName));
	}

	return true;
}

QString BomPdfGenerator::getPackage(ItemBase * itemBase)
{
	if (itemBase == nullptr || itemBase->modelPartShared() == nullptr) {
		return "";
	}
	return itemBase->modelPartShared()->properties().value("package", "");
}

QString BomPdfGenerator::getProperties(ItemBase * itemBase)
{
	if (itemBase == nullptr) {
		return "";
	}

	QStringList props;
	static const QStringList propertyNames = {"mn", "mpn", "part number"};

	for (const QString & propName : propertyNames) {
		QString value = itemBase->prop(propName);
		if (!value.isEmpty()) {
			props << propName + ": " + value;
		}
	}

	return props.join(", ");
}

void BomPdfGenerator::collectMetadata(SketchWidget * schematicView,
                                      QString & projectName,
                                      QString & projectDescr,
                                      QString & projectVersion)
{
	if (schematicView == nullptr) return;

	QList<ItemBase*> items;
	schematicView->collectParts(items);

	for (ItemBase* item : items) {
		if (item->moduleID().endsWith(ModuleIDNames::SchematicFrameModuleIDName)) {
			projectName = item->modelPart()->localProp("project").toString();
			projectDescr = item->modelPart()->localProp("descr").toString();
			projectVersion = item->modelPart()->localProp("rev").toString();
			return;
		}
	}
}

void BomPdfGenerator::drawHeader(QPainter & painter, const QPageLayout & pageLayout,
                                 const QString & projectName, const QString & projectDescr,
                                 const QString & sketchFileName)
{
	int resolution = painter.device()->logicalDpiX();
	QRectF paintRect = pageLayout.paintRect(QPageLayout::Point);
	int pageWidth = static_cast<int>(paintRect.width() * resolution / 72);

	// Two-column layout: 50/50
	int columnWidth = pageWidth / 2;
	int maxTextWidth = columnWidth - 10 * resolution / 72;  // 10pt margin from logo

	// Calculate logo height first (needed to determine title position)
	int logoHeight = 28 * resolution / 72;  // 28pt tall logo

	// Left column: Project info with text wrapping
	QFont metaFont("Noto Sans", MetadataFontSize, QFont::Normal);
	painter.setFont(metaFont);
	painter.setPen(Qt::black);
	QFontMetrics metaFm = painter.fontMetrics();

	int metaY = MetadataFontSize * resolution / 72;
	int lineSpacing = (MetadataFontSize + 4) * resolution / 72;

	// Helper lambda for drawing wrapped text. Wraps at spaces; an unbreakable
	// word wider than the column is force-broken character-by-character so it
	// can't bleed into the right-hand logo/title column.
	auto drawWrappedText = [&](const QString & text) {
		auto emitLine = [&](const QString & line) {
			painter.drawText(0, metaY, line);
			metaY += lineSpacing;
		};

		// Consume a word that is wider than maxTextWidth, emitting full-width
		// chunks and returning the remainder that still fits.
		auto forceBreakWord = [&](const QString & word) -> QString {
			QString chunk;
			for (QChar ch : word) {
				QString test = chunk + ch;
				if (metaFm.horizontalAdvance(test) > maxTextWidth && !chunk.isEmpty()) {
					emitLine(chunk);
					chunk = ch;
				} else {
					chunk = test;
				}
			}
			return chunk;
		};

		QStringList words = text.split(' ');
		QString currentLine;

		for (const QString & word : words) {
			QString candidate = currentLine.isEmpty() ? word : currentLine + " " + word;
			if (metaFm.horizontalAdvance(candidate) <= maxTextWidth) {
				currentLine = candidate;
				continue;
			}

			if (!currentLine.isEmpty()) {
				emitLine(currentLine);
				currentLine.clear();
			}

			if (metaFm.horizontalAdvance(word) > maxTextWidth) {
				currentLine = forceBreakWord(word);
			} else {
				currentLine = word;
			}
		}
		if (!currentLine.isEmpty()) {
			emitLine(currentLine);
		}
	};

	if (!projectName.isEmpty()) {
		drawWrappedText(tr("Project: %1").arg(projectName));
	}

	QFileInfo fileInfo(sketchFileName);
	QString displayFileName = fileInfo.fileName();
	if (!displayFileName.isEmpty()) {
		drawWrappedText(tr("File: %1").arg(displayFileName));
	}

	QDateTime now = QDateTime::currentDateTime();
	drawWrappedText(tr("Date: %1").arg(now.toString("yyyy-MM-dd hh:mm")));

	if (!projectDescr.isEmpty()) {
		drawWrappedText(tr("Description: %1").arg(projectDescr));
	}

	// Right column: Fritzing logo, right-aligned
	int logoWidth = 0;
	QSvgRenderer svgRenderer(QString(":/resources/images/watermark_fritzing_outline.svg"));
	if (svgRenderer.isValid()) {
		QSizeF svgSize = svgRenderer.defaultSize();
		logoWidth = static_cast<int>(logoHeight * svgSize.width() / svgSize.height());
		int logoX = pageWidth - logoWidth;
		int logoY = 0;
		QRectF logoRect(logoX, logoY, logoWidth, logoHeight);
		svgRenderer.render(&painter, logoRect);
	}

	// "Bill of Materials" title below logo, right-aligned to page edge
	int titleY = logoHeight + 8 * resolution / 72;

	QFont titleFont("OCR-Fritzing-mono", TitleFontSize, QFont::Bold);
	painter.setFont(titleFont);
	QFontMetrics titleFm = painter.fontMetrics();
	QString titleText = tr("Bill of Materials");
	int titleWidth = titleFm.horizontalAdvance(titleText);
	int titleX = pageWidth - titleWidth;  // Right-align title

	painter.setPen(Qt::black);
	painter.drawText(titleX, titleY + TitleFontSize * resolution / 72, titleText);

	// Draw separator line
	int headerHeight = static_cast<int>(HeaderHeightMM * resolution / 25.4);
	painter.setPen(QPen(Qt::gray, 1));
	painter.drawLine(0, headerHeight - 5 * resolution / 72, pageWidth, headerHeight - 5 * resolution / 72);

	// DEBUG: Draw helper lines to visualize layout (only when debug dialog is enabled)
	if (DebugDialog::enabled()) {
		painter.setPen(QPen(Qt::red, 1, Qt::DashLine));
		// Vertical line: left column boundary (50%)
		painter.drawLine(columnWidth, 0, columnWidth, headerHeight);
		// Horizontal line: logo bottom
		painter.drawLine(0, logoHeight, pageWidth, logoHeight);
		// Horizontal line: title baseline
		painter.drawLine(0, titleY + TitleFontSize * resolution / 72, pageWidth, titleY + TitleFontSize * resolution / 72);
		// Vertical line on left: punch hole centering guide (at x=0, full header height)
		painter.setPen(QPen(Qt::blue, 2));
		painter.drawLine(0, 0, 0, headerHeight);
	}
}

int BomPdfGenerator::drawTableSection(QPainter & painter, const QPageLayout & pageLayout,
                                      const QString & sectionTitle,
                                      const QStringList & headers,
                                      const QList<QStringList> & rows,
                                      int startY,
                                      const QString & projectName, const QString & projectVersion,
                                      const QString & sketchFileName, int & pageNumber,
                                      int totalPages)
{
	int resolution = painter.device()->logicalDpiX();
	QRectF paintRect = pageLayout.paintRect(QPageLayout::Point);
	int pageWidth = static_cast<int>(paintRect.width() * resolution / 72);
	int pageHeight = static_cast<int>(paintRect.height() * resolution / 72);

	int footerHeight = FooterHeightPt * resolution / 72;
	int rowHeight = RowHeightPt * resolution / 72;
	int tableHeaderHeight = TableHeaderHeightPt * resolution / 72;

	// Calculate column widths
	QList<double> colPercents;
	colPercents << CheckboxColPercent << LabelQtyColPercent << ValueColPercent
	            << PartTypeColPercent << PackageColPercent << PropertiesColPercent;

	QList<int> colWidths;
	for (double pct : colPercents) {
		colWidths << static_cast<int>(pageWidth * pct);
	}

	int currentY = startY;

	// Draw section header
	QFont sectionFont("OCR-Fritzing-mono", SectionHeaderFontSize, QFont::Bold);
	painter.setFont(sectionFont);
	painter.setPen(Qt::black);
	painter.drawText(0, currentY + SectionHeaderFontSize * resolution / 72, sectionTitle);
	currentY += (SectionHeaderFontSize + SectionTitleSpacingPt) * resolution / 72;

	// Draw table header background
	painter.fillRect(0, currentY, pageWidth, tableHeaderHeight, QColor(240, 240, 240));

	// Draw table header text
	QFont headerFont("Noto Sans", TableHeaderFontSize, QFont::Bold);
	painter.setFont(headerFont);
	painter.setPen(Qt::black);

	int x = 0;
	for (int i = 0; i < headers.size(); i++) {
		int textX = x + 4 * resolution / 72;
		int textY = currentY + (tableHeaderHeight + TableHeaderFontSize * resolution / 72) / 2 - 2 * resolution / 72;
		painter.drawText(textX, textY, headers[i]);
		x += colWidths[i];
	}

	// Draw header border
	painter.setPen(QPen(Qt::gray, 1));
	painter.drawRect(0, currentY, pageWidth, tableHeaderHeight);

	currentY += tableHeaderHeight;

	// Draw table rows
	QFont bodyFont("Noto Sans", TableBodyFontSize, QFont::Normal);
	painter.setFont(bodyFont);
	QFontMetrics bodyFm(bodyFont);

	int checkboxSize = 10 * resolution / 72;

	for (int rowIdx = 0; rowIdx < rows.size(); rowIdx++) {
		// Check if we need a new page
		if (currentY + rowHeight > pageHeight - footerHeight) {
			drawFooter(painter, pageLayout, pageNumber, totalPages, projectName, projectVersion, sketchFileName);

			// Request new page through painter's device
			QPrinter * printer = dynamic_cast<QPrinter*>(painter.device());
			if (printer) {
				printer->newPage();
			}
			pageNumber++;

			// Reset Y position for new page
			currentY = MarginMM * resolution / 25.4;

			// Draw continuation header
			QFont continueFont("OCR-Fritzing-mono", SectionHeaderFontSize - 2, QFont::Normal);
			painter.setFont(continueFont);
			painter.setPen(Qt::black);
			painter.drawText(0, currentY + (SectionHeaderFontSize - 2) * resolution / 72,
			                 tr("%1 (continued)").arg(sectionTitle));
			currentY += SectionHeaderFontSize * resolution / 72;

			// Redraw table header
			painter.fillRect(0, currentY, pageWidth, tableHeaderHeight, QColor(240, 240, 240));

			painter.setFont(headerFont);
			x = 0;
			for (int i = 0; i < headers.size(); i++) {
				int textX = x + 4 * resolution / 72;
				int textY = currentY + (tableHeaderHeight + TableHeaderFontSize * resolution / 72) / 2 - 2 * resolution / 72;
				painter.drawText(textX, textY, headers[i]);
				x += colWidths[i];
			}

			painter.setPen(QPen(Qt::gray, 1));
			painter.drawRect(0, currentY, pageWidth, tableHeaderHeight);
			currentY += tableHeaderHeight;

			painter.setFont(bodyFont);
		}

		// Alternate row background
		if (rowIdx % 2 == 1) {
			painter.fillRect(0, currentY, pageWidth, rowHeight, QColor(250, 250, 250));
		}

		// Draw row content
		const QStringList & rowData = rows[rowIdx];
		x = 0;

		for (int colIdx = 0; colIdx < rowData.size() && colIdx < colWidths.size(); colIdx++) {
			if (colIdx == 0) {
				// Draw checkbox
				int cbX = x + (colWidths[0] - checkboxSize) / 2;
				int cbY = currentY + (rowHeight - checkboxSize) / 2;
				drawCheckbox(painter, cbX, cbY, checkboxSize);
			} else {
				// Draw text with clipping
				int textX = x + 4 * resolution / 72;
				int textY = currentY + (rowHeight + TableBodyFontSize * resolution / 72) / 2 - 2 * resolution / 72;

				QString text = rowData[colIdx];

				// Clip text if too long
				int availableWidth = colWidths[colIdx] - 8 * resolution / 72;
				if (bodyFm.horizontalAdvance(text) > availableWidth) {
					text = bodyFm.elidedText(text, Qt::ElideRight, availableWidth);
				}

				painter.setPen(Qt::black);
				painter.drawText(textX, textY, text);
			}
			x += colWidths[colIdx];
		}

		// Draw row border
		painter.setPen(QPen(QColor(220, 220, 220), 1));
		painter.drawLine(0, currentY + rowHeight, pageWidth, currentY + rowHeight);

		currentY += rowHeight;
	}

	// Draw table border
	painter.setPen(QPen(Qt::gray, 1));
	int tableTop = startY + (SectionHeaderFontSize + SectionTitleSpacingPt) * resolution / 72;
	painter.drawRect(0, tableTop, pageWidth, currentY - tableTop);

	return currentY;
}

void BomPdfGenerator::drawFooter(QPainter & painter, const QPageLayout & pageLayout,
                                 int pageNumber, int totalPages,
                                 const QString & projectName, const QString & projectVersion,
                                 const QString & sketchFileName)
{
	int resolution = painter.device()->logicalDpiX();
	QRectF paintRect = pageLayout.paintRect(QPageLayout::Point);
	int pageWidth = static_cast<int>(paintRect.width() * resolution / 72);
	int pageHeight = static_cast<int>(paintRect.height() * resolution / 72);

	int footerY = pageHeight - FooterFontSize * resolution / 72;

	QFont footerFont("Noto Sans", FooterFontSize, QFont::Normal);
	painter.setFont(footerFont);
	painter.setPen(Qt::gray);
	QFontMetrics fm = painter.fontMetrics();  // Use painter's metrics for correct resolution

	// Left side: Fritzing version with URL
	QString versionStr = QString("Fritzing %1.%2.%3 - https://fritzing.org")
	                         .arg(Version::majorVersion())
	                         .arg(Version::minorVersion())
	                         .arg(Version::minorSubVersion());
	painter.drawText(0, footerY, versionStr);

	// Center: project name + version, or filename without suffix
	QString centerText;
	if (!projectName.isEmpty()) {
		centerText = projectName;
		if (!projectVersion.isEmpty()) {
			centerText += " " + projectVersion;
		}
	} else {
		QFileInfo fileInfo(sketchFileName);
		centerText = fileInfo.completeBaseName();  // filename without suffix
	}

	if (!centerText.isEmpty()) {
		int centerWidth = fm.horizontalAdvance(centerText);
		painter.drawText((pageWidth - centerWidth) / 2, footerY, centerText);
	}

	// Right side: page number with total (right-aligned like title)
	QString pageStr = tr("Page %1/%2").arg(pageNumber).arg(totalPages);
	int pageStrWidth = fm.horizontalAdvance(pageStr);
	painter.drawText(pageWidth - pageStrWidth, footerY, pageStr);
}

int BomPdfGenerator::measureTotalPages(int assemblyRowCount, int shoppingRowCount,
                                       int firstSectionStartY, int pageHeight,
                                       int footerHeight, int marginTop,
                                       int rowHeight, int tableHeaderHeight,
                                       int sectionHeaderHeight, int continuationHeaderHeight,
                                       int sectionSpacing, int minSectionHeight)
{
	// Mirrors drawTableSection's actual page-break logic so the precomputed
	// total agrees with the page numbers stamped into each footer.
	auto walkSection = [&](int startY, int rowCount, int & pageNumber) -> int {
		int currentY = startY + sectionHeaderHeight + tableHeaderHeight;
		for (int i = 0; i < rowCount; i++) {
			if (currentY + rowHeight > pageHeight - footerHeight) {
				pageNumber++;
				currentY = marginTop + continuationHeaderHeight + tableHeaderHeight;
			}
			currentY += rowHeight;
		}
		return currentY;
	};

	int pageNumber = 1;
	int currentY = walkSection(firstSectionStartY, assemblyRowCount, pageNumber);

	currentY += sectionSpacing;

	int availableHeight = pageHeight - currentY - footerHeight;
	if (availableHeight < minSectionHeight) {
		pageNumber++;
		currentY = marginTop;
	}

	walkSection(currentY, shoppingRowCount, pageNumber);

	return pageNumber;
}

void BomPdfGenerator::drawCheckbox(QPainter & painter, int x, int y, int size)
{
	painter.setPen(QPen(Qt::black, 1));
	painter.setBrush(Qt::NoBrush);
	painter.drawRect(x, y, size, size);
}
