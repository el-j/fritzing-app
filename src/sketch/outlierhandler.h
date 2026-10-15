	#ifndef OUTLIERHANDLER_H
#define OUTLIERHANDLER_H

#include <QObject>
#include <QRectF>
#include <QPointF>
#include <QList>
#include <QString>

class QGraphicsItem;
class ItemBase;
class PartLabel;
class SketchWidget;
class QUndoCommand;
class QDialog;
class QLabel;
class QPushButton;

struct OutlierInfo {
	QGraphicsItem* item;
	ItemBase* owner;
	PartLabel* partLabel;
	QRectF rect;
	QString itemType;
	QString instanceTitle;
};

class OutlierHandler : public QObject
{
	Q_OBJECT

public:
	explicit OutlierHandler(SketchWidget* sketchWidget, QObject *parent = nullptr);

	// Main interface
	QRectF calculateBoundingRectWithOutlierDetection(bool handleOutliers = true);

	// Outlier detection and handling
	QList<OutlierInfo> detectOutliers(const QList<QGraphicsItem*>& items);
	bool askUserToRepositionOutliers(const QList<OutlierInfo>& outliers);
	void repositionOutliers(const QList<OutlierInfo>& outliers, const QRectF& normalBounds);
	void centerAndZoomOnOutlier(const OutlierInfo& outlier);

	// Deferred dialog handling
	void showOutlierDialogDeferred(const QList<OutlierInfo>& outliers);

private slots:
	void handlePendingOutlierDialog();

private:
	QPointF calculateRepositionBase(const QRectF& normalBounds);
	void updateDialogForCurrentOutlier();

	SketchWidget* m_sketchWidget;
	QList<OutlierInfo> m_pendingOutliers;

	// Navigation dialog members
	QDialog* m_navigationDialog;
	QLabel* m_itemInfoLabel;
	QLabel* m_counterLabel;
	QPushButton* m_prevButton;
	QPushButton* m_nextButton;
	int m_currentOutlierIndex;
};

#endif // OUTLIERHANDLER_H
