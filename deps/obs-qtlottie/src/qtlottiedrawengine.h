/*
 * MIT License
 *
 * Copyright (C) 2022 by wangwenx190 (Yuhang Zhao)
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

#pragma once

#include <QtCore>

QT_BEGIN_NAMESPACE
class QPainter;
QT_END_NAMESPACE

namespace rlottie {
class Animation;
};

class QtLottieDrawEngine : public QObject {
	Q_OBJECT
	Q_DISABLE_COPY_MOVE(QtLottieDrawEngine)
	Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged
			   FINAL)
	Q_PROPERTY(qint64 frameRate READ frameRate NOTIFY frameRateChanged FINAL)
	Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged FINAL)
	Q_PROPERTY(QSize size READ size NOTIFY sizeChanged FINAL)
	Q_PROPERTY(qint64 loops READ loops WRITE setLoops NOTIFY loopsChanged
			   FINAL)
	Q_PROPERTY(bool playing READ playing NOTIFY playingChanged FINAL)
	Q_PROPERTY(qreal devicePixelRatio READ devicePixelRatio WRITE
			   setDevicePixelRatio NOTIFY devicePixelRatioChanged
				   FINAL)

public:
	explicit QtLottieDrawEngine(QObject *parent = nullptr);
	~QtLottieDrawEngine() override = default;

	void paint(QPainter *painter, const QSize &s);
	void render(const QSize &s);

	QUrl source() const;
	bool setSource(const QUrl &value, const QUrl &resources = QUrl());

	qint64 frameRate() const;

	qint64 duration() const;

	QSize size() const;

	qint64 loops() const;
	void setLoops(const qint64 value);

	bool playing() const;

	qreal devicePixelRatio() const;
	void setDevicePixelRatio(const qreal value);

public Q_SLOTS:
	void pause();
	void resume();

Q_SIGNALS:
	void sourceChanged(const QUrl &);
	void frameRateChanged(qint64);
	void durationChanged(qint64);
	void sizeChanged(const QSize &);
	void loopsChanged(qint64);
	void playingChanged(bool);
	void devicePixelRatioChanged(qreal);

	void needsRepaint();

private:
	QUrl m_source = {};
	std::unique_ptr<rlottie::Animation> m_player = nullptr;
	QScopedArrayPointer<char> m_frameBuffer;
	size_t m_currentFrame = 0;
	size_t m_totalFrame = 0;
	size_t m_width = 0;
	size_t m_height = 0;
	bool m_hasFirstUpdate = false;
	qint64 m_frameRate = 0;
	qint64 m_duration = 0;
	qint64 m_loops = 0;
	qint64 m_loopTimes = 0;
	bool m_shouldStop = false;
	qreal m_devicePixelRatio = 0.0;
};
