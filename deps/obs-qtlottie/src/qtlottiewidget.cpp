/*
 * MIT License
 *
 * Copyright (C) 2021 by wangwenx190 (Yuhang Zhao)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "qtlottiewidget.h"
#include "qtlottiedrawengine.h"

#include <QPainter>

QtLottieWidget::QtLottieWidget(QWidget *parent) : QWidget(parent)
{
	auto drawEngine = new QtLottieDrawEngine(this);
	m_drawEngine.reset(drawEngine);

	// Use "Qt::PreciseTimer" to get the best appearance, however, it
	// will have a performance impact.
	m_timer.setTimerType(Qt::PreciseTimer);
	connect(&m_timer, &QTimer::timeout, this, [this]() {
		if (m_drawEngine->playing()) {
			m_drawEngine->render(size());
		}
	});

	connect(drawEngine, &QtLottieDrawEngine::needsRepaint, this,
		qOverload<>(&QtLottieWidget::update));
	connect(drawEngine, &QtLottieDrawEngine::frameRateChanged, this,
		[this](qint64 arg) {
			if (m_timer.isActive()) {
				m_timer.stop();
			}
			m_timer.setInterval(qRound(qreal(1000) / qreal(arg)));
			if (m_drawEngine->playing()) {
				m_timer.start();
			}
			Q_EMIT frameRateChanged(arg);
		});
	connect(drawEngine, &QtLottieDrawEngine::playingChanged, this, [this](bool arg) {
		if (arg) {
			if (!m_timer.isActive()) {
				if (m_timer.interval() <= 0) {
					m_timer.setInterval(qRound(
						qreal(1000) /
						qreal(m_drawEngine
							      ->frameRate())));
				}
				m_timer.start();
			}
		} else {
			if (m_timer.isActive()) {
				m_timer.stop();
			}
		}
	});
	connect(drawEngine, &QtLottieDrawEngine::durationChanged, this,
		&QtLottieWidget::durationChanged);
	connect(drawEngine, &QtLottieDrawEngine::sizeChanged, this,
		&QtLottieWidget::sourceSizeChanged);
	connect(drawEngine, &QtLottieDrawEngine::loopsChanged, this,
		&QtLottieWidget::loopsChanged);
	connect(drawEngine, &QtLottieDrawEngine::devicePixelRatioChanged, this,
		&QtLottieWidget::devicePixelRatioChanged);
}

QtLottieWidget::~QtLottieWidget()
{
	dispose();
}

void QtLottieWidget::dispose()
{
	if (m_timer.isActive()) {
		m_timer.stop();
	}
}

void QtLottieWidget::pause()
{
	m_drawEngine->pause();
}

void QtLottieWidget::resume()
{
	m_drawEngine->resume();
}

QSize QtLottieWidget::minimumSizeHint() const
{
	// Our lottie backend will fail to paint if the size of the widget is too small.
	// This size will be ignored if you set the size policy or minimum size explicitly.
	return {50, 50};
}

QUrl QtLottieWidget::source() const
{
	return m_source;
}

void QtLottieWidget::setSource(const QUrl &value, const QUrl &resources)
{
	if (!value.isValid()) {
		qWarning() << value << "is not a valid URL.";
		return;
	}
	if (m_source != value) {
		m_source = value;
		if (!m_drawEngine->setSource(value, resources)) {
			qWarning() << "Failed to start playing.";
		}
		Q_EMIT sourceChanged(m_source);
	}
}

qint64 QtLottieWidget::frameRate() const
{
	return m_drawEngine->frameRate();
}

qint64 QtLottieWidget::duration() const
{
	return m_drawEngine->duration();
}

QSize QtLottieWidget::sourceSize() const
{
	return m_drawEngine->size();
}

qint64 QtLottieWidget::loops() const
{
	return m_drawEngine->loops();
}

void QtLottieWidget::setLoops(const qint64 value)
{
	m_drawEngine->setLoops(value);
}

qreal QtLottieWidget::devicePixelRatio() const
{
	return m_drawEngine->devicePixelRatio();
}

void QtLottieWidget::setDevicePixelRatio(const qreal value)
{
	m_drawEngine->setDevicePixelRatio(value);
}

void QtLottieWidget::paintEvent(QPaintEvent *event)
{
	QWidget::paintEvent(event);
	QPainter painter(this);
	m_drawEngine->paint(&painter, size());
}
