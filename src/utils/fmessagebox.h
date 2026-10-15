/*******************************************************************

Part of the Fritzing project - http://fritzing.org
Copyright (c) 2007-2019 Fritzing
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

#ifndef FMESSAGEBOX_H
#define FMESSAGEBOX_H

#include <QMessageBox>
#include <QPushButton>
#include "qdialogbuttonbox.h"

/**
	\class FMessageBox
	\brief The FMessageBox class provides an extended message box with additional features.
	\inmodule QtWidgets

	FMessageBox is a subclass of QMessageBox that adds functionality such as
	message logging, clipboard copying, and global message blocking. It's designed
	to be a drop-in replacement for QMessageBox with enhanced capabilities.

	The class offers static convenience functions to display message boxes:

	\code
	FMessageBox::critical(this, tr("Error"), tr("A critical error occurred."));
	FMessageBox::information(this, tr("Information"), tr("Operation completed successfully."));
	FMessageBox::question(this, tr("Confirm"), tr("Are you sure?"), FMessageBox::Yes | FMessageBox::No);
	FMessageBox::warning(this, tr("Warning"), tr("This operation may take a long time."));
	\endcode

	These functions return a StandardButton value indicating which button was clicked.

	FMessageBox also provides a custom builder method for more complex scenarios:

	\code
	FMessageBox* msgBox = FMessageBox::createCustom(this, FMessageBox::Information,
													tr("Title"), tr("Message"));
	msgBox->enableClipboardButton(true);
	msgBox->setDetailedText(tr("Additional details here."));
	msgBox->exec();
	\endcode

	\sa QMessageBox
*/

class FMessageBox : public QMessageBox
{
	Q_OBJECT

public:
	/**
		Constructs a message box with the given \a parent.
	*/
	explicit FMessageBox(QWidget *parent);

	/**
		\reimp
	*/
	int exec() override;

	/**
		Creates and returns a custom FMessageBox.

		\a parent is the parent widget.
		\a icon is the message box icon.
		\a title is the window title.
		\a text is the main message text.
		\a buttons specifies which buttons to use.
		\a defaultButton is the default button.
	*/
	static FMessageBox* createCustom(QWidget *parent, Icon icon, const QString &title,
									 const QString &text, StandardButtons buttons = Ok,
									 StandardButton defaultButton = NoButton);

	/**
		Sets the detailed text to \a text.
		This text is displayed when the user clicks the "Show Details..." button.
	*/
	void setDetailedText(const QString &text);

	/**
		Enables or disables the clipboard button based on \a enable.
		When enabled, users can copy the message content to the clipboard.
	*/
	void enableClipboardButton(bool enable);

	/**
		Displays a critical message box.
		Returns the identity of the standard button that was clicked.

		\a parent is the parent widget.
		\a title is the window title.
		\a text is the message text.
		\a buttons specifies which buttons to use.
		\a defaultButton is the default button.
	*/
	static StandardButton critical(QWidget *parent, const QString &title, const QString &text,
								   StandardButtons buttons = Ok, StandardButton defaultButton = NoButton);

	/**
		Displays an information message box.
		Returns the identity of the standard button that was clicked.

		Parameters are the same as for critical().
	*/
	static StandardButton information(QWidget *parent, const QString &title, const QString &text,
									  StandardButtons buttons = Ok, StandardButton defaultButton = NoButton);

	/**
		Displays a question message box.
		Returns the identity of the standard button that was clicked.

		Parameters are the same as for critical().
	*/
	static StandardButton question(QWidget *parent, const QString &title, const QString &text,
								   StandardButtons buttons = StandardButtons(Yes | No),
								   StandardButton defaultButton = NoButton);

	/**
		Displays a warning message box.
		Returns the identity of the standard button that was clicked.

		Parameters are the same as for critical().
	*/
	static StandardButton warning(QWidget *parent, const QString &title, const QString &text,
								  StandardButtons buttons = Ok, StandardButton defaultButton = NoButton);

	/**
		Returns a list of all logged messages.
		Each entry in the list is a pair of strings: the title and the message text.
	*/
	static QList<QPair<QString, QString>> getLoggedMessages();

	/**
		When set to true, all message boxes are blocked from being displayed.
		Useful for testing or batch operations.
	*/
	static bool BlockMessages;

protected:
	void closeEvent(QCloseEvent *event) override;

	/**
		Logs a message with the given \a title and \a text.
	*/
	static void logMessage(const QString &title, const QString &text);

	/**
		Stores all logged messages.
	*/
	static QList<QPair<QString, QString>> messageLog;
	static const int MaxLogEntries = 1000;

private:
	QPushButton *m_copyButton;
	bool m_clipboardButtonEnabled;
	QDialogButtonBox *m_buttonBox;
	void setupUI();
};

#endif // FMESSAGEBOX_H
