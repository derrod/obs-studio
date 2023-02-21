#include "obs-toggleswitch.hpp"

#define UNUSED_PARAMETER(param) (void)param

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
	  animation(new QPropertyAnimation(this, "xpos", this)),
	  blendAnimation(new QPropertyAnimation(this, "blend", this))
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
	manualStatus = defaultState;
	if (defaultState)
		xpos = 2 * targetHeight - targetHeight / 2;
}

void OBSToggleSwitch::paintEvent(QPaintEvent *e)
{
	UNUSED_PARAMETER(e);

	QPainter p(this);
	p.setPen(Qt::NoPen);

	p.setOpacity(isEnabled() ? 1.0f : 0.5f);

	if (!manualStatusChange) {
		int offset = isChecked() ? 0 : targetHeight / 2;
		blend = (float)(xpos - offset) /
			(float)(2 * targetHeight - targetHeight / 2);
	}

	p.setBrush(blendColors(backgroundInactive, backgroundActive, blend));
	p.setRenderHint(QPainter::Antialiasing, true);
	p.drawRoundedRect(QRect(0, 0, 2 * targetHeight, targetHeight),
			  targetHeight / 2, targetHeight / 2);

	p.setBrush(handle);
	p.drawEllipse(QRectF(xpos - (targetHeight / 2) + margin / 2, margin / 2,
			     targetHeight - margin, targetHeight - margin));
}

void OBSToggleSwitch::onClicked(bool checked)
{
	if (checked) {
		animation->setStartValue(targetHeight / 2 + margin / 2);
		animation->setEndValue(2 * targetHeight - targetHeight / 2);
	} else {
		animation->setStartValue(xpos);
		animation->setEndValue(targetHeight / 2);
	}

	animation->setDuration(120);
	animation->start();
}

void OBSToggleSwitch::setStatus(bool status)
{
	if (!manualStatusChange)
		return;
	if (status == manualStatus)
		return;

	manualStatus = status;
	if (manualStatus) {
		blendAnimation->setStartValue(0.0f);
		blendAnimation->setEndValue(1.0f);
	} else {
		blendAnimation->setStartValue(1.0f);
		blendAnimation->setEndValue(0.0f);
	}

	blendAnimation->setEasingCurve(QEasingCurve::InOutCubic);
	blendAnimation->setDuration(240);
	blendAnimation->start();
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
