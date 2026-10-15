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

#ifndef SVGCURSORBUILDER_H
#define SVGCURSORBUILDER_H

#include <QString>
#include <QByteArray>
#include <QHash>
#include <QPixmap>
#include <QCursor>
#include <QObject>

/**
 * @brief Interface for objects that want to receive cursor key events
 */
class CursorKeyListener
{
public:
	virtual void cursorKeyEvent(Qt::KeyboardModifiers) = 0;
};

/**
 * @brief Structure to hold cursor information loaded from SVG
 */
struct CursorInfo {
	QPixmap pixmap;
	int hotspotX = 0;
	int hotspotY = 0;
	bool valid = false;
	QString error;
};

/**
 * @brief SVG-based cursor builder and manager for Fritzing
 *
 * This singleton class replaces the old CursorMaster and provides:
 * 1. SVG cursor loading with Fritzing namespace extensions (fritzing:hotspot-x/y)
 * 2. Cursor stack management for context-sensitive cursors
 * 3. Keyboard event filtering for cursor listeners
 * 4. Standard Fritzing cursors (bendpoint, wire, rotate, etc.)
 *
 * SVG Cursor Format:
 * @code
 * <svg xmlns="http://www.w3.org/2000/svg"
 *      xmlns:fritzing="https://fritzing.org/svg/1.0"
 *      width="32" height="32"
 *      viewBox="0 0 32 32"
 *      fritzing:hotspot-x="8"
 *      fritzing:hotspot-y="8">
 *   <path d="..."/>
 * </svg>
 * @endcode
 *
 * Usage:
 * @code
 * // Add a cursor (pushes onto stack)
 * SvgCursorBuilder::instance()->addCursor(this, *SvgCursorBuilder::BendpointCursor);
 *
 * // Remove cursor (pops from stack)
 * SvgCursorBuilder::instance()->removeCursor(this);
 * @endcode
 */
class SvgCursorBuilder : public QObject {
	Q_OBJECT

protected:
	SvgCursorBuilder();

public:
	/**
	 * @brief The Fritzing SVG namespace URI
	 */
	static constexpr const char* FRITZING_SVG_NS = "https://fritzing.org/svg/1.0";

	/**
	 * @brief Get the singleton instance
	 */
	static SvgCursorBuilder * instance();

	/**
	 * @brief Initialize all standard Fritzing cursors
	 *
	 * Must be called once during application startup.
	 * Loads all cursor SVG files and creates QCursor objects.
	 */
	static void initCursors();

	/**
	 * @brief Clean up cursor resources
	 *
	 * Called during application shutdown to free cursor memory.
	 */
	static void cleanup();

	/**
	 * @brief Load cursor information from an SVG file
	 *
	 * @param svgPath Path to SVG file (can be Qt resource path like ":resources/...")
	 * @param requestedSize Desired cursor size in pixels (0 = use SVG's native width)
	 * @param scale HiDPI scale factor (usually from QScreen::devicePixelRatio())
	 * @return CursorInfo structure with loaded pixmap and hotspot, or error info
	 */
	static CursorInfo loadCursorFromSvg(const QString& svgPath, int requestedSize = 0, qreal scale = 1.0);

	/**
	 * @brief Create a QCursor from an SVG file
	 *
	 * @param svgPath Path to SVG file (can be Qt resource path)
	 * @param baseSize Desired cursor size in pixels (0 = use SVG's native width)
	 * @return QCursor object, or default arrow cursor on error
	 *
	 * Automatically handles HiDPI scaling using the primary screen's
	 * device pixel ratio.
	 */
	static QCursor createCursor(const QString& svgPath, int baseSize = 0);

	/**
	 * @brief Resolve CSS custom properties - var(--name, fallback) - in raw
	 *        SVG data.
	 *
	 * Qt's SVG renderer does not understand CSS custom properties, so the
	 * cursor SVGs declare their themeable colors as var(--name, fallback)
	 * and we resolve them here before rendering. Each var() reference is
	 * replaced with the value from @p values for its name, or with its inline
	 * fallback when the name is not in the map. This keeps the raw SVGs valid
	 * and correctly themed in standard SVG viewers (browsers, Illustrator).
	 *
	 * The cursors declare:
	 *   --cursor-body     the ".cls-1" body fill  (fallback #fff)
	 *   --cursor-outline  the outline fill         (fallback #000)
	 * On macOS these are flipped to #000 / #fff for the native black cursor
	 * with a white halo; elsewhere the fallbacks give the white body / black
	 * outline look.
	 *
	 * @param svgData Raw SVG bytes, modified in place.
	 * @param values  Custom-property name -> value overrides. An empty map
	 *                leaves every var() at its inline fallback.
	 */
	static void resolveSvgVariables(QByteArray & svgData, const QHash<QString, QString> & values);

	/**
	 * @brief Add a cursor to the stack
	 *
	 * Pushes the cursor onto the cursor stack and makes it active.
	 * If the same listener adds a cursor again, the cursor is updated.
	 *
	 * @param listener The object that owns this cursor
	 * @param cursor The cursor to display
	 */
	void addCursor(QObject * listener, const QCursor & cursor);

	/**
	 * @brief Remove a cursor from the stack
	 *
	 * Pops the cursor and restores the previous one.
	 *
	 * @param listener The object that added the cursor
	 */
	void removeCursor(QObject * listener);

	/**
	 * @brief Block cursor changes
	 *
	 * Prevents any cursor changes until unblock() is called.
	 */
	void block();

	/**
	 * @brief Unblock cursor changes
	 *
	 * Re-enables cursor changes after block() was called.
	 */
	void unblock();

protected:
	bool eventFilter(QObject *obj, QEvent *event);

protected Q_SLOTS:
	void deleteCursor(QObject *);

public:
	// Standard Fritzing cursors (loaded from SVG files during initCursors())
	static QCursor * BendpointCursor;
	static QCursor * NewBendpointCursor;
	static QCursor * MakeWireCursor;
	static QCursor * MakeCurveCursor;
	static QCursor * RubberbandCursor;
	static QCursor * RotateCursor;
	static QCursor * SpotFaceConnectCursor;
	static QCursor * SpotFaceCutterCursor;

protected:
	static SvgCursorBuilder TheSvgCursorBuilder;
	bool m_blocked;
};

#endif // SVGCURSORBUILDER_H
