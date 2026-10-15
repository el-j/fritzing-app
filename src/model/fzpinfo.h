/*******************************************************************

Part of the Fritzing project - https://fritzing.org
Copyright (c) 2024 Fritzing GmbH

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


#ifndef FZPINFO_H
#define FZPINFO_H

#include <QString>
#include <QFile>
#include <QSharedPointer>
#include <QList>

#include "floaderror.h"


class FzpInfo : public QObject {
	Q_OBJECT
public:
	explicit FzpInfo(QString path, QObject* parent = nullptr);
	~FzpInfo();

	// Read accessors
	QString moduleId() const { return m_moduleId; }
	QString fritzingVersion() const { return m_fritzingVersion; }
	QString title() const { return m_title; }
	QString path() const { return m_path; }

	// Error access
	const QList<QSharedPointer<FLoadError>>& errors() const { return m_errors; }
	bool hasAnyErrors() const { return !m_errors.isEmpty(); }
	bool hasBlockingErrors() const;
	QString getSummaryText() const;
	QString getDetailsText() const;


	// Error methods (all blocking by default)
	void addError(const QString& message);
	void addError(
		const QString& title,
		const QString& message,
		int line = -1,
		int column = -1
		);

	// Warning methods (never blocking)
	void addWarning(const QString& message);
	void addWarning(
		const QString& title,
		const QString& message,
		int line = -1,
		int column = -1
		);

	// Processing
	void parse();
	void validate();

private:
	void validateTitle();
	void validateVersion();

	QSharedPointer<FLoadError> createLoadError(
		const QString& title,
		const QString& message,
		bool isBlocker,
		int line,
		int column
		);

	QString m_moduleId;
	QString m_fritzingVersion;
	QString m_title;
	// QString label;
	// QString author;
	// QString date;
	QString m_path;
	int m_moduleLineNumber = -1;  // Track line number of the module tag

	QList<QSharedPointer<FLoadError>> m_errors;

};

#endif //  FZPINFO_H
