#pragma once

#include <QFrame>
#include <QWidget>
#include <QLayout>
#include <QSpinBox>
#include <QPushButton>

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
