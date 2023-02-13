#pragma once

#include <QCheckbox>
#include <QLabel>
#include <QWidget>
#include <QEvent>
#include <QBrush>
#include <QPropertyAnimation>
#include <QPainter>
#include <QMouseEvent>

class OBSPropertiesList;

/*
* Custom OBS Widgets inspired by GNOME/Adawaita styling
*/

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

	QSize sizeHint() const override;

	void setPos(int x)
	{
		xpos = x;
		update();
	}

protected:
	void paintEvent(QPaintEvent *) override;
	void enterEvent(QEnterEvent *) override;

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

/**
* Generic OBS property container
*/

class OBSActionRow : public QFrame {
	Q_OBJECT

public:
	OBSActionRow(const QString &name, QWidget *parent = nullptr);
	OBSActionRow(const QString &name, const QString &desc,
		     QWidget *parent = nullptr);

	void setPrefix(QWidget *w);
	void setSuffix(QWidget *w);

	OBSPropertiesList *properties() const { return plist; }

private:
	QGridLayout *layout;
	QVBoxLayout *vlayout;

	QLabel *nameLbl = nullptr;
	QLabel *descLbl = nullptr;

	QWidget *prefix = nullptr;
	QWidget *suffix = nullptr;

	OBSPropertiesList *plist = nullptr;
};

/**
* Container for OBS properties, mainly exists to manage the contained VLayout
*/
class OBSPropertiesList : public QFrame {
	Q_OBJECT

public:
	OBSPropertiesList(QWidget *parent = nullptr);

	void addProperty(OBSActionRow *ar);

private:
	QVBoxLayout *layout = nullptr;
};

/**
* 
*/
class OBSGroupBox : public QFrame {
	Q_OBJECT

public:
	OBSGroupBox(const QString &name, QWidget *parent = nullptr);
	OBSGroupBox(const QString &name, bool checkable,
		    QWidget *parent = nullptr);
	OBSGroupBox(const QString &name, const QString &desc,
		    bool checkable = false, QWidget *parent = nullptr);
	OBSGroupBox(const QString &name, const QString &desc,
		    QWidget *parent = nullptr);

	OBSPropertiesList *properties() const { return plist; }

private:
	QGridLayout *layout = nullptr;

	QLabel *nameLbl = nullptr;
	QLabel *descLbl = nullptr;

	OBSPropertiesList *plist = nullptr;

	OBSToggleSwitch *toggleSwitch = nullptr;
};
