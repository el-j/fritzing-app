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


#include <QAbstractItemView>
#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QCompleter>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QTextDocument>
#include <QToolButton>
#include <QWidgetItem>

#include "tageditorwidget.h"

// Custom item roles carried on the suggestion model.
static const int TagRole = Qt::UserRole + 1;      // the raw tag a row would add
static const int CreateRole = Qt::UserRole + 2;   // bool: this is the "Create new tag …" row

//////////////////////////////////////////////////////////////////////
// FlowLayout -- the canonical Qt wrapping layout, plus an insertWidget().

FlowLayout::FlowLayout(QWidget * parent, int margin, int hSpacing, int vSpacing)
	: QLayout(parent), m_hSpace(hSpacing), m_vSpace(vSpacing)
{
	setContentsMargins(margin, margin, margin, margin);
}

FlowLayout::FlowLayout(int margin, int hSpacing, int vSpacing)
	: m_hSpace(hSpacing), m_vSpace(vSpacing)
{
	setContentsMargins(margin, margin, margin, margin);
}

FlowLayout::~FlowLayout()
{
	QLayoutItem * item = nullptr;
	while ((item = takeAt(0)) != nullptr) {
		delete item;
	}
}

void FlowLayout::addItem(QLayoutItem * item)
{
	m_itemList.append(item);
}

void FlowLayout::insertWidget(int index, QWidget * widget)
{
	addChildWidget(widget);
	if (index < 0 || index > m_itemList.size()) index = m_itemList.size();
	m_itemList.insert(index, new QWidgetItem(widget));
	invalidate();
}

int FlowLayout::horizontalSpacing() const
{
	if (m_hSpace >= 0) return m_hSpace;
	return smartSpacing(QStyle::PM_LayoutHorizontalSpacing);
}

int FlowLayout::verticalSpacing() const
{
	if (m_vSpace >= 0) return m_vSpace;
	return smartSpacing(QStyle::PM_LayoutVerticalSpacing);
}

int FlowLayout::count() const
{
	return m_itemList.size();
}

QLayoutItem * FlowLayout::itemAt(int index) const
{
	return m_itemList.value(index);
}

QLayoutItem * FlowLayout::takeAt(int index)
{
	if (index >= 0 && index < m_itemList.size()) {
		return m_itemList.takeAt(index);
	}
	return nullptr;
}

Qt::Orientations FlowLayout::expandingDirections() const
{
	return Qt::Orientations();
}

bool FlowLayout::hasHeightForWidth() const
{
	return true;
}

int FlowLayout::heightForWidth(int width) const
{
	return doLayout(QRect(0, 0, width, 0), true);
}

void FlowLayout::setGeometry(const QRect & rect)
{
	QLayout::setGeometry(rect);
	doLayout(rect, false);
}

QSize FlowLayout::sizeHint() const
{
	return minimumSize();
}

QSize FlowLayout::minimumSize() const
{
	QSize size;
	for (const QLayoutItem * item : m_itemList) {
		size = size.expandedTo(item->minimumSize());
	}
	const QMargins margins = contentsMargins();
	size += QSize(margins.left() + margins.right(), margins.top() + margins.bottom());
	return size;
}

int FlowLayout::doLayout(const QRect & rect, bool testOnly) const
{
	int left = 0, top = 0, right = 0, bottom = 0;
	getContentsMargins(&left, &top, &right, &bottom);
	QRect effectiveRect = rect.adjusted(+left, +top, -right, -bottom);
	int x = effectiveRect.x();
	int y = effectiveRect.y();
	int lineHeight = 0;

	for (QLayoutItem * item : m_itemList) {
		const QWidget * wid = item->widget();
		if (wid && !wid->isVisibleTo(parentWidget())) continue;

		int spaceX = horizontalSpacing();
		if (spaceX == -1) spaceX = 0;
		int spaceY = verticalSpacing();
		if (spaceY == -1) spaceY = 0;

		int nextX = x + item->sizeHint().width() + spaceX;
		if (nextX - spaceX > effectiveRect.right() && lineHeight > 0) {
			x = effectiveRect.x();
			y = y + lineHeight + spaceY;
			nextX = x + item->sizeHint().width() + spaceX;
			lineHeight = 0;
		}

		if (!testOnly) {
			QSize sz = item->sizeHint();
			if (item == m_itemList.last()) {
				// the trailing item (the inline input) grows to fill the rest of its row
				const int fill = effectiveRect.right() - x + 1;
				if (fill > sz.width()) sz.setWidth(fill);
			}
			item->setGeometry(QRect(QPoint(x, y), sz));
		}

		x = nextX;
		lineHeight = qMax(lineHeight, item->sizeHint().height());
	}
	return y + lineHeight - rect.y() + bottom;
}

