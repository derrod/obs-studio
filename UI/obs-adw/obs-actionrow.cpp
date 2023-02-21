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
	prefix = w;

	// HACK: Remove text from checkbox
	QCheckBox *cbox = dynamic_cast<QCheckBox *>(w);
	if (cbox)
		cbox->setText("");

	// If element is checkable, forward clicks on the widget
	QAbstractButton *abtn = dynamic_cast<QAbstractButton *>(w);
	if (auto_connect && abtn && abtn->isCheckable())
		connect(this, &OBSActionRow::clicked, abtn,
			&QAbstractButton::click);

	prefix->setParent(this);
	layout->addWidget(prefix, 0, 0, rowspan, 1, Qt::AlignLeft);
	layout->setColumnStretch(0, 5);
}

void OBSActionRow::setSuffix(QWidget *w, bool auto_connect)
{
	setPrefixEnabled(false);

	int rowspan = !!descLbl ? 2 : 1;
	suffix = w;

	// HACK: Remove text from checkbox
	QCheckBox *cbox = dynamic_cast<QCheckBox *>(w);
	if (cbox)
		cbox->setText("");

	// If element is checkable, forward clicks on the widget
	QAbstractButton *abtn = dynamic_cast<QAbstractButton *>(w);
	if (auto_connect && abtn && abtn->isCheckable())
		connect(this, &OBSActionRow::clicked, abtn,
			&QAbstractButton::click);

	suffix->setParent(this);
	layout->addWidget(suffix, 0, 2, rowspan, 1,
			  Qt::AlignRight | Qt::AlignVCenter);
}

void OBSActionRow::setPrefixEnabled(bool enabled)
{
	if (!prefix)
		return;
	if (enabled)
		setSuffixEnabled(false);
	if (enabled == prefix->isEnabled() && enabled == prefix->isVisible())
		return;

	prefix->setEnabled(enabled);
	prefix->setVisible(enabled);
}

void OBSActionRow::setSuffixEnabled(bool enabled)
{
	if (!suffix)
		return;
	if (enabled)
		setPrefixEnabled(false);
	if (enabled == suffix->isEnabled() && enabled == suffix->isVisible())
		return;

	suffix->setEnabled(enabled);
	suffix->setVisible(enabled);
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
	sw = new OBSToggleSwitch(false);
	ar->setSuffix(sw);

	// connect(sw, &OBSToggleSwitch::toggled, this, 	&OBSCollapsibleActionRow::collapse);
	connect(sw, &OBSToggleSwitch::toggled, plist,
		&OBSPropertiesList::setVisible);
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

void OBSCollapsibleActionRow::addProperty(OBSActionBaseClass *ar)
{
	plist->addProperty(ar);
}
