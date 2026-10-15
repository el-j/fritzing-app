/*******************************************************************

Part of the Fritzing project - http://fritzing.org
Copyright (c) 2007-2019 Fritzing

Fritzing is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or

Fritzing is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Fritzing.  If not, see <http://www.gnu.org/licenses/>.

********************************************************************/

#include <QSizeGrip>
#include <QStatusBar>
#include <QtDebug>
#include <QApplication>

#include "mainwindow.h"
#include "fdockwidget.h"
#include "debugdialog.h"

#include "dock/layerpalette.h"
#include "infoview/htmlinfoview.h"
#include "partsbinpalette/binmanager/binmanager.h"
#include "utils/fileprogressdialog.h"
#include "utils/fsizegrip.h"
#include "utils/misc.h"


/////////////////////////////////////

static constexpr int PartsBinMinHeight = 100;
static constexpr int UndoHistoryDefaultHeight = 70;
static constexpr int UndoHistoryMinHeight = UndoHistoryDefaultHeight;
const int MainWindow::DockMinWidth = 130;
const int MainWindow::DockMinHeight = 30;

///////////////////////////////////////

void MainWindow::dockChangeActivation(bool activate, QWidget * originator) {
	Q_UNUSED(activate);
	Q_UNUSED(originator);
	if (!m_closing) {
		m_sizeGrip->rearrange();
	}

}

void MainWindow::createDockWindows()
{
	auto * widget = new QWidget();
	widget->setMinimumHeight(0);
	widget->setMaximumHeight(0);

	makeDock(BinManager::Title, m_binManager, PartsBinMinHeight, PartsBinHeightDefault/*, Qt::LeftDockWidgetArea*/);

	makeDock(tr("Inspector", "dock widget title"), m_infoView, InfoViewMinHeight, InfoViewHeightDefault);

	makeDock(tr("Undo History", "dock widget title"), m_undoView, UndoHistoryMinHeight, UndoHistoryDefaultHeight)->hide();
	m_undoView->setMinimumSize(DockMinWidth, UndoHistoryMinHeight);

	//: Title of the view-layers visibility palette (show/hide display layers).
	//: "Layers" in the graphics sense — not the copper stackup.
	makeDock(tr("Layers", "dock widget title"), m_layerPalette, DockMinWidth, DockMinHeight)->hide();
	m_layerPalette->setMinimumSize(DockMinWidth, DockMinHeight);
	m_layerPalette->setShowAllLayersAction(m_showAllLayersAct);
	m_layerPalette->setHideAllLayersAction(m_hideAllLayersAct);

	if (m_programView == nullptr) {
		m_windowMenu->addSeparator();
		m_windowMenu->addAction(m_openProgramWindowAct);
	}

#ifndef QT_NO_DEBUG
	m_windowMenu->addSeparator();
	m_windowMenu->addAction(m_toggleDebuggerOutputAct);
#endif

	m_windowMenuSeparator = m_windowMenu->addSeparator();
}

FDockWidget * MainWindow::makeDock(const QString & title, QWidget * widget, int dockMinHeight, int dockDefaultHeight, Qt::DockWidgetArea area, DockFactory dockFactory) {
	FDockWidget * dock = ((dockFactory) != nullptr ? dockFactory(title, this) : new FDockWidget(title, this));
	dock->setObjectName(title);
	dock->setWidget(widget);
	widget->setParent(dock);
	widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

	connect(dock, SIGNAL(positionChanged()), this, SLOT(keepMargins()));
	connect(dock, SIGNAL(topLevelChanged(bool)), this, SLOT(keepMargins()));
	connect(dock, SIGNAL(visibilityChanged(bool)), this, SLOT(keepMargins()));

	return dockIt(dock, dockMinHeight, dockDefaultHeight, area);
}

