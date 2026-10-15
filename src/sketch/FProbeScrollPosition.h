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

#ifndef FPROBESCROLLPOSITION_H
#define FPROBESCROLLPOSITION_H

#include "testing/FProbe.h"

#include "sketchwidget.h"

#include <QString>
#include <QVariant>

/**
 * FProbeScrollPosition - Read scroll position and zoom state from the current view.
 *
 * Returns a JSON object with:
 * - hValue: horizontal scrollbar value
 * - vValue: vertical scrollbar value
 * - hMin/hMax: horizontal scrollbar range
 * - vMin/vMax: vertical scrollbar range
 * - zoom: current zoom level (percentage)
 * - viewType: current view type (breadboard, schematic, pcb)
 * - sceneRect: current scene rectangle
 * - viewportRect: current viewport rectangle in scene coordinates
 *
 * This probe is useful for debugging and stabilizing screenshot tests,
 * as it allows tracking the exact viewport state after fitInWindow operations.
 */
class FProbeScrollPosition : public FProbe {
public:
	/**
	 * Create a scroll position probe with an optional view suffix.
	 * @param sketchWidget The sketch widget to monitor
	 * @param viewSuffix Optional suffix (e.g., "Breadboard", "Schematic", "PCB")
	 *                   Creates probe name "ScrollPosition" or "ScrollPosition_<suffix>"
	 */
	FProbeScrollPosition(SketchWidget * sketchWidget, const QString& viewSuffix = QString());
	~FProbeScrollPosition();

	QVariant read();
	void write(QVariant value);

private:
	SketchWidget * m_sketchWidget;
};

#endif
