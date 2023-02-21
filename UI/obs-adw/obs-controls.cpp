#include "obs-controls.hpp"

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