int FlowLayout::smartSpacing(QStyle::PixelMetric pm) const
{
	QObject * parent = this->parent();
	if (!parent) {
		return -1;
	}
	if (parent->isWidgetType()) {
		auto * pw = static_cast<QWidget *>(parent);
		return pw->style()->pixelMetric(pm, nullptr, pw);
	}
	return static_cast<QLayout *>(parent)->spacing();
}


//////////////////////////////////////////////////////////////////////
// TagChip

TagChip::TagChip(const QString & tag, QWidget * parent) : QFrame(parent), m_tag(tag)
{
	setObjectName("TagChip");
	auto * layout = new QHBoxLayout(this);
	layout->setContentsMargins(8, 2, 4, 2);
	layout->setSpacing(3);

	// The leading "#" is render-only and dimmed; the tag text is the stored value.
	auto * label = new QLabel(this);
	label->setText(QStringLiteral("<span style='color:#9bb6e2;'>#</span>"
	                              "<span style='color:#1f5fb5;'>%1</span>").arg(tag.toHtmlEscaped()));
	label->setTextFormat(Qt::RichText);
	layout->addWidget(label);

	auto * remove = new QToolButton(this);
	remove->setObjectName("TagChipRemove");
	remove->setText(QString(QChar(ushort(0x00D7))));   // × U+00D7 multiplication sign
	remove->setCursor(Qt::ArrowCursor);
	remove->setFocusPolicy(Qt::NoFocus);
	remove->setToolTip(tr("Remove this tag"));
	remove->setAutoRaise(true);
	connect(remove, &QToolButton::clicked, this, [this]() { Q_EMIT removeRequested(this); });
	layout->addWidget(remove);
}


//////////////////////////////////////////////////////////////////////
// TagLineEdit

TagLineEdit::TagLineEdit(QWidget * parent) : QLineEdit(parent)
{
	setObjectName("TagLineEdit");
	setFrame(false);
	setMaxLength(30);
	setPlaceholderText(tr("add a tag…"));
}

bool TagLineEdit::event(QEvent * e)
{
	// Tab normally moves focus; intercept it as a commit gesture, but only when there is
	// something to commit so the user can still tab out of an empty field.
	if (e->type() == QEvent::KeyPress) {
		auto * ke = static_cast<QKeyEvent *>(e);
		if (ke->key() == Qt::Key_Tab && !text().trimmed().isEmpty()) {
			Q_EMIT commitRequested();
			return true;
		}
	}
	return QLineEdit::event(e);
}

void TagLineEdit::keyPressEvent(QKeyEvent * e)
{
	// Up / Down / Return / Escape are consumed by the QCompleter's event filter while its
	// popup is visible, so here they only fire when the popup is closed.
	switch (e->key()) {
	case Qt::Key_Return:
	case Qt::Key_Enter:
	case Qt::Key_Comma:        // a comma ends a tag and is not part of it
		Q_EMIT commitRequested();
		return;
	case Qt::Key_Backspace:
		if (text().isEmpty()) {
			Q_EMIT backspaceOnEmpty();
			return;
		}
		break;
	default:
		break;
	}
	QLineEdit::keyPressEvent(e);
}

void TagLineEdit::focusInEvent(QFocusEvent * e)
{
	QLineEdit::focusInEvent(e);
	Q_EMIT focusChanged(true);
}

