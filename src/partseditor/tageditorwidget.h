/*******************************************************************

Part of the Fritzing project - http://fritzing.org
Copyright (c) 2026 Fritzing

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


#ifndef TAGEDITORWIDGET_H
#define TAGEDITORWIDGET_H

#include <QFrame>
#include <QLayout>
#include <QLayoutItem>
#include <QLineEdit>
#include <QList>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QStyledItemDelegate>

class QCompleter;
class QStandardItem;
class QStandardItemModel;

// A wrapping layout (the classic Qt "Flow Layout" example) so the tag chips flow
// onto multiple lines inside a single bordered container.
class FlowLayout : public QLayout
{
public:
	explicit FlowLayout(QWidget * parent, int margin = -1, int hSpacing = -1, int vSpacing = -1);
	explicit FlowLayout(int margin = -1, int hSpacing = -1, int vSpacing = -1);
	~FlowLayout() override;

	void addItem(QLayoutItem * item) override;
	void insertWidget(int index, QWidget * widget);
	int horizontalSpacing() const;
	int verticalSpacing() const;
	Qt::Orientations expandingDirections() const override;
	bool hasHeightForWidth() const override;
	int heightForWidth(int) const override;
	int count() const override;
	QLayoutItem * itemAt(int index) const override;
	QSize minimumSize() const override;
	void setGeometry(const QRect & rect) override;
	QSize sizeHint() const override;
	QLayoutItem * takeAt(int index) override;

private:
	int doLayout(const QRect & rect, bool testOnly) const;
	int smartSpacing(QStyle::PixelMetric pm) const;

	QList<QLayoutItem *> m_itemList;
	int m_hSpace;
	int m_vSpace;
};


// A single tag rendered as a compact "#tag" chip with an × remove affordance.
class TagChip : public QFrame
{
	Q_OBJECT
public:
	explicit TagChip(const QString & tag, QWidget * parent = nullptr);
	QString tag() const { return m_tag; }

Q_SIGNALS:
	void removeRequested(TagChip * chip);

private:
	QString m_tag;
};


// The inline input. Reports the editor-relevant key gestures up to the TagEditorWidget.
// Navigation / activation inside the completer popup is handled by QCompleter itself.
class TagLineEdit : public QLineEdit
{
	Q_OBJECT
public:
	explicit TagLineEdit(QWidget * parent = nullptr);

Q_SIGNALS:
	void commitRequested();      // Enter / Return / comma (and Tab when there is text)
	void backspaceOnEmpty();     // Backspace pressed while the field is empty
	void focusChanged(bool focused);

protected:
	bool event(QEvent * e) override;
	void keyPressEvent(QKeyEvent * e) override;
	void focusInEvent(QFocusEvent * e) override;
	void focusOutEvent(QFocusEvent * e) override;
};


// Paints a suggestion row: a dimmed "#" prefix, the tag, the part that matches what the
// user typed in bold/blue, a greyed "✓ added" marker for tags already on the part, and the
// "Create new tag …" affordance.
class TagSuggestionDelegate : public QStyledItemDelegate
{
	Q_OBJECT
public:
	explicit TagSuggestionDelegate(QObject * parent = nullptr);
	void setFilterText(const QString & text) { m_filter = text; }
	void paint(QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index) const override;
	QSize sizeHint(const QStyleOptionViewItem & option, const QModelIndex & index) const override;

private:
	QString m_filter;
};


// A token / chip field for editing a part's tags, with autocomplete from a global tag pool.
// Self-contained and reusable: hand it the current tags and the pool, read tags() back, and
// listen to changed().
class TagEditorWidget : public QFrame
{
	Q_OBJECT
public:
	TagEditorWidget(const QStringList & tags, const QStringList & pool, QWidget * parent = nullptr);

	QStringList tags() const { return m_tags; }
	void setTags(const QStringList & tags);

Q_SIGNALS:
	void changed();

protected:
	void mousePressEvent(QMouseEvent * event) override;   // click anywhere in the field focuses the input

private Q_SLOTS:
	void onTextEdited(const QString & text);
	void commitCurrent();
	void removeLastChip();
	void removeChip(TagChip * chip);
	void onInputFocusChanged(bool focused);

private:
	void buildModel();
	void updateAddedState();
	void updateCreateRow(const QString & text);
	void addChip(const QString & tag);
	bool addTag(const QString & raw);          // returns true if a chip was added
	bool containsTag(const QString & tag) const;   // case-insensitive
	bool poolHasExact(const QString & tag) const;  // case-insensitive

	static const int MaxTagLength = 30;

	FlowLayout * m_flow = nullptr;
	TagLineEdit * m_input = nullptr;
	QCompleter * m_completer = nullptr;
	QStandardItemModel * m_model = nullptr;
	QStandardItem * m_createItem = nullptr;
	TagSuggestionDelegate * m_delegate = nullptr;
	QStringList m_tags;
	QStringList m_pool;
	QList<TagChip *> m_chips;
};

#endif // TAGEDITORWIDGET_H
