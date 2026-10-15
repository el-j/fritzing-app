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

#include "FProbeBin.h"
#include "binmanager/binmanager.h"
#include "binmanager/stacktabwidget.h"
#include "partsbinpalettewidget.h"
#include "debugdialog.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QTabWidget>

FProbeBin::FProbeBin(BinManager *binManager)
	: QObject(binManager)
	, FProbe("Bin")
	, m_binManager(binManager)
{
	connect(this, &FProbeBin::requestAddPart, m_binManager, [this](const QString &moduleID) {
		PartsBinPaletteWidget *bin = m_binManager->currentBin();
		if (bin) {
			bin->addPart(moduleID, -1);
			DebugDialog::debug(QString("FProbeBin: added part %1 to current bin").arg(moduleID));
		} else {
			DebugDialog::debug("FProbeBin: no current bin");
		}
	}, Qt::BlockingQueuedConnection);

	connect(this, &FProbeBin::requestSetCurrentTab, m_binManager, [this](int index) {
		QTabWidget *tabs = m_binManager->findChild<QTabWidget *>();
		if (tabs) {
			tabs->setCurrentIndex(index);
			DebugDialog::debug(QString("FProbeBin: switched to tab %1").arg(index));
		} else {
			DebugDialog::debug("FProbeBin: no QTabWidget found");
		}
	}, Qt::BlockingQueuedConnection);
}

QVariant FProbeBin::read()
{
	QTabWidget *tabs = m_binManager->findChild<QTabWidget *>();
	if (!tabs) return QVariant();

	QJsonArray binsArray;
	for (int i = 0; i < tabs->count(); i++) {
		QJsonObject binObj;
		binObj["index"] = i;
		binObj["title"] = tabs->tabText(i);
		binObj["current"] = (i == tabs->currentIndex());
		binsArray.append(binObj);
	}

	QJsonDocument doc(binsArray);
	return QVariant(QString(doc.toJson(QJsonDocument::Compact)));
}

void FProbeBin::write(QVariant var)
{
	QString param = var.toString();
	if (param.startsWith("add_part:")) {
		QString moduleID = param.mid(9);
		Q_EMIT requestAddPart(moduleID);
	} else if (param.startsWith("set_current:")) {
		bool ok;
		int index = param.mid(12).toInt(&ok);
		if (ok) {
			Q_EMIT requestSetCurrentTab(index);
		} else {
			DebugDialog::debug(QString("FProbeBin: invalid index in '%1'").arg(param));
		}
	} else {
		DebugDialog::debug(QString("FProbeBin: unknown command '%1'").arg(param));
	}
}
