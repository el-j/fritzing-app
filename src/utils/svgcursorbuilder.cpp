/*******************************************************************

Part of the Fritzing project - http://fritzing.org
Copyright (c) 2025, 2026 Fritzing GmbH

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

#include "svgcursorbuilder.h"
#include "../debugdialog.h"

#include <QApplication>
#include <QFile>
#include <QByteArray>
#include <QHash>
#include <QXmlStreamReader>
#include <QSvgRenderer>
#include <QPainter>
#include <QGuiApplication>
#include <QScreen>
#include <QRegularExpression>
#include <QKeyEvent>
#include <QEvent>
#include <QDebug>
#ifdef Q_OS_WIN
#include <QVersionNumber>
#endif

// Static cursor pointers
QCursor * SvgCursorBuilder::BendpointCursor = nullptr;
QCursor * SvgCursorBuilder::NewBendpointCursor = nullptr;
QCursor * SvgCursorBuilder::MakeWireCursor = nullptr;
QCursor * SvgCursorBuilder::MakeCurveCursor = nullptr;
QCursor * SvgCursorBuilder::RubberbandCursor = nullptr;
QCursor * SvgCursorBuilder::RotateCursor = nullptr;
QCursor * SvgCursorBuilder::SpotFaceConnectCursor = nullptr;
QCursor * SvgCursorBuilder::SpotFaceCutterCursor = nullptr;

SvgCursorBuilder SvgCursorBuilder::TheSvgCursorBuilder;
static QList<QObject *> Listeners;
static QList<QCursor **> Cursors;

// Sources for rebuilding the managed cursors at a different scale factor,
// needed by the Windows override-cursor workaround (see overrideSafeCursor()).
struct CursorSource {
	QString svgPath;      // empty -> scale basePixmap instead of re-rendering
	QPixmap basePixmap;
	QPoint baseHotspot;   // in device-independent pixels
};
static QList<CursorSource> CursorSources;
static QHash<qint64, int> CursorPixmapKeys;   // QPixmap::cacheKey() -> index into CursorSources

#ifdef Q_OS_WIN
struct CompensatedCursor {
	QCursor cursor;
	qreal targetFactor = 0;
	qreal applyFactor = 0;
};
static QHash<int, CompensatedCursor> CompensatedCursors;
#endif

static void registerCursorSource(const QCursor & cursor, const QString & svgPath, const QPixmap & basePixmap)
{
	if (cursor.pixmap().isNull()) return;   // cursor creation failed, fell back to a shape cursor

	CursorSources.append({svgPath, basePixmap, cursor.hotSpot()});
	CursorPixmapKeys.insert(cursor.pixmap().cacheKey(), CursorSources.count() - 1);
}

// QTBUG-132709 (fixed in Qt 6.12): on Windows, QGuiApplication::setOverrideCursor()
// builds one native cursor per screen and calls SetCursor() each time, so the
// cursor built for the *last* screen in QGuiApplication::screens() is the one
// that stays visible. With mixed scale factors it is sized for the wrong screen.
// Compensate by re-rendering the cursor for the screen under the pointer while
// tagging it with the device pixel ratio of the screen Qt builds the visible
// native cursor with; the two factors then cancel out to the correct pixel size.
static QCursor overrideSafeCursor(const QCursor & cursor)
{
#ifdef Q_OS_WIN
	static const bool qtHandlesOverrideScaling =
		QVersionNumber::fromString(qVersion()) >= QVersionNumber(6, 12, 0);
	if (qtHandlesOverrideScaling) return cursor;

	const QList<QScreen *> screens = QGuiApplication::screens();
	if (screens.count() < 2) return cursor;

	QScreen * pointerScreen = QGuiApplication::screenAt(QCursor::pos());
	if (pointerScreen == nullptr) pointerScreen = QGuiApplication::primaryScreen();
	if (pointerScreen == nullptr) return cursor;
	const qreal targetFactor = pointerScreen->devicePixelRatio();
	const qreal applyFactor = screens.last()->devicePixelRatio();
	if (qFuzzyCompare(targetFactor, applyFactor)) return cursor;

	const QPixmap pixmap = cursor.pixmap();
	if (pixmap.isNull()) return cursor;   // shape cursors are handled natively
	auto it = CursorPixmapKeys.constFind(pixmap.cacheKey());
	if (it == CursorPixmapKeys.constEnd()) return cursor;
	int index = it.value();

	CompensatedCursor & comp = CompensatedCursors[index];
	if (!comp.cursor.pixmap().isNull()
	    && qFuzzyCompare(comp.targetFactor, targetFactor)
	    && qFuzzyCompare(comp.applyFactor, applyFactor)) {
		return comp.cursor;
	}

	const CursorSource & source = CursorSources.at(index);
	QPixmap scaled;
	QPoint hotspot = source.baseHotspot;
	if (!source.svgPath.isEmpty()) {
		CursorInfo info = SvgCursorBuilder::loadCursorFromSvg(source.svgPath, 0, targetFactor);
		if (!info.valid) return cursor;
		scaled = info.pixmap;
		hotspot = QPoint(info.hotspotX, info.hotspotY);
	} else {
		QSizeF baseSize = QSizeF(source.basePixmap.size()) / source.basePixmap.devicePixelRatio();
		scaled = source.basePixmap.scaled((baseSize * targetFactor).toSize(),
			Qt::KeepAspectRatio, Qt::SmoothTransformation);
	}
	// The platform multiplies the hotspot by the scale factor of the screen it
	// builds the cursor for, so pre-divide by that and scale to the pointer screen.
	scaled.setDevicePixelRatio(applyFactor);
	hotspot = QPoint(qRound(hotspot.x() * targetFactor / applyFactor),
	                 qRound(hotspot.y() * targetFactor / applyFactor));

	comp.cursor = QCursor(scaled, hotspot.x(), hotspot.y());
	comp.targetFactor = targetFactor;
	comp.applyFactor = applyFactor;
	return comp.cursor;
#else
	return cursor;
#endif
}

SvgCursorBuilder::SvgCursorBuilder() : QObject()
{
	m_blocked = false;
}

SvgCursorBuilder * SvgCursorBuilder::instance()
{
	return &TheSvgCursorBuilder;
}

void SvgCursorBuilder::cleanup() {
	for (QCursor ** cursor : Cursors) {
		delete *cursor;
	}
	Cursors.clear();
	CursorSources.clear();
	CursorPixmapKeys.clear();
#ifdef Q_OS_WIN
	CompensatedCursors.clear();
#endif

	// Clean up PNG cursors loaded separately
	delete RubberbandCursor;
	RubberbandCursor = nullptr;
}

void SvgCursorBuilder::initCursors()
{
	if (BendpointCursor == nullptr) {
		QStringList names;

		// All cursors are now loaded from SVG files
		names << ":resources/images/cursor/bendpoint.svg"
			  << ":resources/images/cursor/new_bendpoint.svg"
			  << ":resources/images/cursor/make_wire.svg"
			  << ":resources/images/cursor/curve.svg"
			  << ":resources/images/cursor/rotate.svg"
			  << ":resources/images/cursor/spot_face_connect.svg"
			  << ":resources/images/cursor/spot_face_cutter.svg";

		Cursors << &BendpointCursor
		        << &NewBendpointCursor
		        << &MakeWireCursor
		        << &MakeCurveCursor
		        << &RotateCursor
		        << &SpotFaceConnectCursor
		        << &SpotFaceCutterCursor;

		for (int i = 0; i < Cursors.count(); i++) {
			*Cursors.at(i) = new QCursor(createCursor(names.at(i)));
			registerCursorSource(**Cursors.at(i), names.at(i), QPixmap());
		}

		// Load rubberband cursor from PNG (temporary until we have SVG version)
		QPixmap rubberbandPixmap(":resources/images/cursor/rubberband_move.png");
		RubberbandCursor = new QCursor(rubberbandPixmap, 0, 0);
		registerCursorSource(*RubberbandCursor, QString(), rubberbandPixmap);

		QApplication::instance()->installEventFilter(instance());
	}
}

CursorInfo SvgCursorBuilder::loadCursorFromSvg(const QString& svgPath, int requestedSize, qreal scale) {
	CursorInfo info;

	// Open and read the whole SVG file. We keep the raw bytes so that, on
	// macOS, we can recolor them before handing them to the renderer.
	QFile file(svgPath);
	if (!file.open(QIODevice::ReadOnly)) {
		info.error = QString("Cannot open file: %1").arg(svgPath);
		return info;
	}
	QByteArray svgData = file.readAll();
	file.close();

	// The cursor SVGs declare their colors as CSS custom properties with
	// fallbacks - var(--cursor-body, #fff) / var(--cursor-outline, #000).
	// Qt's SVG renderer can't resolve var(), so we resolve them here on every
	// platform. With no overrides each var() falls back to its inline default
	// (the standard white-body / black-outline look).
	QHash<QString, QString> cursorColors;
#ifdef Q_OS_MACOS
	// macOS cursors are black with a white halo - the inverse scheme.
	cursorColors.insert(QStringLiteral("--cursor-body"), QStringLiteral("#000"));
	cursorColors.insert(QStringLiteral("--cursor-outline"), QStringLiteral("#fff"));
#endif
	resolveSvgVariables(svgData, cursorColors);

	// Parse the SVG XML to extract attributes
	QXmlStreamReader xml(svgData);

	int svgWidth = 0;
	int svgHeight = 0;
	bool hasHotspotX = false;
	bool hasHotspotY = false;

	while (!xml.atEnd()) {
		xml.readNext();

		if (xml.isStartElement() && xml.name() == u"svg") {
			QXmlStreamAttributes attrs = xml.attributes();

			// Extract width attribute
			if (attrs.hasAttribute("width")) {
				QString widthStr = attrs.value("width").toString();
				// Remove units (px, pt, etc.) - just keep the number
				widthStr.remove(QRegularExpression("[^0-9.]"));
				svgWidth = widthStr.toInt();
			}

			// Extract height attribute
			if (attrs.hasAttribute("height")) {
				QString heightStr = attrs.value("height").toString();
				// Remove units (px, pt, etc.) - just keep the number
				heightStr.remove(QRegularExpression("[^0-9.]"));
				svgHeight = heightStr.toInt();
			}

			// Extract fritzing namespace attributes
			for (const QXmlStreamAttribute& attr : attrs) {
				if (attr.namespaceUri() == QLatin1String(FRITZING_SVG_NS)) {
					if (attr.name() == u"hotspot-x") {
						info.hotspotX = attr.value().toInt();
						hasHotspotX = true;
					} else if (attr.name() == u"hotspot-y") {
						info.hotspotY = attr.value().toInt();
						hasHotspotY = true;
					}
				}
			}
			break; // We only need the root <svg> element
		}
	}

	// Check for XML parsing errors
	if (xml.hasError()) {
		info.error = QString("XML parsing error: %1").arg(xml.errorString());
		return info;
	}

	// Validate required attributes
	if (svgWidth == 0 || svgHeight == 0) {
		info.error = "SVG must have width and height attributes (with numeric values)";
		return info;
	}

	if (!hasHotspotX || !hasHotspotY) {
		info.error = QString("SVG must have %1:hotspot-x and %1:hotspot-y attributes")
			.arg("fritzing");
		return info;
	}

	// Determine the actual size to render
	int baseSize = (requestedSize > 0) ? requestedSize : svgWidth;

	// Scale hotspot proportionally if the requested size differs from SVG width
	if (svgWidth != baseSize) {
		qreal scaleFactor = static_cast<qreal>(baseSize) / svgWidth;
		info.hotspotX = static_cast<int>(info.hotspotX * scaleFactor);
		info.hotspotY = static_cast<int>(info.hotspotY * scaleFactor);
	}

	// Render the SVG to a pixmap (from the possibly-recolored bytes)
	QSvgRenderer renderer(svgData);
	if (!renderer.isValid()) {
		info.error = "Failed to load SVG for rendering";
		return info;
	}

	int scaledSize = static_cast<int>(baseSize * scale);
	QPixmap pixmap(scaledSize, scaledSize);
	pixmap.fill(Qt::transparent);

	QPainter painter(&pixmap);
	renderer.render(&painter);
	painter.end();

	// Set the device pixel ratio for HiDPI support
	pixmap.setDevicePixelRatio(scale);

	info.pixmap = pixmap;
	info.valid = true;

	return info;
}

void SvgCursorBuilder::resolveSvgVariables(QByteArray & svgData, const QHash<QString, QString> & values) {
	QString svg = QString::fromUtf8(svgData);

	// Matches a CSS custom-property reference: var(--name) or
	// var(--name, fallback). Group 1 is the name, group 2 the (optional)
	// fallback.
	static const QRegularExpression varRef(
		QStringLiteral("var\\(\\s*(--[A-Za-z0-9_-]+)\\s*(?:,\\s*([^)]*?))?\\s*\\)"));

	QString out;
	out.reserve(svg.size());
	qsizetype last = 0;
	QRegularExpressionMatchIterator it = varRef.globalMatch(svg);
	while (it.hasNext()) {
		QRegularExpressionMatch m = it.next();
		out += svg.mid(last, m.capturedStart() - last);
		const QString name = m.captured(1);
		// Use the override if present, otherwise the inline fallback.
		out += values.value(name, m.captured(2).trimmed());
		last = m.capturedEnd();
	}
	out += svg.mid(last);

	svgData = out.toUtf8();
}

QCursor SvgCursorBuilder::createCursor(const QString& svgPath, int baseSize) {
	// Get the primary screen's device pixel ratio for HiDPI support
	qreal scale = qApp->primaryScreen()->devicePixelRatio();

	CursorInfo info = loadCursorFromSvg(svgPath, baseSize, scale);

	if (!info.valid) {
		qWarning() << "Failed to load cursor from" << svgPath << ":" << info.error;
		return QCursor(Qt::ArrowCursor);
	}

	return QCursor(info.pixmap, info.hotspotX, info.hotspotY);
}

void SvgCursorBuilder::addCursor(QObject * object, const QCursor & cursor)
{
	if (m_blocked) return;

	if (object == nullptr) return;

	const QCursor effectiveCursor = overrideSafeCursor(cursor);

	if (Listeners.contains(object)) {
		if (Listeners.first() != object) {
			Listeners.removeOne(object);
			Listeners.push_front(object);
		}
		QApplication::changeOverrideCursor(effectiveCursor);
		return;
	}

	Listeners.push_front(object);
	connect(object, SIGNAL(destroyed(QObject *)), this, SLOT(deleteCursor(QObject *)));
	QApplication::setOverrideCursor(effectiveCursor);
}

void SvgCursorBuilder::removeCursor(QObject * object)
{
	if (object == nullptr) return;

	if (Listeners.contains(object)) {
		disconnect(object, SIGNAL(destroyed(QObject *)), this, SLOT(deleteCursor(QObject *)));
		Listeners.removeOne(object);
		QApplication::restoreOverrideCursor();
	}
}

void SvgCursorBuilder::deleteCursor(QObject * object)
{
	removeCursor(object);
}

bool SvgCursorBuilder::eventFilter(QObject * object, QEvent * event)
{
	Q_UNUSED(object);

	switch (event->type()) {
	case QEvent::KeyPress:
	case QEvent::KeyRelease:
	{
		auto *keyEvent = static_cast<QKeyEvent*>(event);
		for (QObject * listener : Listeners) {
			if (listener != nullptr) {
				dynamic_cast<CursorKeyListener *>(listener)->cursorKeyEvent(keyEvent->modifiers());
				break;
			}
		}
	}
	break;
	default:
		break;
	}

	return false;
}

void SvgCursorBuilder::block() {
	m_blocked = true;
}

void SvgCursorBuilder::unblock() {
	m_blocked = false;
}
