#ifndef FPSMONITOR_H
#define FPSMONITOR_H

#include <QElapsedTimer>
#include <QPainter>
#include <QVector>
#include <QObject>

class FPSMonitor : public QObject {
	Q_OBJECT
public:
	explicit FPSMonitor(QObject *parent = nullptr);

	void update();
	void reset();
	qreal getLastFrameFPS() const;
	qreal getMedianFPS() const;
	void printTotalFrameStatistics() const;

	void paint(QPainter* painter, const QRectF&, const QWidget*);
	void setShowFPS(bool show);
	bool isShowingFPS() const;

	void showDiagnostics();
	static bool checkOpenGLAvailability();

private:
	QElapsedTimer totalTimeTimer;
	QElapsedTimer frameTimer;
	QVector<qreal> renderTimes;
	qint64 totalFrameCount;
	qreal lastFrameFPS;
	bool showFPS;

	qreal calculateMedianFPS() const;

};

#endif // FPSMONITOR_H
