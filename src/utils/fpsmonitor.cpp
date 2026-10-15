#include "fpsmonitor.h"
#include <QGuiApplication>
#include <QOpenGLContext>
#include <QOpenGLDebugLogger>
#include <QOpenGLFunctions>
#include <QScreen>
#include <QOffscreenSurface>
#include <QGraphicsView>
#include "utils/fmessagebox.h"
#include "debugdialog.h"
#include <algorithm>

FPSMonitor::FPSMonitor(QObject *parent)
	: QObject(parent)
	, totalFrameCount(0)
	, lastFrameFPS(0)
	, showFPS(true)
{
	checkOpenGLAvailability();
	totalTimeTimer.start();
	frameTimer.start();
}

bool FPSMonitor::checkOpenGLAvailability() {
	QOpenGLContext testContext;
	bool openGLAvailable = testContext.create();

	if (openGLAvailable) {
		// We need a surface to make the context current
		QOffscreenSurface surface;
		surface.create();

		if (testContext.makeCurrent(&surface)) {
			QOpenGLFunctions *f = testContext.functions();
			const GLubyte* version = f->glGetString(GL_VERSION);
			DebugDialog::stream() << "OpenGL Version:" << QString::fromLatin1(reinterpret_cast<const char*>(version));
			testContext.doneCurrent();
		} else {
			openGLAvailable = false;
			DebugDialog::stream(DebugDialog::Warning) << "Failed to make OpenGL context current";
		}

		surface.destroy();
	} else {
		DebugDialog::stream(DebugDialog::Warning) << "Failed to create OpenGL context";
	}

	return openGLAvailable;
}

void FPSMonitor::update()
{
	qreal frameTime = frameTimer.restart() / 1000.0; // Convert to seconds
	if (frameTime > 0) {
		lastFrameFPS = 1.0 / frameTime;
		renderTimes.append(frameTime);
	}
	totalFrameCount++;
}

void FPSMonitor::reset()
{
	totalTimeTimer.restart();
	frameTimer.restart();
	renderTimes.clear();
	totalFrameCount = 0;
	lastFrameFPS = 0;
}

qreal FPSMonitor::getLastFrameFPS() const
{
	return lastFrameFPS;
}

qreal FPSMonitor::getMedianFPS() const
{
	return calculateMedianFPS();
}

qreal FPSMonitor::calculateMedianFPS() const
{
	if (renderTimes.isEmpty()) {
		return 0;
	}

	QVector<qreal> sortedTimes = renderTimes;
	std::sort(sortedTimes.begin(), sortedTimes.end());

	size_t size = sortedTimes.size();
	qreal medianTime;
	if (size % 2 == 0) {
		medianTime = (sortedTimes[size / 2 - 1] + sortedTimes[size / 2]) / 2;
	} else {
		medianTime = sortedTimes[size / 2];
	}

	return medianTime > 0 ? 1.0 / medianTime : 0;
}

void FPSMonitor::printTotalFrameStatistics() const
{
	qint64 totalTimeElapsed = totalTimeTimer.elapsed();
	DebugDialog::stream() << "Total frames:" << totalFrameCount
			 << "Total time:" << QString::number(totalTimeElapsed / 1000.0, 'f', 2) << "s"
			 << "Overall FPS:"
			 << QString::number(totalFrameCount * 1000.0 / totalTimeElapsed, 'f', 2) << "fps";
}

void FPSMonitor::paint(QPainter* painter, const QRectF&, const QWidget*)
{
	if (!showFPS) return;

	painter->save();

	// Reset the painter's transform to work in viewport coordinates
	painter->resetTransform();

	// Set up the font and color for the FPS display
	QFont font = painter->font();
	qreal baseFontSize = 12; // Base font size
	font.setPointSizeF(baseFontSize);
	painter->setFont(font);
	painter->setPen(Qt::white);

	// Create a semi-transparent background for better readability
	QColor bgColor(0, 0, 0, 128);
	painter->setBrush(bgColor);

	// Format the FPS strings
	QString lastFrameFPSString = QString("Last: %1").arg(lastFrameFPS, 0, 'f', 1);
	QString medianFPSString = QString("Median: %1").arg(calculateMedianFPS(), 0, 'f', 1);

	// Calculate the text rectangle
	QFontMetrics fm(font);
	QRect lastFrameRect = fm.boundingRect(lastFrameFPSString);
	QRect medianRect = fm.boundingRect(medianFPSString);
	QRect textRect = lastFrameRect.united(medianRect);
	textRect.adjust(-5, -2, 5, 2);  // Add some padding

	// Position the text in the top-left corner of the viewport
	textRect.moveTopLeft(QPoint(10, 10));

	// Draw the background rectangle and the text
	painter->drawRoundedRect(textRect, 5, 5);
	painter->drawText(textRect.adjusted(0, 0, 0, -textRect.height()/2), Qt::AlignCenter, lastFrameFPSString);
	painter->drawText(textRect.adjusted(0, textRect.height()/2, 0, 0), Qt::AlignCenter, medianFPSString);

	painter->restore();
}