void TagLineEdit::focusOutEvent(QFocusEvent * e)
{
	QLineEdit::focusOutEvent(e);
	Q_EMIT focusChanged(false);
}


//////////////////////////////////////////////////////////////////////
// TagSuggestionDelegate

TagSuggestionDelegate::TagSuggestionDelegate(QObject * parent) : QStyledItemDelegate(parent)
{
}

void TagSuggestionDelegate::paint(QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index) const
{
	QStyleOptionViewItem opt = option;
	initStyleOption(&opt, index);

	const bool isCreate = index.data(CreateRole).toBool();
	const bool enabled = opt.state & QStyle::State_Enabled;
	const bool added = !enabled && !isCreate;            // a pool tag already on this part
	const QString value = index.data(Qt::DisplayRole).toString();

	QString plain;
	if (isCreate) {
		plain = QStringLiteral("+  ") + tr("Create new tag “%1”").arg(value);
	} else {
		plain = QStringLiteral("#") + value;
	}

	opt.text.clear();
	QStyle * style = opt.widget ? opt.widget->style() : QApplication::style();
	style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);   // background / selection

	QString html = plain.toHtmlEscaped();
	if (!m_filter.isEmpty()) {
		int idx = plain.indexOf(m_filter, 0, Qt::CaseInsensitive);
		if (idx >= 0) {
			const QString before = plain.left(idx).toHtmlEscaped();
			const QString match  = plain.mid(idx, m_filter.length()).toHtmlEscaped();
			const QString after  = plain.mid(idx + m_filter.length()).toHtmlEscaped();
			html = before + QStringLiteral("<b><span style='color:#1f5fb5;'>") + match
			       + QStringLiteral("</span></b>") + after;
		}
	}
	if (added) {
		html = QStringLiteral("<span style='color:#9a9a9a;'>") + html + QStringLiteral("</span>");
	} else if (isCreate) {
		html = QStringLiteral("<span style='color:#5b6470;'>") + html + QStringLiteral("</span>");
	}

	const QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &opt, opt.widget);

	QTextDocument doc;
	doc.setHtml(html);
	doc.setDefaultFont(opt.font);
	doc.setDocumentMargin(0);

	// Selection uses a light-blue background (see the popup stylesheet), so keep the dark text
	// rather than switching to the palette's highlighted-text colour.
	painter->save();
	painter->translate(textRect.topLeft());
	const double dy = (textRect.height() - doc.size().height()) / 2.0;
	painter->translate(0, dy > 0 ? dy : 0);
	QAbstractTextDocumentLayout::PaintContext ctx;
	doc.documentLayout()->draw(painter, ctx);
	painter->restore();

	// "✓ added" marker, right-aligned and dimmed, for tags already on the part.
	if (added) {
		painter->save();
		painter->setPen(QColor(0x9a, 0x9a, 0x9a));
		painter->drawText(textRect.adjusted(0, 0, -2, 0), Qt::AlignRight | Qt::AlignVCenter,
		                  QStringLiteral("✓ ") + tr("added"));
		painter->restore();
	}
}

QSize TagSuggestionDelegate::sizeHint(const QStyleOptionViewItem & option, const QModelIndex & index) const
{
	QSize s = QStyledItemDelegate::sizeHint(option, index);
	s.setHeight(qMax(s.height(), 22));
	return s;
}


//////////////////////////////////////////////////////////////////////
// TagEditorWidget

