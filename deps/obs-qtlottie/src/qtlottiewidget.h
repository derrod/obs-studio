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

#pragma once

#include <QWidget>
#include <QUrl>
#include <QTimer>

class QtLottieDrawEngine;

class QtLottieWidget : public QWidget {
	Q_OBJECT
	Q_DISABLE_COPY_MOVE(QtLottieWidget)
	Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged
			   FINAL)
	Q_PROPERTY(qint64 frameRate READ frameRate NOTIFY frameRateChanged FINAL)
	Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged FINAL)
	Q_PROPERTY(
		QSize sourceSize READ sourceSize NOTIFY sourceSizeChanged FINAL)
	Q_PROPERTY(qint64 loops READ loops WRITE setLoops NOTIFY loopsChanged
			   FINAL)
	Q_PROPERTY(qreal devicePixelRatio READ devicePixelRatio WRITE
			   setDevicePixelRatio NOTIFY devicePixelRatioChanged
				   FINAL)

public:
	explicit QtLottieWidget(QWidget *parent = nullptr);
	~QtLottieWidget() override;

	QSize minimumSizeHint() const override;

	QUrl source() const;
	void setSource(const QUrl &value);

	qint64 frameRate() const;

	qint64 duration() const;

	QSize sourceSize() const;

	qint64 loops() const;
	void setLoops(const qint64 value);

	qreal devicePixelRatio() const;
	void setDevicePixelRatio(const qreal value);

public Q_SLOTS:
	void dispose();
	void pause();
	void resume();

Q_SIGNALS:
	void sourceChanged(const QUrl &);
	void frameRateChanged(qint64);
	void durationChanged(qint64);
	void sourceSizeChanged(const QSize &);
	void loopsChanged(qint64);
	void devicePixelRatioChanged(qreal);

protected:
	void paintEvent(QPaintEvent *event) override;

private:
	QUrl m_source = {};
	std::unique_ptr<QtLottieDrawEngine> m_drawEngine;
	QTimer m_timer;
};
