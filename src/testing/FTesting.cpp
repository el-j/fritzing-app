/*******************************************************************

Part of the Fritzing project - http://fritzing.org
Copyright (c) 2022 Fritzing GmbH

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

#include "FTesting.h"
#include "FTestingServer.h"

#include "utils/fmessagebox.h"
#include "debugdialog.h"

#include <QUrl>
#include <QStringBuilder>
#include <QRegularExpression>

FTesting::FTesting() {
}

std::shared_ptr<FTesting> FTesting::getInstance() {
	static std::shared_ptr<FTesting> instance(new FTesting);
	return instance;
}

FTesting::~FTesting() {
}

void FTesting::init() {
	if(m_initialized) return;
	initServer();
	m_initialized = true;
}

bool FTesting::enabled() {
	return m_initialized;
}

void FTesting::addProbe(FProbe * probe)
{
	m_probeMap[probe->name()] = probe;
}

void FTesting::removeProbe(const std::string & name, FProbe * probe)
{
	// Only drop the entry if it still points at this probe. When a MainWindow is recreated (e.g.
	// "Revert"), the new window registers same-named probes before the old window's probe children
	// are destroyed; without this guard the old probe's destructor would erase the new probe's
	// entry, 404-ing every MainWindow-scoped probe after a revert.
	auto it = m_probeMap.find(name);
	if (it != m_probeMap.end() && it->second == probe) {
		m_probeMap.erase(it);
	}
}

std::optional<QVariant> FTesting::readProbe(std::string name)
{
	auto it = m_probeMap.find(name);
	if(it != m_probeMap.end()) {
		return it->second->read();
	}
	return std::nullopt;
}

bool FTesting::writeProbe(std::string name, QVariant param)
{
	auto it = m_probeMap.find(name);
	if(it != m_probeMap.end()) {
		it->second->write(param);
		return true;
	}
	return false;
}

void FTesting::initServer() {
	FMessageBox::BlockMessages = true;
	m_server = new FTestingServer(this);
	connect(m_server, &FTestingServer::newConnection, this, &FTesting::newConnection);
	DebugDialog::debug("FTestingServer active");
	m_server->listen(QHostAddress::Any, m_portNumber);
}

void FTesting::newConnection(qintptr socketDescription) {
	auto *thread = new FTestingServerThread(socketDescription, this);
	connect(thread, SIGNAL(finished()), thread, SLOT(deleteLater()));
	thread->start();
}


QMutex FTestingServerThread::m_busy;

FTestingServerThread::FTestingServerThread(qintptr socketDescriptor, QObject *parent) : QThread(parent), m_socketDescriptor(socketDescriptor)
{
}

void FTestingServerThread::run()
{
	auto * socket = new QTcpSocket();
	if (!socket->setSocketDescriptor(m_socketDescriptor)) {
		Q_EMIT error(socket->error());
		DebugDialog::debug(QString("Socket error %1 %2").arg(socket->error()).arg(socket->errorString()));
		socket->deleteLater();
		return;
	}

	socket->waitForReadyRead();
	QString header;
	while (socket->canReadLine()) {
		header += socket->readLine();
	}

	static auto line_end(QRegularExpression("[ \r\n][ \r\n]*"));
	QStringList tokens = header.split(line_end, Qt::SplitBehaviorFlags::SkipEmptyParts);
	if (tokens.count() <= 0) {
		writeResponse(socket, 400, "Bad Request", "", "");
		return;
	}

	if (tokens[0] != "GET") {
		writeResponse(socket, 405, "Method Not Allowed", "", "");
		return;
	}

	if (tokens.count() < 2) {
		writeResponse(socket, 400, "Bad Request", "", "");
		return;
	}

	QStringList params = tokens.at(1).split("/", Qt::SplitBehaviorFlags::SkipEmptyParts);
	if (params.empty()) {
		writeResponse(socket, 400, "Bad Request", "", "");
		return;
	}
	QString command = QUrl::fromPercentEncoding(params.takeFirst().toUtf8());
	if (params.count() == 0) {
		writeResponse(socket, 400, "Bad Request", "", "");
		return;
	}

	QString readOrWrite = params.takeFirst();

	QString param = "";
	if (readOrWrite.compare("write") == 0 ) {
		if (params.count() == 0) {
			writeResponse(socket, 400, "Bad Request", "", "");
			return;
		}
		param = params.takeFirst();
		param = QUrl::fromPercentEncoding(param.toUtf8());
	}
	
	if (readOrWrite.compare("write") == 0) {
		DebugDialog::debug(QString("FTesting write %1 %2").arg(command, param));
	} else {
		DebugDialog::debug(QString("FTesting read %1").arg(command));
	}

	int waitInterval = 100;     // 100ms to wait
	int timeoutSeconds = 2 * 60;    // timeout after 2 minutes
	int attempts = timeoutSeconds * 1000 / waitInterval;  // timeout a
	bool gotLock = false;
	for (int i = 0; i < attempts; i++) {
		if (m_busy.tryLock()) {
			gotLock = true;
			break;
		}
	}

	if (!gotLock) {
		writeResponse(socket, 503, "Service Unavailable", "", "Server busy.");
		return;
	}

	std::shared_ptr<FTesting> fTesting = FTesting::getInstance();

	if (readOrWrite.compare("write") == 0) {
		bool success = fTesting->writeProbe(command.toStdString(), QVariant(param));
		if (success) {
			writeResponse(socket, 200, "OK", "text/plain", "");
		} else {
			writeResponse(socket, 404, "Not Found", "text/plain", "Probe not found");
		}
	} else {
		std::optional<QVariant> probeResult = fTesting->readProbe(command.toStdString());

		if (probeResult == std::nullopt) {
			DebugDialog::debug(QString("Reading probe failed."));
			writeResponse(socket, 404, "Not Found", "text/plain", "Probe not found");
		} else {
			QString result;
			if (probeResult->typeId() == QMetaType::QPointF) {
				result = QString("%1 %2").arg(probeResult->toPointF().x()).arg(probeResult->toPointF().y());
			} else {
				result = probeResult->toString();
			}

			// Check if result is JSON and set appropriate content type
			QString mimeType = "text/plain";
			if (result.startsWith('{') || result.startsWith('[')) {
				mimeType = "application/json";
			}

			writeResponse(socket, 200, "OK", mimeType, result);
		}
	}

	m_busy.unlock();
}

void FTestingServerThread::writeResponse(QTcpSocket * socket, int code, const QString & codeString, const QString & mimeType, const QString & message)
{
	QString type = mimeType;
	if (type.isEmpty()) type = "text/plain";

	QByteArray messageBytes = message.toUtf8();
	QByteArray header = QString("HTTP/1.0 %1 %2\r\n"
								"Content-Type: %3; charset=utf-8\r\n"
								"Content-Length: %4\r\n"
								"Connection: close\r\n"
								"\r\n")
							.arg(code)
							.arg(codeString, type)
							.arg(messageBytes.length())
							.toUtf8();

	// Send the complete HTTP response in one go
	socket->write(header);
	socket->write(messageBytes);
	socket->flush();
	socket->waitForBytesWritten();

	socket->disconnectFromHost();
	socket->waitForDisconnected();
	socket->deleteLater();
}