FDockWidget *MainWindow::dockIt(FDockWidget* dock, int dockMinHeight, int dockDefaultHeight, Qt::DockWidgetArea area) {
	dock->setAllowedAreas(area);
	addDockWidget(area, dock);
	if (m_windowMenu != nullptr) {
		QString title = dock->windowTitle();
		if (title.isEmpty()) {
			title = "Dock Widget";  // Fallback title
		}

		QAction *tristateAction = new QAction(title, this);
		tristateAction->setCheckable(true);
		tristateAction->setProperty("dockTristate", true);
		
		// Track tristate in action data: 0=unchecked, 1=partial, 2=checked
		int initialState = 0;
		if (dock->isVisible()) {
			initialState = dock->isFloating() ? 2 : 1;
		}
		tristateAction->setData(initialState);
		tristateAction->setChecked(initialState > 0);
		
		// Helper function to update action visual state and status tip
		auto updateActionState = [tristateAction, title](int state) {
			tristateAction->setData(state);
			
			// Create status tip with current state first in the cycling sequence
			QString sequence;
			switch (state) {
			case 0: // Unchecked - invisible (no icon, unchecked)
				tristateAction->setChecked(false);
				tristateAction->setIcon(QIcon());
				sequence = tr("Hidden → Docked → Floating", "dock widget state cycle sequence starting from hidden");
				break;
			case 1: // Partial - visible and docked (no icon, checked - uses default checkbox)
				tristateAction->setChecked(true);
				tristateAction->setIcon(QIcon());
				sequence = tr("Docked → Floating → Hidden", "dock widget state cycle sequence starting from docked");
				break;
			case 2: // Checked - visible and floating (float icon)
				tristateAction->setChecked(true);
				tristateAction->setIcon(QIcon(":/resources/images/icons/dockWidgetFloatNormal_icon.png"));
				sequence = tr("Floating → Hidden → Docked", "dock widget state cycle sequence starting from floating");
				break;
			}
			
			QString statusTip = tr("%1 - Click to cycle: %2", "dock widget status tip: %1=dock name, %2=cycle sequence")
									.arg(title)
									.arg(sequence);
			tristateAction->setStatusTip(statusTip);
		};
		
		updateActionState(initialState);

		// Handle tristate cycling on click
		connect(tristateAction, &QAction::triggered, [dock, updateActionState, tristateAction]() {
			int currentState = tristateAction->data().toInt();
			int nextState = (currentState + 1) % 3;  // Cycle 0->1->2->0
			
			switch (nextState) {
			case 0: // Unchecked - invisible
				dock->setVisible(false);
				break;
			case 1: // Partial - visible and docked
				dock->setVisible(true);
				dock->setFloating(false);
				break;
			case 2: // Checked - visible and floating
				dock->setVisible(true);
				dock->setFloating(true);
				break;
			}
			
			updateActionState(nextState);
		});
		
		// Update action when dock state changes externally
		connect(dock, &QDockWidget::visibilityChanged, [updateActionState, dock](bool visible) {
			int newState = 0;
			if (visible) {
				newState = dock->isFloating() ? 2 : 1;
			}
			updateActionState(newState);
		});
		
		connect(dock, &QDockWidget::topLevelChanged, [updateActionState](bool floating) {
			int newState = floating ? 2 : 1;
			updateActionState(newState);
		});
		
		m_windowMenu->addAction(tristateAction);
	}

	dock->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	dock->setMinimumSize(DockMinWidth, dockMinHeight);
	dock->resize(DockWidthDefault, dockDefaultHeight);
	connect(dock, SIGNAL(dockChangeActivationSignal(bool, QWidget *)), this, SLOT(dockChangeActivation(bool, QWidget *)));
	connect(dock, SIGNAL(destroyed(QObject *)), qApp, SLOT(topLevelWidgetDestroyed(QObject *)));
	connect(dock, SIGNAL(dockChangeActivationSignal(bool, QWidget *)), qApp, SLOT(changeActivation(bool, QWidget *)), Qt::DirectConnection);

	m_docks << dock;

	return dock;
}

FDockWidget *MainWindow::newTopWidget() {
	int topMostY = 10000;
	FDockWidget *topWidget = nullptr;
	Q_FOREACH(FDockWidget* dock, m_docks) {
		if(/*!dock->isFloating() && dock->isVisible() &&*/
		    dockWidgetArea(dock) == Qt::RightDockWidgetArea
		    && dock->pos().y() < topMostY) {
			topMostY = dock->pos().y();
			topWidget = dock;
		}
	}
	return topWidget;
}

