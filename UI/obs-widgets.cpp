#include <QLayout>
#include <QSizePolicy>
#include <QAbstractSpinBox>

#include "obs-widgets.hpp"

#define UNUSED_PARAMETER(param) (void)param

///
/// OBS GROUP BOX
///
OBSGroupBox::OBSGroupBox(QWidget *parent) : QFrame(parent)
{
	layout = new QGridLayout(this);
	plist = new OBSPropertiesList(this);

	QSizePolicy policy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	setSizePolicy(policy);
	setLayout(layout);

	setFixedWidth(600);

	layout->addWidget(plist, 2, 0, -1, -1);
}

OBSGroupBox::OBSGroupBox(const QString &name, QWidget *parent)
	: OBSGroupBox(parent)
{

	nameLbl = new QLabel();
	nameLbl->setText(name);
	nameLbl->setObjectName("groupName");

	layout->addWidget(nameLbl, 0, 0, Qt::AlignLeft);
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
	layout->addWidget(descLbl, 1, 0, Qt::AlignLeft);
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

void OBSGroupBox::addProperty(OBSActionBaseClass *ar)
{
	plist->addProperty(ar);
}

///
/// OBS PROPERTY LIST
///
OBSPropertiesList::OBSPropertiesList(QWidget *parent) : QFrame(parent)
{
	layout = new QVBoxLayout();
	layout->setSpacing(0);
	layout->setContentsMargins(0, 0, 0, 0);
	// layout->setSizeConstraint(QLayout::SetMinimumSize);

	//setFixedWidth(600);
	setLayout(layout);
}

void OBSPropertiesList::addProperty(OBSActionBaseClass *ar)
{
	// All non-first objects get a special name so they can have a top border.
	// Alternatively we could insert a widget here that acts as a separator and could
	// be styled separately.
	if (layout->count() > 0) {
		ar->setObjectName("secondary");
		/*
		QWidget *spacer = new QWidget(this);
		spacer->setObjectName("obsSpacer");
		spacer->setSizePolicy(QSizePolicy::Expanding,
				      QSizePolicy::Fixed);
		layout->addWidget(spacer);
		*/
	}

	ar->setParent(this);
	layout->addWidget(ar);
	adjustSize();
}

///
/// OBS ACTION ROW
///
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

///
/// OBS ACTION ROW WITH PROPERTY LIST
///

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

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
void OBSToggleSwitch::enterEvent(QEvent *e)
#else
void OBSToggleSwitch::enterEvent(QEnterEvent *e)
#endif
{
	setCursor(Qt::PointingHandCursor);
	QAbstractButton::enterEvent(e);
}

QSize OBSToggleSwitch::sizeHint() const
{
	return QSize(2 * targetHeight, targetHeight);
}

///
/// OBS SPIN BOXES
///

OBSSpinBox::OBSSpinBox(QWidget *parent) : QFrame(parent)
{
	layout = new QHBoxLayout();
	setLayout(layout);

	layout->setContentsMargins(0, 0, 0, 0);

	decr = new QPushButton("-");
	decr->setObjectName("obsSpinBoxButton");
	layout->addWidget(decr);

	sbox = new QSpinBox();
	sbox->setObjectName("obsSpinBox");
	sbox->setButtonSymbols(QAbstractSpinBox::NoButtons);
	layout->addWidget(sbox);

	incr = new QPushButton("+");
	incr->setObjectName("obsSpinBoxButton");
	layout->addWidget(incr);

	connect(decr, &QPushButton::pressed, sbox, &QSpinBox::stepDown);
	connect(incr, &QPushButton::pressed, sbox, &QSpinBox::stepUp);
}

OBSDoubleSpinBox::OBSDoubleSpinBox(QWidget *parent) : QFrame(parent)
{
	layout = new QHBoxLayout();
	setLayout(layout);

	layout->setContentsMargins(0, 0, 0, 0);

	decr = new QPushButton("-");
	decr->setObjectName("obsSpinBoxButton");
	layout->addWidget(decr);

	sbox = new QDoubleSpinBox();
	sbox->setObjectName("obsSpinBox");
	sbox->setButtonSymbols(QAbstractSpinBox::NoButtons);
	layout->addWidget(sbox);

	incr = new QPushButton("+");
	incr->setObjectName("obsSpinBoxButton");
	layout->addWidget(incr);

	connect(decr, &QPushButton::pressed, sbox, &QDoubleSpinBox::stepDown);
	connect(incr, &QPushButton::pressed, sbox, &QDoubleSpinBox::stepUp);
}
