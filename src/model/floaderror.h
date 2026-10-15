#ifndef FLOADERROR_H
#define FLOADERROR_H

#include <QObject>
#include <QFile>

class FLoadError : public QObject
{
	Q_OBJECT
public:
	FLoadError();
	FLoadError(QString filepath);

	struct Location
	{
		std::filesystem::path name;
		int line;
		int column;
		QString substring;
	};

	// Getters
	QString getMessage() const;
	QString getDescription() const;
	Location getLocation() const;
	bool isBlocker() const;

	// Setters
	void setMessage(const QString& message);
	void setDescription(const QString& description);
	void setLocation(const Location& location);
	void setBlocker(bool blocker);

	// Utility
	QString toString() const;

	void setLine(int line);

	void setColumn(int column);

private:
	QString m_message;
	QString m_description;
	Location m_location;

	// Errors are blockers, loading can not continue
	// Warnings are no blockers, we still try to load
	bool m_blocker;


};

#endif // FLOADERROR_H
