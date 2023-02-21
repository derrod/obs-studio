#include "obs-groupbox.hpp"

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
