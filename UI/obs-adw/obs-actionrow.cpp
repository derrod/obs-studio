#include <QSizePolicy>

#include "obs-actionrow.hpp"

OBSActionRow::OBSActionRow(const QString &name, QWidget *parent)
	: OBSActionBaseClass(parent)
{
	layout = new QGridLayout(this);

	QSizePolicy policy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
	setSizePolicy(policy);
	setLayout(layout);

	layout->setColumnMinimumWidth(0, 0);
	layout->setColumnStretch(0, 0);
	layout->setColumnStretch(1, 40);
	layout->setColumnStretch(2, 55);

	nameLbl = new QLabel();
	nameLbl->setText(name);
	nameLbl->setObjectName("actionRowName");

	layout->addWidget(nameLbl, 0, 1, Qt::AlignLeft);
}

OBSActionRow::OBSActionRow(const QString &name, const QString &desc,
			   QWidget *parent)
	: OBSActionRow(name, parent)
{
	descLbl = new QLabel();
	descLbl->setText(desc);
	descLbl->setObjectName("actionRowSubtitle");

	layout->addWidget(descLbl, 1, 1, Qt::AlignLeft);
}

void OBSActionRow::setPrefix(QWidget *w, bool auto_connect)
{
	setSuffixEnabled(false);

	int rowspan = !!descLbl ? 2 : 1;
	_prefix = w;

	// HACK: Remove text from checkbox
	QCheckBox *cbox = dynamic_cast<QCheckBox *>(w);
	if (cbox)
		cbox->setText("");

	// If element is checkable, forward clicks on the widget
	QAbstractButton *abtn = dynamic_cast<QAbstractButton *>(w);
	if (auto_connect && abtn && abtn->isCheckable())
		connect(this, &OBSActionRow::clicked, abtn,
			&QAbstractButton::click);

	_prefix->setParent(this);
	layout->addWidget(_prefix, 0, 0, rowspan, 1, Qt::AlignLeft);
	layout->setColumnStretch(0, 5);
}

void OBSActionRow::setSuffix(QWidget *w, bool auto_connect)
{
	setPrefixEnabled(false);

	int rowspan = !!descLbl ? 2 : 1;
	_suffix = w;

	// HACK: Remove text from checkbox
	QCheckBox *cbox = dynamic_cast<QCheckBox *>(w);
	if (cbox)
		cbox->setText("");

	// If element is checkable, forward clicks on the widget
	QAbstractButton *abtn = dynamic_cast<QAbstractButton *>(w);
	if (auto_connect && abtn && abtn->isCheckable())
		connect(this, &OBSActionRow::clicked, abtn,
			&QAbstractButton::click);

	_suffix->setParent(this);
	layout->addWidget(_suffix, 0, 2, rowspan, 1,
			  Qt::AlignRight | Qt::AlignVCenter);
}

void OBSActionRow::setPrefixEnabled(bool enabled)
{
	if (!_prefix)
		return;
	if (enabled)
		setSuffixEnabled(false);
	if (enabled == _prefix->isEnabled() && enabled == _prefix->isVisible())
		return;

	_prefix->setEnabled(enabled);
	_prefix->setVisible(enabled);
}

void OBSActionRow::setSuffixEnabled(bool enabled)
{
	if (!_suffix)
		return;
	if (enabled)
		setPrefixEnabled(false);
	if (enabled == _suffix->isEnabled() && enabled == _suffix->isVisible())
		return;

	_suffix->setEnabled(enabled);
	_suffix->setVisible(enabled);
}

void OBSActionRow::mouseReleaseEvent(QMouseEvent *e)
{
	if (e->button() & Qt::LeftButton) {
		emit clicked();
	}
	QFrame::mouseReleaseEvent(e);
}

/*
* ActionRow variant that can be expanded to show another properties list
*/
OBSCollapsibleActionRow::OBSCollapsibleActionRow(const QString &name,
						 const QString &desc,
						 bool toggleable,
						 QWidget *parent)
	: OBSActionBaseClass(parent)
{
	layout = new QVBoxLayout;
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);
	setLayout(layout);

	if (desc != nullptr)
		ar = new OBSActionRow(name, desc, this);
	else
		ar = new OBSActionRow(name, this);

	layout->addWidget(ar);

	plist = new OBSPropertiesList(this);
	plist->setVisible(false);
	layout->addWidget(plist);

	// ToDo, make this a property
	extendDown = QPixmap(":res/images/hide.svg");
	QTransform tr;
	tr.rotate(180);
	extendUp = extendDown.transformed(tr);

	extendIcon = new QLabel(this);
	extendIcon->setPixmap(extendDown);

	/*
	sa = new OBSScrollArea(this);

	sa->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	sa->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	sa->setContentsMargins(0, 0, 0, 0);
	sa->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
	sa->setAlignment(Qt::AlignHCenter);
	sa->setWidgetResizable(false);
	sa->setWidget(plist);

	anim = new QPropertyAnimation(sa, "maximumHeight", this);
	layout->addWidget(sa);
	*/

	if (toggleable) {
		plist->setEnabled(false);
		sw = new OBSToggleSwitch(false);
		QWidget *multiSuffix = new QWidget(ar);
		QHBoxLayout *multiLayout = new QHBoxLayout;

		multiLayout->setContentsMargins(0, 0, 0, 0);
		multiLayout->addWidget(sw);
		multiLayout->addWidget(extendIcon);

		multiSuffix->setLayout(multiLayout);

		ar->setSuffix(multiSuffix, false);
		connect(sw, &OBSToggleSwitch::toggled, plist,
			&OBSPropertiesList::setEnabled);
	} else {
		ar->setSuffix(extendIcon);
	}

	connect(ar, &OBSActionRow::clicked, this,
		&OBSCollapsibleActionRow::toggleVisibility);
}

void OBSCollapsibleActionRow::collapse(bool enabled)
{
	if (!enabled) {
		anim->setStartValue(plist->sizeHint().height());
		anim->setEndValue(0);
		anim->setDuration(1200);
		anim->start();
	} else {
		anim->setStartValue(0);
		anim->setEndValue(plist->sizeHint().height());
		anim->setDuration(1200);
		anim->start();
	}
}

void OBSCollapsibleActionRow::toggleVisibility()
{
	bool visible = !plist->isVisible();

	plist->setVisible(visible);
	extendIcon->setPixmap(visible ? extendUp : extendDown);
}

void OBSCollapsibleActionRow::addProperty(OBSActionBaseClass *ar)
{
	plist->addProperty(ar);
}
