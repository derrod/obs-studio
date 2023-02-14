#include <QLayout>
#include <QSizePolicy>

#include "obs-widgets.hpp"

#define UNUSED_PARAMETER(param) (void)param

///
/// OBS GROUP BOX
///
OBSGroupBox::OBSGroupBox(QWidget *parent) : QFrame(parent)
{
	layout = new QGridLayout(this);
	plist = new OBSPropertiesList(this);

	layout->setAlignment(Qt::AlignHCenter);

	QSizePolicy policy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	setSizePolicy(policy);
	setLayout(layout);

	layout->addWidget(plist, 2, 0, -1, -1);
}

OBSGroupBox::OBSGroupBox(const QString &name, QWidget *parent)
	: OBSGroupBox(parent)
{

	nameLbl = new QLabel();
	nameLbl->setText(name);
	nameLbl->setObjectName("groupName");

	layout->addWidget(nameLbl, 0, 0);
}

OBSGroupBox::OBSGroupBox(const QString &name, bool checkable, QWidget *parent)
	: OBSGroupBox(name, parent)
{
	if (checkable) {
		toggleSwitch = new OBSToggleSwitch(true);
		layout->addWidget(toggleSwitch, 0, 1, Qt::AlignRight);
		connect(toggleSwitch, &OBSToggleSwitch::toggled, this,
			[=](bool checked) { plist->setEnabled(checked); });
	}
}

OBSGroupBox::OBSGroupBox(const QString &name, const QString &desc,
			 QWidget *parent)
	: OBSGroupBox(name, parent)
{
	descLbl = new QLabel();
	descLbl->setText(desc);
	descLbl->setObjectName("groupSubtitle");
	layout->addWidget(descLbl, 1, 0);
}

OBSGroupBox::OBSGroupBox(const QString &name, const QString &desc,
			 bool checkable, QWidget *parent)
	: OBSGroupBox(name, desc, parent)
{
	if (checkable) {
		toggleSwitch = new OBSToggleSwitch(true);
		layout->addWidget(toggleSwitch, 0, 1, 2, 1, Qt::AlignRight);
		connect(toggleSwitch, &OBSToggleSwitch::toggled, this,
			[=](bool checked) { plist->setEnabled(checked); });
	}
}

///
/// OBS PROPERTY LIST
///
OBSPropertiesList::OBSPropertiesList(QWidget *parent) : QFrame(parent)
{
	layout = new QVBoxLayout();
	layout->setSpacing(0);
	layout->setContentsMargins(0, 0, 0, 0);

	setMaximumWidth(600);
	setLayout(layout);
}

void OBSPropertiesList::addProperty(OBSActionRow *ar)
{
	// All non-first objects get a special name so they can have a top border.
	// Alternatively we could insert a widget here that acts as a separator and could
	// be styled separately.
	if (layout->count() > 0)
		ar->setObjectName("secondary");

	ar->setParent(this);
	layout->addWidget(ar);
	adjustSize();

	// In case widget was disabled to be invisible while it contained no items
	setEnabled(true);
	setVisible(true);
}

QSize OBSPropertiesList::sizeHint() const
{
	return QSize(600, height());
}

///
/// OBS ACTION ROW
///
OBSActionRow::OBSActionRow(const QString &name, QWidget *parent)
	: QFrame(parent)
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

void OBSActionRow::setPrefix(QWidget *w)
{
	int rowspan = !!descLbl ? 2 : 1;
	prefix = w;

	// HACK: Remove text from checkbox
	QCheckBox *cbox = dynamic_cast<QCheckBox *>(w);
	if (cbox)
		cbox->setText("");

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

OBSToggleSwitch::OBSToggleSwitch(bool defaultState, QWidget *parent)
	: OBSToggleSwitch(parent)
{
	setChecked(defaultState);
	if (defaultState)
		xpos = 2 * targetHeight - targetHeight / 2;
}

void OBSToggleSwitch::paintEvent(QPaintEvent *e)
{
	UNUSED_PARAMETER(e);

	QPainter p(this);
	p.setPen(Qt::NoPen);

	p.setOpacity(isEnabled() ? 1.0f : 0.5f);
	int offset = isChecked() ? 0 : targetHeight / 2;
	float progress = (float)(xpos - offset) /
			 (float)(2 * targetHeight - targetHeight / 2);

	p.setBrush(blendColors(backgroundInactive, backgroundActive, progress));
	p.setRenderHint(QPainter::Antialiasing, true);
	p.drawRoundedRect(QRect(0, 0, 2 * targetHeight, targetHeight),
			  targetHeight / 2, targetHeight / 2);

	p.setBrush(handle);
	p.drawEllipse(QRectF(xpos - (targetHeight / 2) + margin / 2, margin / 2,
			     targetHeight - margin, targetHeight - margin));
}

// ToDo figure out if this works with keyboard control (accessibility concern)
void OBSToggleSwitch::onClicked(bool checked)
{
	if (checked) {
		animation->setStartValue(targetHeight / 2 + margin / 2);
		animation->setEndValue(2 * targetHeight - targetHeight / 2);
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
