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
	Q_PROPERTY(int xpos MEMBER xpos WRITE setPos)
	Q_PROPERTY(QColor backgroundInactive MEMBER backgroundInactive
			   DESIGNABLE true)
	Q_PROPERTY(
		QColor backgroundActive MEMBER backgroundActive DESIGNABLE true)
	Q_PROPERTY(QColor handle MEMBER handle DESIGNABLE true)
	Q_PROPERTY(int margin MEMBER margin DESIGNABLE true)
	Q_PROPERTY(int height MEMBER targetHeight DESIGNABLE true)

public:
	OBSToggleSwitch(QWidget *parent = nullptr);
	OBSToggleSwitch(bool defaultState, QWidget *parent = nullptr);

	QSize sizeHint() const override;

	void setPos(int x)
	{
		xpos = x;
		update();
	}

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
	int xpos;
	int targetHeight;
	int margin;

	QColor backgroundInactive;
	QColor backgroundActive;
	QColor handle;

	QPropertyAnimation *animation = nullptr;
};