TagEditorWidget::TagEditorWidget(const QStringList & tags, const QStringList & pool, QWidget * parent)
	: QFrame(parent), m_pool(pool)
{
	setObjectName("TagEditorWidget");
	setStyleSheet(QStringLiteral(
	    "#TagEditorWidget {"
	    "  background: #ffffff;"
	    "  border: 1px solid #b9c0cc;"
	    "  border-radius: 4px;"
	    "  min-height: 30px;"
	    "}"
	    "#TagEditorWidget[tagFocus=\"true\"] {"
	    "  border: 1px solid #1f5fb5;"
	    "}"
	    "TagChip {"
	    "  background: #eff4fd;"
	    "  border: 1px solid #cdddf5;"
	    "  border-radius: 5px;"
	    "}"
	    "TagChip QLabel { background: transparent; border: none; }"
	    "#TagChipRemove {"
	    "  border: none; background: transparent; color: #9aa6b8;"
	    "  font-weight: bold; padding: 0px 2px;"
	    "}"
	    "#TagChipRemove:hover { color: #1f5fb5; }"
	    "#TagLineEdit { border: none; background: transparent; }"
	));
	setProperty("tagFocus", false);
	setCursor(Qt::IBeamCursor);

	m_flow = new FlowLayout(this, 5, 5, 5);

	m_input = new TagLineEdit(this);
	m_input->setMinimumWidth(120);
	m_flow->addWidget(m_input);

	// Autocomplete: a QCompleter drives the popup (positioning, navigation, dismissal); a
	// delegate renders the rows (highlight, "✓ added", "Create new tag …").
	m_delegate = new TagSuggestionDelegate(this);
	buildModel();
	m_completer = new QCompleter(m_model, this);
	m_completer->setWidget(m_input);
	m_completer->setCaseSensitivity(Qt::CaseInsensitive);
	m_completer->setFilterMode(Qt::MatchContains);
	m_completer->setCompletionMode(QCompleter::PopupCompletion);
	m_completer->setCompletionRole(Qt::DisplayRole);   // match against the visible tag text
	m_completer->setMaxVisibleItems(8);
	m_completer->popup()->setItemDelegate(m_delegate);
	m_completer->popup()->setStyleSheet(QStringLiteral(
	    "QAbstractItemView {"
	    "  background: #ffffff;"
	    "  border: 1px solid #9aa7bd;"
	    "  outline: 0px;"
	    "  padding: 2px;"
	    "}"
	    "QAbstractItemView::item { padding: 4px 8px; border: 0px; }"
	    "QAbstractItemView::item:selected { background: #dce7fb; }"
	));

	connect(m_input, &QLineEdit::textEdited, this, &TagEditorWidget::onTextEdited);
	connect(m_input, &TagLineEdit::commitRequested, this, &TagEditorWidget::commitCurrent);
	connect(m_input, &TagLineEdit::backspaceOnEmpty, this, &TagEditorWidget::removeLastChip);
	connect(m_input, &TagLineEdit::focusChanged, this, &TagEditorWidget::onInputFocusChanged);
	connect(m_completer, QOverload<const QString &>::of(&QCompleter::activated),
	        this, [this](const QString & text) {
		addTag(text);
		m_input->clear();
	});

	setTags(tags);
}

void TagEditorWidget::buildModel()
{
	m_model = new QStandardItemModel(this);
	for (const QString & t : m_pool) {
		if (t.trimmed().isEmpty()) continue;
		auto * item = new QStandardItem(t);   // DisplayRole = plain tag, used for matching + commit
		item->setData(t, TagRole);
		item->setData(false, CreateRole);
		item->setEditable(false);
		m_model->appendRow(item);
	}
	// The "Create new tag …" row lives last; its text is filled in as the user types.
	m_createItem = new QStandardItem(QString());
	m_createItem->setData(true, CreateRole);
	m_createItem->setEditable(false);
	m_model->appendRow(m_createItem);
}

bool TagEditorWidget::containsTag(const QString & tag) const
{
	for (const QString & t : m_tags) {
		if (t.compare(tag, Qt::CaseInsensitive) == 0) return true;
	}
	return false;
}

bool TagEditorWidget::poolHasExact(const QString & tag) const
{
	for (const QString & t : m_pool) {
		if (t.compare(tag, Qt::CaseInsensitive) == 0) return true;
	}
	return false;
}

void TagEditorWidget::updateAddedState()
{
	if (!m_model) return;
	for (int i = 0; i < m_model->rowCount(); ++i) {
		QStandardItem * item = m_model->item(i);
		if (item == m_createItem) continue;
		const bool added = containsTag(item->data(TagRole).toString());
		item->setEnabled(!added);       // already-added tags appear disabled ("✓ added")
		item->setSelectable(!added);
	}
}

