#pragma once

#include <QCheckBox>
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
	OBSToggleSwitch(bool defaultState, QWidget *parent = nullptr);

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

private:
	QGridLayout *layout;

	QLabel *nameLbl = nullptr;
	QLabel *descLbl = nullptr;

	QWidget *prefix = nullptr;
	QWidget *suffix = nullptr;
};

// ToDo subclass OBSActionRow with one that has its own properties list
// that can be expanded/enabled like a groupbox itself.
// OBSExpandableActionRow or something

/**
* Container for OBS properties, mainly exists to manage the contained VLayout
*/
class OBSPropertiesList : public QFrame {
	Q_OBJECT

public:
	OBSPropertiesList(QWidget *parent = nullptr);

	void addProperty(OBSActionRow *ar);

	QSize sizeHint() const override;

private:
	QVBoxLayout *layout = nullptr;
};

/**
* 
*/
class OBSGroupBox : public QFrame {
	Q_OBJECT

public:
	OBSGroupBox(QWidget *parent = nullptr);
	OBSGroupBox(const QString &name, QWidget *parent = nullptr);
	OBSGroupBox(const QString &name, bool checkable,
		    QWidget *parent = nullptr);
	OBSGroupBox(const QString &name, const QString &desc,
		    QWidget *parent = nullptr);
	OBSGroupBox(const QString &name, const QString &desc,
		    bool checkable = false, QWidget *parent = nullptr);

	OBSPropertiesList *properties() const { return plist; }

	// ToDo add event for checkable group being enabled/disabled
	// ToDo add option for hiding properties list when disabled
	// ToDo allow setting enabled state

private:
	QGridLayout *layout = nullptr;

	QLabel *nameLbl = nullptr;
	QLabel *descLbl = nullptr;

	OBSPropertiesList *plist = nullptr;

	OBSToggleSwitch *toggleSwitch = nullptr;
};
