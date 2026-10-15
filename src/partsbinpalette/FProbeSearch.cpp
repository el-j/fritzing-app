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

#include "FProbeSearch.h"
#include "binmanager/binmanager.h"
#include "partsbinpalettewidget.h"
#include "debugdialog.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>

FProbeSearch::FProbeSearch(BinManager *binManager)
	: QObject(binManager)
	, FProbe("Search")
	, m_binManager(binManager)
{
	connect(this, &FProbeSearch::requestSearch, m_binManager, [this](const QString &searchText) {
		m_binManager->search(searchText);
		DebugDialog::debug(QString("FProbeSearch: searched for '%1'").arg(searchText));
	}, Qt::BlockingQueuedConnection);
}

QVariant FProbeSearch::read()
{
	QJsonArray moduleIds;
	PartsBinPaletteWidget *bin = m_binManager->searchBin();
	if (bin != nullptr) {
		for (ModelPart *mp : bin->getAllParts()) {
			moduleIds.append(mp->moduleID());
		}
	}

	QJsonObject result;
	result["moduleIds"] = moduleIds;

	QJsonDocument doc(result);
	return QVariant(QString(doc.toJson(QJsonDocument::Compact)));
}

void FProbeSearch::write(QVariant var)
{
	QString searchText = var.toString();
	Q_EMIT requestSearch(searchText);
}
