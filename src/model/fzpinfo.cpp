#include "fzpinfo.h"
#include "floaderror.h"
#include "version/version.h"

#include <QFile>
#include <QXmlStreamReader>
#include <QTranslator>

FzpInfo::FzpInfo(QString path, QObject* parent)
	: QObject(parent)
	, m_path(path) {}

FzpInfo::~FzpInfo() {}

void FzpInfo::addError(const QString& message)
{
	addError(tr("Error", "error title"), message);
}

void FzpInfo::addError(
	const QString& title,
	const QString& message,
	int line,
	int column
	) {
	auto error = createLoadError(title, message, true, line, column);
	m_errors.append(error);
}

void FzpInfo::addWarning(const QString& message) {
	addWarning(tr("Warning", "warning title"), message);
}

void FzpInfo::addWarning(const QString& title, const QString& message, int line, int column)
{
	auto error = createLoadError(title, message, false, line, column);
	m_errors.append(error);
}

QSharedPointer<FLoadError> FzpInfo::createLoadError(
	const QString& title, const QString& message, bool isBlocker, int line, int column)
{
	auto error = QSharedPointer<FLoadError>::create(m_path);
	error->setMessage(title);
	error->setDescription(message);
	error->setBlocker(isBlocker);

	if (line >= 0) {
		error->setLine(line);
	}
	if (column >= 0) {
		error->setColumn(column);
	}

	return error;
}

void FzpInfo::parse()
{
	QFile file(m_path);
	if (!file.open(QFile::ReadOnly)) {
		addError(
			tr("Cannot open file", "error title"),
			tr("Cannot open file '%1'.").arg(m_path)
			);
		return;
	}

	if (file.size() == 0) {
		addError(
			tr("File is empty", "error title"),
			tr("File '%1' is empty.").arg(m_path)
			);
		return;
	}

	QXmlStreamReader streamReader(&file);

	streamReader.setNamespaceProcessing(false);

	QString elementName;
	while (!streamReader.atEnd()) {
		switch (streamReader.readNext()) {
		case QXmlStreamReader::StartElement:
			if (streamReader.name().toString().compare("module") == 0) {
				m_moduleId = streamReader.attributes().value("moduleId").toString();
				m_fritzingVersion = streamReader.attributes().value("fritzingVersion").toString();
				m_moduleLineNumber = streamReader.lineNumber(); // Store module tag line number

				// Look for other elements within module
				while (!streamReader.atEnd() ) {
					switch (streamReader.readNext()) {
					case QXmlStreamReader::StartElement:
						elementName = streamReader.name().toString();
						if (elementName == "title") {
							m_title = streamReader.readElementText();
						}
						// else if (elementName == "label") {
						// 	info.label = streamReader.readElementText();
						// }
						// else if (elementName == "author") {
						// 	info.author = streamReader.readElementText();
						// }
						// else if (elementName == "date") {
						// 	info.date = streamReader.readElementText();
						// }
						break;
					case QXmlStreamReader::EndElement:
						if (streamReader.name().toString().compare("module") == 0) {
							// We've reached the end of module element
							// addError("incomplete"); // not an error, we currently don't have an earlier exit or completion check
							return;
						}
						break;
					default:
						break;
					}
				}
			}
			break;
		default:
			break;
		}
	}

	if (streamReader.hasError()) {
		addError(
			tr("XML Error", "error title"),
			streamReader.errorString(),
			streamReader.lineNumber(),
			streamReader.columnNumber()
			);
	}
}

void FzpInfo::validate()
{
	validateTitle();
	validateVersion();
}

void FzpInfo::validateTitle()
{
	if (m_title.isEmpty()) {
		addWarning(
			tr("Title is missing.", "warning title"),
			tr("The part is missing a title.\n\n"
			   "All parts must have a title tag.")
			);
	}
}

void FzpInfo::validateVersion()
{
	if (m_fritzingVersion.isEmpty()) {
		addWarning(
			tr("Version number missing.", "warning title"),
			tr("The part is missing a fritzing version.\n"
			   "All parts must have a fritzingVersion attribute: fritzingVersion=\"x.y.z\"."),
			m_moduleLineNumber
			);
		return;
	}

	VersionThing versionThingFzp;
	Version::toVersionThing(m_fritzingVersion, versionThingFzp);

	if (!versionThingFzp.ok) {
		addWarning(
			tr("Invalid Version", "warning title"),
			tr("The fritzing version '%1' is invalid.\n"
			   "The part might not work properly.").arg(m_fritzingVersion),
			m_moduleLineNumber
			);
		return;
	}

	VersionThing currentVersionThing;
	Version::toVersionThing(Version::versionString(), currentVersionThing);

	if (Version::greaterThan(currentVersionThing, versionThingFzp)) {
		addWarning(
			tr("Version Mismatch", "warning title"),
			tr("This part was created with Fritzing version '%1'.\n"
			   "Current version is '%2' which might not support it properly."
			   "Please consider updating your Fritzing.\n\n")
				.arg(m_fritzingVersion, Version::versionString()),
			m_moduleLineNumber
			);
	}
}


QString FzpInfo::getSummaryText() const {
	QString summary;
	for (const auto& error : m_errors) {
		summary += "- " + error->getMessage() + "\n";
	}
	return summary;
}

QString FzpInfo::getDetailsText() const {
	QString details;
	for (const auto& error : m_errors) {
		details += error->getMessage() + "\n";  // Title/type of issue
		details += "─────────────────────────\n";
		details += error->getDescription() + "\n";  // Detailed description

		// Add location information if available
		auto location = error->getLocation();
		if (location.line >= 0) {
			details += tr("Location: Line %1, Column %2\n")
			.arg(location.line)
				.arg(location.column);
		}
		details += "\n";  // Space between entries
	}
	return details;
}

bool FzpInfo::hasBlockingErrors() const {
	return std::any_of(m_errors.begin(), m_errors.end(),
					   [](const QSharedPointer<FLoadError>& error) {
						   return error->isBlocker();
					   });
}
