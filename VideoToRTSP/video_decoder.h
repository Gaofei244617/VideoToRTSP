#pragma once

#include <string>
#include <queue>
#include <mutex>
#include <memory>
#include <QImage>
#include "video_info.h"

/*** 解码输出队列 ***/
class ImageQueue
{
public:
	ImageQueue() {};
	~ImageQueue() {};

	void push(const QImage& image)
	{
		std::unique_lock<std::mutex> lck(m_mtx);
		m_images.push(image);
	}

	QImage pop()
	{
		std::unique_lock<std::mutex> lck(m_mtx);
		QImage img = m_images.front();
		m_images.pop();
		return img;
	}

	size_t size()
	{
		std::unique_lock<std::mutex> lck(m_mtx);
		return m_images.size();
	}

protected:
	std::queue<QImage> m_images;
	std::mutex m_mtx;
};

using ImageQueuePtr = std::shared_ptr<ImageQueue>;

/*** 视频解码器 ***/
class VideoDecoder
{
public:
	VideoDecoder(const std::string& video, ImageQueuePtr images);
	~VideoDecoder() {}

	void decode();
	void stop();

protected:
	VideoInfo m_videoInfo;
	ImageQueuePtr m_images;
	bool m_stop = false;
};
