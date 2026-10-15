#include "outlierhandler.h"
#include "sketchwidget.h"
#include "items/itembase.h"
#include "items/partlabel.h"
#include "commands.h"

#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QUndoCommand>
#include <QTimer>
#include <QDebug>
#include "../debugdialog.h"
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QApplication>
#include <QGraphicsView>
#include <QFont>
#include <QtMath>
#include <vector>
#include <cmath>

OutlierHandler::OutlierHandler(SketchWidget* sketchWidget, QObject *parent)
	: QObject(parent), m_sketchWidget(sketchWidget), m_navigationDialog(nullptr)
	, m_itemInfoLabel(nullptr), m_counterLabel(nullptr), m_prevButton(nullptr)
	, m_nextButton(nullptr), m_currentOutlierIndex(0)
{
}

QRectF OutlierHandler::calculateBoundingRectWithOutlierDetection(bool handleOutliers) {
	// Structure for centroid-based outlier detection
	struct PositionData {
		std::vector<QPointF> positions;
		std::vector<QGraphicsItem*> items;
		double outlierDistanceThreshold = 10000.0; // 10k units from centroid
		
		QPointF calculateCentroid() const {
			if (positions.empty()) return QPointF(0, 0);
			double sumX = 0.0, sumY = 0.0;
			for (const auto& pos : positions) {
				sumX += pos.x();
				sumY += pos.y();
			}
			return QPointF(sumX / positions.size(), sumY / positions.size());
		}
		
		std::vector<bool> detectOutliers() const {
			if (positions.size() <= 1) return std::vector<bool>(positions.size(), false);
			
			QPointF centroid = calculateCentroid();
			std::vector<bool> isOutlier(positions.size(), false);
			
			for (size_t i = 0; i < positions.size(); ++i) {
				double distance = std::sqrt(
					std::pow(positions[i].x() - centroid.x(), 2) +
					std::pow(positions[i].y() - centroid.y(), 2)
				);
				isOutlier[i] = distance > outlierDistanceThreshold;
			}
			
			return isOutlier;
		}
	};

	QRectF itemsRect;
	QList<OutlierInfo> outliers;
	PositionData positionData;

	QList<QGraphicsItem*> items = m_sketchWidget->scene()->items();
	for (QGraphicsItem* item : items) {
		if (!item->isVisible()) continue;

		auto * partLabel = dynamic_cast<PartLabel *>(item);
		if (partLabel && partLabel->initialized()) {
			itemsRect |= partLabel->sceneBoundingRect();
			if (handleOutliers) {
				positionData.positions.push_back(partLabel->sceneBoundingRect().center());
				positionData.items.push_back(item);
			}
			continue;
		}

		auto * itemBase = dynamic_cast<ItemBase*>(item);
		if (itemBase && itemBase->isEverVisible()) {
			itemsRect |= itemBase->sceneBoundingRect();
			if (handleOutliers) {
				positionData.positions.push_back(itemBase->sceneBoundingRect().center());
				positionData.items.push_back(item);
			}
		}
	}

	// If outlier handling is requested, detect outliers using centroid approach
	if (handleOutliers && !positionData.positions.empty()) {
		std::vector<bool> isOutlier = positionData.detectOutliers();
		
		// Create outlier info for detected outliers
		for (size_t i = 0; i < isOutlier.size(); ++i) {
			if (isOutlier[i]) {
				QGraphicsItem* item = positionData.items[i];
				QRectF itemRect = item->sceneBoundingRect();
				
				OutlierInfo outlierInfo;
				outlierInfo.item = item;
				outlierInfo.owner = dynamic_cast<ItemBase*>(item);
				if (!outlierInfo.owner) {
					PartLabel* partLabel = dynamic_cast<PartLabel*>(item);
					if (partLabel) {
						outlierInfo.owner = partLabel->owner();
						outlierInfo.itemType = "label";
						outlierInfo.instanceTitle = partLabel->owner() ? partLabel->owner()->instanceTitle() : "unknown";
					}
				} else {
					outlierInfo.itemType = outlierInfo.owner->instanceTitle();
					outlierInfo.instanceTitle = outlierInfo.owner->instanceTitle();
				}
				outlierInfo.partLabel = dynamic_cast<PartLabel*>(item);
				outlierInfo.rect = itemRect;
				outliers.append(outlierInfo);
			}
		}
		
		// If outliers were found, handle them
		if (!outliers.isEmpty()) {
			// Don't show outlier dialog during initial automatic fit-in-window
			if (m_sketchWidget->everZoomed()) {
				showOutlierDialogDeferred(outliers);
			} else {
				DebugDialog::debug(QString("Outliers detected during initial load, skipping dialog. Count: %1").arg(outliers.size()));
			}
			
			// IMPORTANT: Keep original itemsRect unchanged when outliers are found
			// The outliers will be handled separately through the dialog
		}
	}

	return itemsRect;
}

