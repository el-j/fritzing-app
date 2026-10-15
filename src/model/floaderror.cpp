#include "floaderror.h"

using namespace Qt::Literals::StringLiterals;

FLoadError::FLoadError() : QObject(nullptr),
	m_message(""),
	m_description(""),
	m_location{std::filesystem::path(), 0, 0, ""},
	m_blocker(true)
{
}

FLoadError::FLoadError(QString filepath) : QObject(nullptr),
	m_message(u""_s),
	m_description(u""_s),
	m_location{std::filesystem::path(filepath.toStdString()), 0, 0, u""_s},
	m_blocker(true)
{
}

// Getters
QString FLoadError::getMessage() const
{
	return m_message;
}

QString FLoadError::getDescription() const
{
	return m_description;
}

FLoadError::Location FLoadError::getLocation() const
{
	return m_location;
}

bool FLoadError::isBlocker() const
{
	return m_blocker;
}

// Setters
void FLoadError::setMessage(const QString& message)
{
	m_message = message;
}

void FLoadError::setDescription(const QString& description)
{
	m_description = description;
}

void FLoadError::setLocation(const Location& location)
{
	m_location = location;
}

void FLoadError::setBlocker(bool blocker)
{
	m_blocker = blocker;
}

void FLoadError::setLine(int line)
{
	m_location.line = line;
}

void FLoadError::setColumn(int column)
{
	m_location.column = column;
}


// Utility method to format error information
QString FLoadError::toString() const
{
	QString result = u"%1: %2\n"_s % (m_blocker ? u"Error"_s : u"Warning"_s) % m_message;

	if (!m_description.isEmpty()) {
		result = result % u"Description: %1\n"_s % m_description;
	}

	if (!m_location.name.empty()) {
		result = result % u"File: %1\n"_s % QString::fromStdString(m_location.name.string());

		if (m_location.line > 0) {
			result = result % u"Line: %1, Column: %2\n"_s % QString::number(m_location.line) % QString::number(m_location.column);
		}

		if (!m_location.substring.isEmpty()) {
			result = result % u"Context: %1\n"_s % m_location.substring;
		}
	}

	return result;
}