void FPSMonitor::showDiagnostics()
{
	QString diagnostics;

	QOpenGLContext *context = QOpenGLContext::currentContext();
	if (context) {
		QOpenGLFunctions *f = context->functions();
		const GLubyte* version = f->glGetString(GL_VERSION);
		const GLubyte* renderer = f->glGetString(GL_RENDERER);
		const GLubyte* vendor = f->glGetString(GL_VENDOR);

		diagnostics += QString("OpenGL Version: %1\n").arg(reinterpret_cast<const char*>(version));
		diagnostics += QString("OpenGL Renderer: %1\n").arg(reinterpret_cast<const char*>(renderer));
		diagnostics += QString("OpenGL Vendor: %1\n").arg(reinterpret_cast<const char*>(vendor));
	} else {
		diagnostics += "OpenGL context not available\n";
	}

	// Qt Platform and Rendering Backend
	diagnostics += "\nQt Rendering Backend:\n";
	diagnostics += QString("Platform plugin: %1\n").arg(QGuiApplication::platformName());

	// Application attributes that affect rendering
	diagnostics += QString("AA_UseDesktopOpenGL: %1\n").arg(QCoreApplication::testAttribute(Qt::AA_UseDesktopOpenGL) ? "true" : "false");
	diagnostics += QString("AA_UseOpenGLES: %1\n").arg(QCoreApplication::testAttribute(Qt::AA_UseOpenGLES) ? "true" : "false");
	diagnostics += QString("AA_UseSoftwareOpenGL: %1\n").arg(QCoreApplication::testAttribute(Qt::AA_UseSoftwareOpenGL) ? "true" : "false");

	// Viewport information (if this FPSMonitor belongs to a QGraphicsView)
	diagnostics += "\nViewport Rendering Path:\n";
	QGraphicsView* view = qobject_cast<QGraphicsView*>(parent());
	if (view) {
		QWidget* viewport = view->viewport();
		diagnostics += QString("Viewport type: %1\n").arg(viewport->metaObject()->className());
	} else {
		diagnostics += "No viewport information available (parent is not QGraphicsView)\n";
	}


	// Get info for all screens
	QList<QScreen*> screens = QGuiApplication::screens();
	diagnostics += QString("\nNumber of screens: %1\n").arg(screens.size());

	for (int i = 0; i < screens.size(); ++i) {
		QScreen *screen = screens[i];
		diagnostics += QString("\nScreen %1: %2\n").arg(i + 1).arg(screen->name());
		diagnostics += QString("Resolution: %1x%2\n")
						   .arg(screen->size().width())
						   .arg(screen->size().height());
		diagnostics += QString("Refresh Rate: %1 Hz\n").arg(screen->refreshRate());
		diagnostics += QString("Device Pixel Ratio: %1\n").arg(screen->devicePixelRatio());
		diagnostics += QString("Physical Size: %1x%2 mm\n")
						   .arg(screen->physicalSize().width())
						   .arg(screen->physicalSize().height());
		diagnostics += QString("Virtual Geometry: (%1,%2) %3x%4\n")
						   .arg(screen->virtualGeometry().x())
						   .arg(screen->virtualGeometry().y())
						   .arg(screen->virtualGeometry().width())
						   .arg(screen->virtualGeometry().height());
		diagnostics += QString("Primary: %1\n").arg(screen == QGuiApplication::primaryScreen() ? "Yes" : "No");
	}

	// Display the diagnostics using FMessageBox
	FMessageBox* msgBox = FMessageBox::createCustom(
		nullptr,
		FMessageBox::Information,
		"Graphics Diagnostics",
		diagnostics,
		FMessageBox::Ok
		);
	msgBox->enableClipboardButton(true);
	msgBox->exec();
	delete msgBox;
}

void FPSMonitor::setShowFPS(bool show)
{
	showFPS = show;
}

bool FPSMonitor::isShowingFPS() const
{
	return showFPS;
}