FDockWidget *MainWindow::newBottomWidget() {
	int bottomMostY = -1;
	FDockWidget *bottomWidget = nullptr;
	Q_FOREACH(FDockWidget* dock, m_docks) {
		if(!dock->isFloating() && dock->isVisible() &&
		        dockWidgetArea(dock) == Qt::RightDockWidgetArea
		        && dock->pos().y() > bottomMostY) {
			bottomMostY = dock->pos().y();
			bottomWidget = dock;
		}
	}
	return bottomWidget;
}

void MainWindow::keepMargins() {
	if (m_dontKeepMargins) return;

	/*FDockWidget* newTopWidget = this->newTopWidget();
	if(m_topDock != newTopWidget) {
		removeMargin(m_topDock);
		m_topDock = newTopWidget;
		if(m_topDock) m_oldTopDockStyle = m_topDock->styleSheet();
		addTopMargin(m_topDock);
	}*/

	FDockWidget* newBottomWidget = this->newBottomWidget();
	if(m_bottomDock != newBottomWidget) {
		removeMargin(m_bottomDock);
		m_bottomDock = newBottomWidget;
		if(m_bottomDock != nullptr) m_oldBottomDockStyle = m_bottomDock->styleSheet();
		addBottomMargin(m_bottomDock);
		m_sizeGrip->raise();
	}
}


void MainWindow::removeMargin(FDockWidget* dock) {
	if(dock != nullptr) {
		dockMarginAux(dock, "", m_oldBottomDockStyle);
	}
}

void MainWindow::addTopMargin(FDockWidget* dock) {
	if(dock != nullptr) dockMarginAux(dock, "topMostDock", dock->widget()->styleSheet());
}

void MainWindow::addBottomMargin(FDockWidget* dock) {
	if(dock != nullptr) {
		if(qobject_cast<BinManager*>(dock->widget()) != nullptr) {
			// already has enough space
		} else {
			dockMarginAux(dock, "bottomMostDock", dock->widget()->styleSheet());
		}
	}
}


void MainWindow::dockMarginAux(FDockWidget* dock, const QString &name, const QString &style) {
	if(dock != nullptr) {
		dock->widget()->setObjectName(name);
		dock->widget()->setStyleSheet(style);
		dock->setStyleSheet(dock->styleSheet());
	} else {
		DebugDialog::debug("Couldn't get the dock widget", DebugDialog::Warning);
	}

}

void MainWindow::dontKeepMargins() {
	m_dontKeepMargins = true;
}

void MainWindow::initDock() {
	m_layerPalette = new LayerPalette(this);

	m_infoView = new HtmlInfoView();
	m_infoView->init(false);
	connect(m_infoView, SIGNAL(clickObsoleteSignal(qint64)), this, SLOT(migrateObsoletePart(qint64)));

	if (m_fileProgressDialog != nullptr) {
		m_fileProgressDialog->setValue(49);
	}

	//DebugDialog::debug("after html view");

	m_binManager = new BinManager(m_referenceModel, m_infoView, m_undoStack, this);
	m_binManager->initStandardBins();

	//DebugDialog::debug("after creating bins");
	if (m_fileProgressDialog != nullptr) {
		m_fileProgressDialog->setValue(89);
	}
}

void MainWindow::moreInitDock() {
	//DebugDialog::debug("create view switcher");

	createDockWindows();

	if (m_fileProgressDialog != nullptr) {
		m_fileProgressDialog->setValue(93);
	}
}


void MainWindow::saveDocks()
{
	for (int i = 0; i < children().count(); i++) {
		auto * dock = qobject_cast<FDockWidget *>(children()[i]);
		if (dock == nullptr) continue;

		//DebugDialog::debug(QString("saving dock %1").arg(dock->windowTitle()));
		dock->saveState();

		if (dock->isFloating() && dock->isVisible()) {
			//DebugDialog::debug(QString("hiding dock %1").arg(dock->windowTitle()));
			dock->hide();
		}
	}
}

void MainWindow::restoreDocks() {
	for (int i = 0; i < children().count(); i++) {
		auto * dock = qobject_cast<FDockWidget *>(children()[i]);
		if (dock == nullptr) continue;

		// DebugDialog::debug(QString("restoring dock %1").arg(dock->windowTitle()));
		dock->restoreState();
	}
}
