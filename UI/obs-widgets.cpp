#include <QLayout>

#include "obs-widgets.hpp"

OBSSettingsContainer::OBSSettingsContainer(QWidget *parent, const QString &name,
					   const QString &desc)
{
	descLbl = new QLabel();
	nameLbl = new QLabel();

	QGridLayout *layout = new QGridLayout(this);

	setFrameShape(QFrame::Box);
	setFrameStyle(QFrame::StyledPanel | QFrame::Plain);
	setLineWidth(2);

	int rowspan = 1;

	if (name != nullptr) {
		nameLbl->setText(name);
		auto tmp_font = QFont("OpenSans", 12, QFont::Bold);
		nameLbl->setFont(tmp_font);
		layout->addWidget(nameLbl, 0, 0);
	}

	if (desc != nullptr) {
		descLbl->setText(desc);
		auto tmp_font2 = QFont("OpenSans", 8);
		descLbl->setFont(tmp_font2);
		layout->addWidget(descLbl, 1, 0);

		rowspan = 2;
	}

	cbox = new QCheckBox();

	layout->addWidget(cbox, 0, 1, rowspan, 1, Qt::AlignRight);
}