bool OutlierHandler::askUserToRepositionOutliers(const QList<OutlierInfo>& outliers) {
	if (outliers.isEmpty()) return false;

	m_currentOutlierIndex = 0;

	// Create navigation dialog as a heap object to avoid blocking
	m_navigationDialog = new QDialog(qobject_cast<QWidget*>(m_sketchWidget));
	m_navigationDialog->setWindowTitle(tr("Outlier Components Navigator", "dialog title"));
	m_navigationDialog->setModal(false); // Non-modal to avoid rendering issues
	m_navigationDialog->resize(600, 400);
	m_navigationDialog->setAttribute(Qt::WA_DeleteOnClose); // Auto-delete when closed

	QVBoxLayout* layout = new QVBoxLayout(m_navigationDialog);

	// Title and counter
	m_counterLabel = new QLabel(m_navigationDialog);
	m_counterLabel->setAlignment(Qt::AlignCenter);
	QFont titleFont = m_counterLabel->font();
	titleFont.setBold(true);
	titleFont.setPointSize(titleFont.pointSize() + 2);
	m_counterLabel->setFont(titleFont);
	layout->addWidget(m_counterLabel);

	// Item information
	m_itemInfoLabel = new QLabel(m_navigationDialog);
	m_itemInfoLabel->setWordWrap(true);
	m_itemInfoLabel->setAlignment(Qt::AlignTop);
	m_itemInfoLabel->setMinimumHeight(150);
	layout->addWidget(m_itemInfoLabel);

	// Navigation buttons (only show if more than one item)
	QHBoxLayout* navLayout = new QHBoxLayout();
	if (m_pendingOutliers.size() > 1) {
		m_prevButton = new QPushButton(tr("◀ Previous"), m_navigationDialog);
		m_nextButton = new QPushButton(tr("Next ▶"), m_navigationDialog);

		navLayout->addWidget(m_prevButton);
		navLayout->addStretch();
		navLayout->addWidget(m_nextButton);
		layout->addLayout(navLayout);
	}

	// Action buttons
	QHBoxLayout* actionLayout = new QHBoxLayout();
	QString repositionButtonText;
	if (m_pendingOutliers.size() == 1) {
		repositionButtonText = tr("Fix This Item");
	} else {
		repositionButtonText = tr("Fix All Items");
	}
	QPushButton* repositionButton = new QPushButton(repositionButtonText, m_navigationDialog);
	QPushButton* closeButton = new QPushButton(tr("Close"), m_navigationDialog);

	actionLayout->addStretch();
	actionLayout->addWidget(repositionButton);
	actionLayout->addWidget(closeButton);
	actionLayout->addStretch();
	layout->addLayout(actionLayout);

	// Connect navigation buttons (only if they exist)
	if (m_prevButton && m_nextButton) {
		QObject::connect(m_prevButton, &QPushButton::clicked, [this]() {
			DebugDialog::debug(QString("Previous button clicked, current index: %1").arg(m_currentOutlierIndex));
			if (m_currentOutlierIndex > 0) {
				m_currentOutlierIndex--;
				DebugDialog::debug(QString("Moving to index: %1").arg(m_currentOutlierIndex));
				updateDialogForCurrentOutlier();
			}
		});

		QObject::connect(m_nextButton, &QPushButton::clicked, [this]() {
			DebugDialog::debug(QString("Next button clicked, current index: %1").arg(m_currentOutlierIndex));
			if (m_currentOutlierIndex < m_pendingOutliers.size() - 1) {
				m_currentOutlierIndex++;
				DebugDialog::debug(QString("Moving to index: %1").arg(m_currentOutlierIndex));
				updateDialogForCurrentOutlier();
			}
		});
	}

	// Connect action buttons
	QObject::connect(repositionButton, &QPushButton::clicked, [this]() {
		m_navigationDialog->accept();
		m_navigationDialog->close();

		if (m_pendingOutliers.size() == 1) {
			// Handle single item - only fix the current one
			QRectF normalBounds = calculateBoundingRectWithOutlierDetection(false);
			QList<OutlierInfo> singleItem = { m_pendingOutliers[m_currentOutlierIndex] };
			repositionOutliers(singleItem, normalBounds);
			centerAndZoomOnOutlier(m_pendingOutliers[m_currentOutlierIndex]);
		} else {
			// Handle multiple items - fix all
			QRectF normalBounds = calculateBoundingRectWithOutlierDetection(false);
			repositionOutliers(m_pendingOutliers, normalBounds);
			if (!m_pendingOutliers.isEmpty()) {
				centerAndZoomOnOutlier(m_pendingOutliers.first());
			}
		}
	});

	QObject::connect(closeButton, &QPushButton::clicked, [this]() {
		m_navigationDialog->accept();
		m_navigationDialog->close();
		// Just close dialog, do nothing else
		});

	// Initialize dialog content and show first outlier
	updateDialogForCurrentOutlier();

	// Show the dialog non-modally
	m_navigationDialog->show();
	m_navigationDialog->raise();
	m_navigationDialog->activateWindow();

	return false; // This method now returns immediately, actual work happens in button callbacks
}

