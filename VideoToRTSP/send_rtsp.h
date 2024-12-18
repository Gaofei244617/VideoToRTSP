#pragma once

#include <atomic>
#include <string>
#include <chrono>
#include <thread>
#include <functional>
#include "video_info.h"

extern "C"
{
#include "libavformat/avformat.h"
};

struct RTSPConfig
{
	std::string url;      // 流地址
	std::string video;    // 本地视频
	int loop = 1;         // 循环次数
	std::function<void(double)> callback = nullptr; // 进度监控
};

class RtspSender
{
public:
	RtspSender();
	~RtspSender();

	void async_send_rtsp(const RTSPConfig& config);
	void stop();

protected:
	int send_rtsp(const RTSPConfig& config);

protected:
	std::atomic_bool m_stop;
	std::thread t;
};
