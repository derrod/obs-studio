#include <QLayout>
#include <QSizePolicy>

#include "obs-widgets.hpp"

///
/// OBS GROUP BOX
///
OBSGroupBox::OBSGroupBox(const QString &name, QWidget *parent) : QFrame(parent)
{
	layout = new QGridLayout(this);
	plist = new OBSPropertiesList(this);
	setLayout(layout);

	nameLbl = new QLabel();
	nameLbl->setText(name);
	nameLbl->setObjectName("groupName");

	layout->addWidget(nameLbl, 0, 0);
	layout->addWidget(plist, 2, 0, -1, -1);
}

OBSGroupBox::OBSGroupBox(const QString &name, bool checkable, QWidget *parent)
	: OBSGroupBox(name, parent)
{
	if (checkable) {
		toggleSwitch = new OBSToggleSwitch();
		layout->addWidget(toggleSwitch, 0, 1, 1, 1, Qt::AlignRight);
		connect(toggleSwitch, &OBSToggleSwitch::toggled, this,
			[=](bool checked) { plist->setEnabled(checked); });
	}
}

OBSGroupBox::OBSGroupBox(const QString &name, const QString &desc,
			 bool checkable, QWidget *parent)
	: OBSGroupBox(name, parent)
{
	descLbl = new QLabel();
	descLbl->setText(desc);
	descLbl->setObjectName("groupSubtitle");
	layout->addWidget(descLbl, 1, 0);

	if (checkable) {
		toggleSwitch = new OBSToggleSwitch();
		layout->addWidget(toggleSwitch, 0, 1, 2, 1, Qt::AlignRight);
		connect(toggleSwitch, &OBSToggleSwitch::toggled, this,
			[=](bool checked) { plist->setEnabled(checked); });
	}
}

OBSGroupBox::OBSGroupBox(const QString &name, const QString &desc,
			 QWidget *parent)
	: OBSGroupBox(name, desc, false, parent)
{
}

///
/// OBS PROPERTY LIST
///
OBSPropertiesList::OBSPropertiesList(QWidget *parent) : QFrame(parent)
{
	layout = new QVBoxLayout();
	setLayout(layout);
	//QSizePolicy policy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	//setSizePolicy(policy);
}

void OBSPropertiesList::addProperty(OBSActionRow *ar)
{
	if (layout->count() > 0)
		layout->addSpacing(1);

	ar->setParent(this);
	layout->addWidget(ar);

	setEnabled(true);
	setVisible(true);
}

///
/// OBS ACTION ROW
///
OBSActionRow::OBSActionRow(const QString &name, QWidget *parent)
	: QFrame(parent)
{
	layout = new QGridLayout(this);
	vlayout = new QVBoxLayout(this);
	plist = new OBSPropertiesList(this);

	vlayout->addLayout(layout);
	setLayout(vlayout);

	layout->setColumnMinimumWidth(0, 0);
	layout->setColumnStretch(0, 0);
	layout->setColumnStretch(1, 40);
	layout->setColumnStretch(2, 55);

	nameLbl = new QLabel();
	nameLbl->setText(name);
	nameLbl->setObjectName("actionRowName");

	layout->addWidget(nameLbl, 0, 1, Qt::AlignLeft);

	plist->setVisible(false);
	plist->setEnabled(false);
	vlayout->addWidget(plist);
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

void OBSActionRow::setPrefix(QWidget *w)
{
	int rowspan = !!descLbl ? 2 : 1;
	prefix = w;

	prefix->setParent(this);
	layout->addWidget(prefix, 0, 0, rowspan, 1, Qt::AlignLeft);
	layout->setColumnStretch(0, 5);
}

void OBSActionRow::setSuffix(QWidget *w)
{
	int rowspan = !!descLbl ? 2 : 1;
	suffix = w;

	suffix->setParent(this);
	layout->addWidget(suffix, 0, 2, rowspan, 1,
			  Qt::AlignRight | Qt::AlignVCenter);
	suffix->adjustSize();
}

///
/// OBS TOGGLE SWITCH
///

static QColor blendColors(const QColor &color1, const QColor &color2,
			  float ratio)
{
	int r = color1.red() * (1 - ratio) + color2.red() * ratio;
	int g = color1.green() * (1 - ratio) + color2.green() * ratio;
	int b = color1.blue() * (1 - ratio) + color2.blue() * ratio;

	return QColor(r, g, b, 255);
}

OBSToggleSwitch::OBSToggleSwitch(QWidget *parent)
	: QAbstractButton(parent),
	  targetHeight(24),
	  margin(6),
	  animation(new QPropertyAnimation(this, "xpos", this))
{
	xpos = targetHeight / 2;
	setCheckable(true);
	setFocusPolicy(Qt::TabFocus);

	connect(this, &OBSToggleSwitch::clicked, this,
		&OBSToggleSwitch::onClicked);
}

void OBSToggleSwitch::paintEvent(QPaintEvent *e)
{
	QPainter p(this);
	p.setPen(Qt::NoPen);

	p.setOpacity(isEnabled() ? 1.0f : 0.5f);
	int offset = isChecked() ? 0 : targetHeight / 2;
	float progress =
		(float)(xpos - offset) / (float)(width() - targetHeight / 2);

	p.setBrush(blendColors(backgroundInactive, backgroundActive, progress));
	p.setRenderHint(QPainter::Antialiasing, true);
	p.drawRoundedRect(QRect(0, 0, width(), targetHeight), targetHeight / 2,
			  targetHeight / 2);

	p.setBrush(handle);
	p.drawEllipse(QRectF(xpos - (targetHeight / 2) + margin / 2, margin / 2,
			     targetHeight - margin, targetHeight - margin));
}

// ToDo figure out if this works with keyboard control (accessibility concern)
void OBSToggleSwitch::onClicked(bool checked)
{
	if (checked) {
		animation->setStartValue(targetHeight / 2 + margin / 2);
		animation->setEndValue(width() - targetHeight / 2);
		animation->setDuration(120);
		animation->start();
	} else {
		animation->setStartValue(xpos);
		animation->setEndValue(targetHeight / 2);
		animation->setDuration(120);
		animation->start();
	}
}

void OBSToggleSwitch::enterEvent(QEnterEvent *e)
{
	setCursor(Qt::PointingHandCursor);
	QAbstractButton::enterEvent(e);
}

QSize OBSToggleSwitch::sizeHint() const
{
	return QSize(2 * targetHeight, targetHeight);
}