void OutlierHandler::repositionOutliers(const QList<OutlierInfo>& outliers, const QRectF& normalBounds) {
	if (outliers.isEmpty()) return;

	// Create a parent command to group all moves
	auto * parentCommand = new QUndoCommand(tr("Reposition Outlier Components"));

	// Track modified parts for selection
	QList<ItemBase*> modifiedParts;

	// Calculate base position for repositioning
	QPointF repositionBase = calculateRepositionBase(normalBounds);

	for (int i = 0; i < outliers.size(); i++) {
		const OutlierInfo& outlier = outliers[i];

		// Calculate new position with some spacing between items
		QPointF targetPosition = repositionBase + QPointF(0, i * 100);

		if (outlier.partLabel && outlier.owner) {
			// For labels, position at top-right of the owner part
			QRectF ownerBounds = outlier.owner->sceneBoundingRect();
			QPointF labelTargetScenePos = QPointF(ownerBounds.right(), ownerBounds.top());

			QPointF currentPos = outlier.partLabel->pos();
			QPointF currentOffset = outlier.partLabel->getOffset();

			// Label positioning: label->setPos(offset + ownerPos)
			// So: newPos should be the desired final scene position
			// And: newOffset should be newPos - ownerPos
			QPointF ownerPos = outlier.owner->pos();
			QPointF newPos = labelTargetScenePos; // Set label to final desired position
			QPointF newOffset = newPos - ownerPos; // Offset is difference from owner

			DebugDialog::debug(QString("Repositioning label for %1 from scene pos (%2,%3) to scene pos (%4,%5)")
								   .arg(outlier.instanceTitle).arg(currentPos.x()).arg(currentPos.y()).arg(newPos.x()).arg(newPos.y()));
			DebugDialog::debug(QString("  Owner pos: (%1,%2) New offset: (%3,%4)").arg(ownerPos.x()).arg(ownerPos.y()).arg(newOffset.x()).arg(newOffset.y()));

			new MoveLabelCommand(m_sketchWidget, outlier.owner->id(), currentPos, currentOffset, newPos, newOffset, parentCommand);

			if (outlier.owner && !modifiedParts.contains(outlier.owner)) {
				modifiedParts.append(outlier.owner);
			}
		} else if (outlier.owner) {
			// For regular parts, move to calculated position
			QPointF currentPos = outlier.owner->pos();

			new SimpleMoveItemCommand(m_sketchWidget, outlier.owner->id(), currentPos, targetPosition, parentCommand);

			if (!modifiedParts.contains(outlier.owner)) {
				modifiedParts.append(outlier.owner);
			}
		}
	}

	// Execute the command
	m_sketchWidget->undoStack()->push(parentCommand);

	// Select all modified parts
	m_sketchWidget->scene()->clearSelection();
	for (ItemBase* part : modifiedParts) {
		part->setSelected(true);
	}

	// Show undo history
	m_sketchWidget->showUndoHistoryWidget();

	// Update the scene
	m_sketchWidget->scene()->update();
}

QPointF OutlierHandler::calculateRepositionBase(const QRectF& normalBounds) {
	if (!normalBounds.isEmpty()) {
		return normalBounds.topLeft() + QPointF(100, 100);
	} else {
		return QPointF(1000, 1000); // Fallback position
	}
}

