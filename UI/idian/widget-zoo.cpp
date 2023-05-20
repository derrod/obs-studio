/******************************************************************************
    Copyright (C) 2023 by Dennis Sädtler <dennis@obsproject.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include <QTimer>
#include <QCheckBox>
#include <QComboBox>
#include <qtlottiewidget.h>

#include <QMediaPlayer>
#include <QVideoWidget>

#include "obs-widgets.hpp"
#include "widget-zoo.hpp"
#include "platform.hpp"

// This file is a temporary playground for building new widgets,
// it may be removed in the future.

IdianZoo::IdianZoo(QWidget *parent) : QDialog(parent), ui(new Ui_IdianZoo)
{
	ui->setupUi(this);

	OBSGroupBox *test;
	OBSActionRow *tmp;

	QComboBox *cbox = new QComboBox;
	cbox->addItem("Test 1");
	cbox->addItem("Test 2");

	/* Lottie! */

	/* ui->scrollAreaWidgetContents->layout()->addWidget(
		new QLabel("QMovie test:"));

	auto wat = new QLabel(this);
	auto movie = new QMovie(this);

	std::string gif_path;
	GetDataFilePath("themes/lottie/test.gif", gif_path);
	movie->setFileName(QString::fromStdString(gif_path));

	wat->setMovie(movie);
	movie->start();

	ui->scrollAreaWidgetContents->layout()->addWidget(wat);
	*/

	ui->scrollAreaWidgetContents->layout()->addWidget(
		new QLabel("QVideoWidget test:"));

	auto player = new QMediaPlayer(this);

	std::string gif_path;
	GetDataFilePath("themes/lottie/test.mp4", gif_path);
	QUrl path(QString::fromStdString(gif_path));
	player->setSource(path);
	player->setLoops(QMediaPlayer::Infinite);

	auto videowidget = new QVideoWidget(this);
	videowidget->setFixedSize(350, 350);
	player->setVideoOutput(videowidget);
	player->play();
	videowidget->show();

	ui->scrollAreaWidgetContents->layout()->addWidget(videowidget);

	ui->scrollAreaWidgetContents->layout()->addWidget(
		new QLabel("Lottie Test:"));

	std::string lottie_path;
	GetDataFilePath("themes/lottie/test.json", lottie_path);

	auto lottieWidget = new QtLottieWidget(this);
	lottieWidget->setFixedSize(350, 350);
	lottieWidget->setSource(
		QUrl::fromLocalFile(QString::fromStdString(lottie_path)));
	/* connect(lottieWidget, &QtLottieWidget::sourceSizeChanged, this,
		[lottieWidget]() {
		lottieWidget->resize(lottieWidget->sourceSize());
		});
	*/

	ui->scrollAreaWidgetContents->layout()->addWidget(lottieWidget);

	/* Group box 1 */
	test = new OBSGroupBox(this);

	tmp = new OBSActionRow(QString("Row with a dropdown"));
	tmp->setSuffix(cbox);
	test->properties()->addRow(tmp);

	cbox = new QComboBox;
	cbox->addItem("Test 3");
	cbox->addItem("Test 4");
	tmp = new OBSActionRow(QString("Row with a dropdown"),
			       QString("And a subtitle!"));
	tmp->setSuffix(cbox);
	test->properties()->addRow(tmp);

	tmp = new OBSActionRow(QString("Toggle Switch"));
	tmp->setSuffix(new OBSToggleSwitch());
	test->properties()->addRow(tmp);
	ui->scrollAreaWidgetContents->layout()->addWidget(test);

	tmp = new OBSActionRow(QString("Delayed toggle switch"),
			       QString("The state can be set separately"));
	auto tswitch = new OBSToggleSwitch;
	tswitch->setManualStatusChange(true);
	connect(tswitch, &OBSToggleSwitch::toggled, this, [=](bool toggled) {
		QTimer::singleShot(1000,
				   [=]() { tswitch->setStatus(toggled); });
	});
	tmp->setSuffix(tswitch);
	test->properties()->addRow(tmp);

	/* Group box 2 */
	test = new OBSGroupBox(QString("Just a few checkboxes"), this);

	//QTimer::singleShot(10000, this, [=]() { test->properties()->clear(); });

	tmp = new OBSActionRow(QString("Box 1"));
	tmp->setPrefix(new QCheckBox);
	test->properties()->addRow(tmp);

	tmp = new OBSActionRow(QString("Box 2"));
	tmp->setPrefix(new QCheckBox);
	test->properties()->addRow(tmp);

	ui->scrollAreaWidgetContents->layout()->addWidget(test);

	/* Group box 2 */
	test = new OBSGroupBox(QString("Another Group"),
			       QString("With a subtitle"), this);

	tmp = new OBSActionRow("Placeholder");
	tmp->setSuffix(new OBSToggleSwitch);
	test->properties()->addRow(tmp);

	OBSCollapsibleActionRow *tmp2 = new OBSCollapsibleActionRow(
		QString("A Collapsible row!"), nullptr, true, this);
	test->addRow(tmp2);

	tmp = new OBSActionRow(QString("Spin box demo"));
	tmp->setSuffix(new OBSDoubleSpinBox());
	tmp2->addRow(tmp);

	tmp = new OBSActionRow(QString("Just another placeholder"));
	tmp->setSuffix(new OBSToggleSwitch(true));
	tmp2->addRow(tmp);

	tmp = new OBSActionRow("Placeholder 2");
	tmp->setSuffix(new OBSToggleSwitch);
	test->properties()->addRow(tmp);

	ui->scrollAreaWidgetContents->setContentsMargins(0, 0, 0, 0);
	ui->scrollAreaWidgetContents->layout()->setContentsMargins(0, 0, 0, 0);
	ui->scrollAreaWidgetContents->layout()->addWidget(test);
	ui->scrollAreaWidgetContents->layout()->setAlignment(Qt::AlignTop |
							     Qt::AlignHCenter);
}