void TagEditorWidget::updateCreateRow(const QString & text)
{
	if (!m_createItem) return;
	const QString t = text.trimmed();
	// Offer "Create new tag" only when nothing in the pool matches it exactly and it is not
	// already on the part. An empty display string makes the row drop out of a contains-filter.
	const bool show = !t.isEmpty() && !containsTag(t) && !poolHasExact(t);
	m_createItem->setData(show ? t : QString(), Qt::DisplayRole);
	m_createItem->setData(t, TagRole);
}

void TagEditorWidget::setTags(const QStringList & tags)
{
	for (TagChip * chip : m_chips) {
		m_flow->removeWidget(chip);
		chip->deleteLater();
	}
	m_chips.clear();
	m_tags.clear();
	for (const QString & raw : tags) {
		QString tag = raw.trimmed();
		if (tag.isEmpty() || containsTag(tag)) continue;
		if (tag.length() > MaxTagLength) tag = tag.left(MaxTagLength).trimmed();
		m_tags << tag;
		addChip(tag);
	}
	updateAddedState();
}

void TagEditorWidget::addChip(const QString & tag)
{
	auto * chip = new TagChip(tag, this);
	connect(chip, &TagChip::removeRequested, this, &TagEditorWidget::removeChip);
	int idx = m_flow->count() - 1;   // keep the input as the trailing item
	if (idx < 0) idx = 0;
	m_flow->insertWidget(idx, chip);
	m_chips << chip;
}

bool TagEditorWidget::addTag(const QString & raw)
{
	QString tag = raw.trimmed();
	if (tag.isEmpty()) return false;
	if (tag.length() > MaxTagLength) tag = tag.left(MaxTagLength).trimmed();
	if (tag.isEmpty() || containsTag(tag)) return false;
	m_tags << tag;
	addChip(tag);
	updateAddedState();
	Q_EMIT changed();
	return true;
}

void TagEditorWidget::removeChip(TagChip * chip)
{
	if (!chip) return;
	for (int i = 0; i < m_tags.size(); ++i) {
		if (m_tags.at(i).compare(chip->tag(), Qt::CaseInsensitive) == 0) {
			m_tags.removeAt(i);
			break;
		}
	}
	m_chips.removeAll(chip);
	m_flow->removeWidget(chip);
	chip->deleteLater();
	updateAddedState();
	Q_EMIT changed();
}

void TagEditorWidget::removeLastChip()
{
	if (m_chips.isEmpty()) return;
	removeChip(m_chips.last());
}

void TagEditorWidget::onTextEdited(const QString & text)
{
	m_delegate->setFilterText(text.trimmed());
	updateCreateRow(text);
	if (text.trimmed().isEmpty()) {
		m_completer->popup()->hide();
		return;
	}
	m_completer->setCompletionPrefix(text);
	m_completer->complete();
}

void TagEditorWidget::commitCurrent()
{
	// Enter / Tab / comma: commit the highlighted suggestion if the user navigated to one,
	// otherwise the literal typed text.
	QString tag = m_input->text();
	QAbstractItemView * popup = m_completer->popup();
	if (popup && popup->isVisible()) {
		const QModelIndex idx = popup->currentIndex();
		if (idx.isValid() && (idx.flags() & Qt::ItemIsEnabled)) {
			tag = idx.data(Qt::DisplayRole).toString();
		}
		popup->hide();
	}
	addTag(tag);
	m_input->clear();
}

void TagEditorWidget::onInputFocusChanged(bool focused)
{
	// Resting vs focused border. The completer manages its own popup lifetime (it hides on
	// blur), so there is nothing else to do here.
	setProperty("tagFocus", focused);
	style()->unpolish(this);
	style()->polish(this);
}

void TagEditorWidget::mousePressEvent(QMouseEvent * event)
{
	// Clicking anywhere in the field (the gaps around the chips) focuses the input, so the whole
	// bordered container behaves like one text field.
	if (m_input) m_input->setFocus();
	QFrame::mousePressEvent(event);
}