void OutlierHandler::centerAndZoomOnOutlier(const OutlierInfo& outlier) {
	if (!outlier.owner || !m_sketchWidget) return;

	// Get the part's position and create a view rectangle around it
	QPointF partPos = outlier.owner->pos();
	if (outlier.partLabel) {
		QRectF labelRect = outlier.partLabel->sceneBoundingRect();
		if (qAbs(labelRect.x() - partPos.x()) > 1000 || qAbs(labelRect.y() - partPos.y()) > 1000) {
			// Label is far - focus on part only, don't expand view
		}
	}

	QRectF focusRect(partPos.x() - 200, partPos.y() - 200, 400, 400);

	// Center and zoom on the focus area
	m_sketchWidget->fitInView(focusRect, Qt::KeepAspectRatio);

	// Update zoom level immediately, just like fitInWindow() does
	m_sketchWidget->updateZoomFromCurrentTransform();

	// Select the problematic item to highlight it
	m_sketchWidget->scene()->clearSelection();
	if (outlier.owner) {
		outlier.owner->setSelected(true);
	}
}

void OutlierHandler::showOutlierDialogDeferred(const QList<OutlierInfo>& outliers) {
	m_pendingOutliers = outliers;
	QTimer::singleShot(100, this, &OutlierHandler::handlePendingOutlierDialog);
}

void OutlierHandler::handlePendingOutlierDialog() {
	if (m_pendingOutliers.isEmpty()) return;

	askUserToRepositionOutliers(m_pendingOutliers);
}

void OutlierHandler::updateDialogForCurrentOutlier() {
	DebugDialog::debug(QString("updateDialogForCurrentOutlier called, index: %1 size: %2").arg(m_currentOutlierIndex).arg(m_pendingOutliers.size()));

	if (m_currentOutlierIndex < 0 || m_currentOutlierIndex >= m_pendingOutliers.size()) {
		DebugDialog::debug("Invalid index, returning");
		return;
	}

	const OutlierInfo& currentOutlier = m_pendingOutliers[m_currentOutlierIndex];
	DebugDialog::debug(QString("Updating dialog for outlier: %1").arg(currentOutlier.instanceTitle));

	if (m_pendingOutliers.size() == 1) {
		m_counterLabel->setText(tr("Problematic Item"));
	} else {
		m_counterLabel->setText(tr("Item %1 of %2").arg(m_currentOutlierIndex + 1).arg(m_pendingOutliers.size()));
	}

	QString info;
	if (currentOutlier.partLabel) {
		info = tr("<b>Problem:</b> Label positioned far from its component<br><br>");
		info += tr("<b>Component:</b> %1<br>").arg(currentOutlier.instanceTitle);
		info += tr("<b>Label Position:</b> (%1, %2)<br>").arg(qRound(currentOutlier.rect.x())).arg(qRound(currentOutlier.rect.y()));
		if (currentOutlier.owner) {
			QPointF partPos = currentOutlier.owner->pos();
			info += tr("<b>Component Position:</b> (%1, %2)<br>").arg(qRound(partPos.x())).arg(qRound(partPos.y()));
			qreal distance = QLineF(currentOutlier.rect.center(), partPos).length();
			info += tr("<b>Distance:</b> %1 units<br><br>").arg(qRound(distance));
		}
		info += tr("<b>Impact:</b> This label's distant position causes 'Fit in Window' to zoom out excessively.<br><br>");
		info += tr("<b>Solution:</b> The label will be repositioned to the top-right of its component.");
	} else {
		info = tr("<b>Problem:</b> Component positioned far outside the circuit area<br><br>");
		info += tr("<b>Component:</b> %1<br>").arg(currentOutlier.instanceTitle);
		info += tr("<b>Type:</b> %1<br>").arg(currentOutlier.itemType);
		info += tr("<b>Position:</b> (%1, %2)<br>").arg(qRound(currentOutlier.rect.x())).arg(qRound(currentOutlier.rect.y()));
		info += tr("<b>Size:</b> %1 × %2<br><br>").arg(qRound(currentOutlier.rect.width())).arg(qRound(currentOutlier.rect.height()));
		info += tr("<b>Impact:</b> This component's position causes 'Fit in Window' to zoom out excessively.<br><br>");
		info += tr("<b>Solution:</b> The component will be moved to a reasonable location near the main circuit.");
	}

	m_itemInfoLabel->setText(info);

	if (m_prevButton && m_nextButton) {
		m_prevButton->setEnabled(m_currentOutlierIndex > 0);
		m_nextButton->setEnabled(m_currentOutlierIndex < m_pendingOutliers.size() - 1);
	}

	centerAndZoomOnOutlier(currentOutlier);
}
