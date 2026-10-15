#include "fprobefocuswidget.h"
#include <QStringList>

FProbeFocusWidget::FProbeFocusWidget()
	: FProbe("FocusWidget")
{
}

QVariant FProbeFocusWidget::read()
{
	return QVariant();
}

void FProbeFocusWidget::write(QVariant data)
{
	// Accepted forms:
	//   "objectName"                -> first widget with that objectName
	//   "objectName;index"          -> the index-th widget with that objectName
	//   "objectName@property"       -> the widget with that objectName whose dynamic
	//                                  "fProbeProperty" equals `property`. Use this when
	//                                  several editors share an objectName (e.g. the
	//                                  inspector property combos all use "infoViewComboBox").
	QString input = data.toString();
	if (input.contains('@')) {
		QString objectName = input.section('@', 0, 0);
		QString property = input.section('@', 1);
		emit focusWidget(objectName, 0, property);
	} else {
		QStringList parts = input.split(";");
		QString objectName = parts[0];
		int index = (parts.size() >= 2) ? parts[1].toInt() : 0;
		emit focusWidget(objectName, index, QString());
	}
}
