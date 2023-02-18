#pragma once

#include <QCheckBox>
#include <QLabel>
#include <QWidget>
#include <QEvent>
#include <QBrush>
#include <QPropertyAnimation>
#include <QPainter>
#include <QMouseEvent>
#include <QSpinBox>
#include <QPushButton>
#include <QScrollArea>

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

/**
* OBS Spinbox with buttons
*/
class OBSSpinBox : public QFrame {
	Q_OBJECT;

public:
	OBSSpinBox(QWidget *parent = nullptr);

	// TODO: Do we care to reimplement common spinbox options or are we fine with
	// accessing them via obsspin->spinBox()->setStep(...) etc?
	QSpinBox *spinBox() const { return sbox; }

private:
	QHBoxLayout *layout;
	QPushButton *decr;
	QPushButton *incr;
	QSpinBox *sbox;
};

class OBSDoubleSpinBox : public QFrame {
	Q_OBJECT;

public:
	OBSDoubleSpinBox(QWidget *parent = nullptr);

	QDoubleSpinBox *spinBox() const { return sbox; }

private:
	QHBoxLayout *layout;
	QPushButton *decr;
	QPushButton *incr;

	QDoubleSpinBox *sbox;
};

/**
* Generic OBS property container
*/

class OBSActionBaseClass : public QFrame {
	Q_OBJECT

public:
	OBSActionBaseClass(QWidget *parent = nullptr) : QFrame(parent){};
};

class OBSActionRow : public OBSActionBaseClass {
	Q_OBJECT

public:
	OBSActionRow(const QString &name, QWidget *parent = nullptr);
	OBSActionRow(const QString &name, const QString &desc,
		     QWidget *parent = nullptr);

	void setPrefix(QWidget *w, bool auto_connect = true);
	void setSuffix(QWidget *w, bool auto_connect = true);

	void setPrefixEnabled(bool enabled);
	void setSuffixEnabled(bool enabled);

signals:
	void clicked();

protected:
	void mouseReleaseEvent(QMouseEvent *) override;
	bool hasSubtitle() const { return descLbl != nullptr; }

private:
	QGridLayout *layout;

	QLabel *nameLbl = nullptr;
	QLabel *descLbl = nullptr;

	QWidget *prefix = nullptr;
	QWidget *suffix = nullptr;
};

/**
* Proxy for QScrollArea for QSS styling
*/
class OBSScrollArea : public QScrollArea {
	Q_OBJECT
public:
	OBSScrollArea(QWidget *parent = nullptr) : QScrollArea(parent) {}
};

/**
* Collapsible Generic OBS property container
*/
class OBSCollapsibleActionRow : public OBSActionBaseClass {
	Q_OBJECT

public:
	OBSCollapsibleActionRow(const QString &name,
				const QString &desc = nullptr,
				QWidget *parent = nullptr);

	OBSToggleSwitch *getSwitch() const { return sw; }

	void addProperty(OBSActionBaseClass *ar);

private:
	void collapse(bool collapsed);

	QVBoxLayout *layout;
	QPropertyAnimation *anim;

	OBSActionRow *ar;
	OBSScrollArea *sa;
	OBSToggleSwitch *sw;
	OBSPropertiesList *plist;
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

	void addProperty(OBSActionBaseClass *ar);

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

	void addProperty(OBSActionBaseClass *ar);

private:
	QGridLayout *layout = nullptr;

	QLabel *nameLbl = nullptr;
	QLabel *descLbl = nullptr;

	OBSPropertiesList *plist = nullptr;

	OBSToggleSwitch *toggleSwitch = nullptr;
};
