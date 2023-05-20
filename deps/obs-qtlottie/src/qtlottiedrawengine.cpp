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

#include <QPainter>
#include <QFile>
#include <QVariant>
#include <QScreen>
#include <QGuiApplication>

#include "rlottie.h"
#include "qtlottiedrawengine.h"

QtLottieDrawEngine::QtLottieDrawEngine(QObject *parent) : QObject(parent)
{
	if (const auto screen = QGuiApplication::primaryScreen()) {
		m_devicePixelRatio = screen->devicePixelRatio();
	} else {
		m_devicePixelRatio = 1.0;
	}
}

bool QtLottieDrawEngine::setSource(const QUrl &value)
{
	Q_ASSERT(value.isValid());

	if (!value.isValid()) {
		qWarning() << value << "is not a valid URL.";
		return false;
	}

	if (m_source == value) {
		return false;
	}

	QString jsonFilePath = {};
	if (value.scheme() == QStringLiteral("qrc")) {
		jsonFilePath = value.toString();
		// QFile can't recognize url.
		jsonFilePath.replace(QStringLiteral("qrc:"),
				     QStringLiteral(":"), Qt::CaseInsensitive);
		jsonFilePath.replace(QStringLiteral(":///"),
				     QStringLiteral(":/"));
	} else {
		jsonFilePath = value.isLocalFile() ? value.toLocalFile()
						   : value.url();
	}

	Q_ASSERT(QFile::exists(jsonFilePath));
	if (!QFile::exists(jsonFilePath)) {
		qWarning() << jsonFilePath << "doesn't exist.";
		return false;
	}

	QFile file(jsonFilePath);
	if (!file.open(QFile::ReadOnly | QFile::Text)) {
		qWarning() << "Failed to open the JSON file:" << jsonFilePath;
		return false;
	}

	const QByteArray jsonBuffer = file.readAll();
	file.close();

	if (jsonBuffer.isEmpty()) {
		qWarning() << "File is empty:" << jsonFilePath;
		return false;
	}

	// TODO: Support setting this via parameter, since OBS has its own resource handling
	const QString resDirPath = QCoreApplication::applicationDirPath();

	m_player = rlottie::Animation::loadFromData(jsonBuffer.toStdString(),
						    resDirPath.toStdString());

	if (!m_player) {
		qWarning() << "Failed to create lottie animation.";
		return false;
	}

	m_source = value;

	m_player->size(m_width, m_height);
	m_frameRate = qRound64(m_player->frameRate());

	m_duration = qRound64(m_player->duration());
	m_totalFrame = m_player->totalFrame();

	m_frameBuffer.reset(new char[m_width * m_height * 32 / 8]);
	// Clear previous status.
	m_currentFrame = 0;
	m_loopTimes = 0;
	m_shouldStop = false;

	Q_EMIT sourceChanged(m_source);
	Q_EMIT sizeChanged(size());
	Q_EMIT frameRateChanged(m_frameRate);
	Q_EMIT durationChanged(m_duration);
	Q_EMIT playingChanged(true);
	return true;
}

qint64 QtLottieDrawEngine::frameRate() const
{
	return m_frameRate;
}

qint64 QtLottieDrawEngine::duration() const
{
	return m_duration;
}

QSize QtLottieDrawEngine::size() const
{
	return {int(m_width), int(m_height)};
}

qint64 QtLottieDrawEngine::loops() const
{
	return m_loops;
}

void QtLottieDrawEngine::setLoops(const qint64 value)
{
	if (m_loops != value) {
		m_loops = value;
		Q_EMIT loopsChanged(m_loops);
		// Also clear previous status.
		m_loopTimes = 0;
		m_shouldStop = false;
		Q_EMIT playingChanged(true);
	}
}

bool QtLottieDrawEngine::playing() const
{
	return (m_player && !m_shouldStop);
}

qreal QtLottieDrawEngine::devicePixelRatio() const
{
	return m_devicePixelRatio;
}

void QtLottieDrawEngine::setDevicePixelRatio(const qreal value)
{
	if (qFuzzyCompare(m_devicePixelRatio, value)) {
		return;
	}
	m_devicePixelRatio = value;
	Q_EMIT devicePixelRatioChanged(m_devicePixelRatio);
}

void QtLottieDrawEngine::pause()
{
	if (m_player && !m_shouldStop) {
		m_shouldStop = true;
		Q_EMIT playingChanged(false);
	}
}

void QtLottieDrawEngine::resume()
{
	if (m_player && m_shouldStop) {
		m_shouldStop = false;
		Q_EMIT playingChanged(true);
	}
}

void QtLottieDrawEngine::paint(QPainter *painter, const QSize &s)
{
	Q_ASSERT(painter);
	if (!painter) {
		return;
	}
	Q_ASSERT(s.isValid());
	if (!s.isValid()) {
		qWarning() << s << "is not a valid size.";
		return;
	}
	if (!m_player) {
		// lottie animation is not created, mostly due to setSource() not called.
		return;
	}
	if (m_shouldStop) {
		return;
	}
	if (!m_hasFirstUpdate) {
		return;
	}
	QImage image((int)m_width, (int)m_height, QImage::Format_ARGB32);
	for (int i = 0; i != image.height(); ++i) {
		char *p = m_frameBuffer.data() + i * image.bytesPerLine();
		std::memcpy(image.scanLine(i), p, image.bytesPerLine());
	}
	// TODO: let the user be able to set the scale mode.
	// "Qt::SmoothTransformation" is a must otherwise the scaled image will become fuzzy.
	painter->drawImage(QPoint{0, 0},
			   (s == size())
				   ? image
				   : image.scaled(s, Qt::IgnoreAspectRatio,
						  Qt::SmoothTransformation));
}

void QtLottieDrawEngine::render(const QSize &s)
{
	Q_UNUSED(s);
	if (!m_player) {
		// lottie animation is not created, mostly due to setSource() not called.
		// Or the rlottie library is not loaded. Safe to ignore.
		return;
	}
	if (m_shouldStop) {
		return;
	}

	rlottie::Surface surface(
		reinterpret_cast<uint32_t *>(m_frameBuffer.data()), m_width,
		m_height, m_width * 32 / 8);
	m_player->renderSync(m_currentFrame, surface);

	if (m_currentFrame >= m_totalFrame) {
		m_currentFrame = 0;
		// negative number means infinite loops.
		if (m_loops > 0) {
			++m_loopTimes;
			if (m_loopTimes >= m_loops) {
				m_loopTimes = 0;
				m_shouldStop = true;
				Q_EMIT playingChanged(false);
				return;
			}
		}
	} else {
		++m_currentFrame;
	}
	m_hasFirstUpdate = true;
	Q_EMIT needsRepaint();
}

QUrl QtLottieDrawEngine::source() const
{
	return m_source;
}
