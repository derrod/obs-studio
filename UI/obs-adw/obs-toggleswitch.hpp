#pragma once

#include <QWidget>
#include <QEvent>
#include <QBrush>
#include <QPropertyAnimation>
#include <QPainter>
#include <QMouseEvent>
#include <QAbstractButton>

class OBSToggleSwitch : public QAbstractButton {
	Q_OBJECT
	Q_PROPERTY(int xpos MEMBER xPos WRITE setPos)
	Q_PROPERTY(QColor backgroundInactive MEMBER backgroundInactive
			   DESIGNABLE true)
	Q_PROPERTY(
		QColor backgroundActive MEMBER backgroundActive DESIGNABLE true)
	Q_PROPERTY(QColor handle MEMBER handle DESIGNABLE true)
	Q_PROPERTY(int margin MEMBER margin DESIGNABLE true)
	Q_PROPERTY(int height MEMBER targetHeight DESIGNABLE true)
	Q_PROPERTY(float blend MEMBER blend WRITE setBlend DESIGNABLE false)

public:
	OBSToggleSwitch(QWidget *parent = nullptr);
	OBSToggleSwitch(bool defaultState, QWidget *parent = nullptr);

	QSize sizeHint() const override;

	void setPos(int x)
	{
		xPos = x;
		update();
	}

	void setBlend(float newBlend)
	{
		blend = newBlend;
		update();
	}

	void setManualStatusChange(bool manual) { manualStatusChange = manual; }
	void setStatus(bool status);

protected:
	void paintEvent(QPaintEvent *) override;

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	void enterEvent(QEvent *) override;
#else
	void enterEvent(QEnterEvent *) override;
#endif

private slots:
	void onClicked(bool checked);

private:
	int xPos;
	int onPos;
	int offPos;
	int margin;
	int targetHeight;

	float blend = 0.0f;

	bool waiting = false;
	bool manualStatus = false;
	bool manualStatusChange = false;

	QColor backgroundInactive;
	QColor backgroundActive;
	QColor handle;

	QPropertyAnimation *animation = nullptr;
	QPropertyAnimation *blendAnimation = nullptr;
};
