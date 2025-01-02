#include <iostream>
#include <filesystem>
#include <windows.h>
#include "send_rtsp.h"

static std::string toUtf8(const std::string& str)
{
	int nwLen = MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, NULL, 0);

	wchar_t* pwBuf = new wchar_t[nwLen + 1]; // 一定要加1，不然会出现尾巴
	ZeroMemory(pwBuf, nwLen * 2 + 2);

	MultiByteToWideChar(CP_ACP, 0, str.c_str(), str.length(), pwBuf, nwLen);

	int nLen = WideCharToMultiByte(CP_UTF8, 0, pwBuf, -1, NULL, NULL, NULL, NULL);

	char* pBuf = new char[nLen + 1];
	ZeroMemory(pBuf, nLen + 1);

	WideCharToMultiByte(CP_UTF8, 0, pwBuf, nwLen, pBuf, nLen, NULL, NULL);

	std::string retStr(pBuf);

	delete[] pwBuf;
	delete[] pBuf;

	pwBuf = NULL;
	pBuf = NULL;

	return retStr;
}

RtspSender::RtspSender() :m_stop(false)
{
}

RtspSender::~RtspSender()
{
	stop();
}

void RtspSender::async_send_rtsp(const RTSPConfig& config)
{
	t = std::thread(&RtspSender::send_rtsp, this, config);
}

int RtspSender::send_rtsp(const RTSPConfig& config)
{
	m_stop = false;

	if (!std::filesystem::exists(config.video))
	{
		return 10;
	}

	VideoInfo videoInfo = GetVideoInfo(config.video); // 视频信息
	if (videoInfo.video_index == -1)
	{
		// 没有视频流
		return 20;
	}

	AVFormatContext* pInFmtCtx = NULL;      // 输入流
	AVFormatContext* pOutFmtCtx = NULL;     // 输出流
	AVStream* pOutStream = NULL;            // 输出视频流
	AVCodec* pCodec = NULL;                 // 解码器

	int frameNum = 0;       // 帧计数
	int ret = 0;            // 错误码

	auto span = std::chrono::microseconds(int(1000 / videoInfo.fps * 1000)); // 帧间隔
	decltype(std::chrono::high_resolution_clock::now()) startTime; // 开始推流时间

	// 打开文件
	ret = avformat_open_input(&pInFmtCtx, toUtf8(config.video).c_str(), NULL, NULL);
	if (ret < 0)
	{
		ret = 30;
		goto end;
	}

	// 获取流信息
	ret = avformat_find_stream_info(pInFmtCtx, NULL);
	if (ret != 0)
	{
		ret = 40;
		goto end;
	}

	// 创建输出上下文
	ret = avformat_alloc_output_context2(&pOutFmtCtx, NULL, "rtsp", config.url.c_str());
	if (ret < 0)
	{
		ret = 50;
		goto end;
	}

	// 创建一个新的流
	pCodec = (AVCodec*)avcodec_find_encoder(pOutFmtCtx->oformat->video_codec);
	if (pCodec == NULL)
	{
		ret = 60;
		goto end;
	}

	pOutStream = avformat_new_stream(pOutFmtCtx, pCodec);
	if (pOutStream == NULL)
	{
		ret = 70;
		goto end;
	}

	// 复制配置信息
	ret = avcodec_parameters_copy(pOutStream->codecpar, pInFmtCtx->streams[videoInfo.video_index]->codecpar);
	if (ret < 0)
	{
		ret = 80;
		goto end;
	}
	pOutStream->codecpar->codec_tag = 0;

	// 写入头部信息
	ret = avformat_write_header(pOutFmtCtx, NULL);
	if (ret < 0)
	{
		ret = 90;
		goto end;
	}

	startTime = std::chrono::high_resolution_clock::now();
	for (int i = 0; i < config.loop; i++)
	{
		// 时间基数
		AVRational timeBase = av_make_q(1000, int(videoInfo.fps * 1000 + 0.5));
		AVRational otime = pOutFmtCtx->streams[videoInfo.video_index]->time_base;

		AVPacket avPacket;
		while (true)
		{
			if (m_stop)
			{
				goto end;
			}

			if (av_read_frame(pInFmtCtx, &avPacket) == AVERROR_EOF)
			{
				av_packet_unref(&avPacket);
				break;
			}

			if (avPacket.stream_index == videoInfo.video_index)
			{
				// 计算转换时间戳
				avPacket.pts = av_rescale_q_rnd(frameNum, timeBase, otime, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_NEAR_INF));
				avPacket.dts = avPacket.pts;
				avPacket.duration = 0;
				avPacket.pos = -1;

				// 控制推帧速度
				std::this_thread::sleep_until(startTime + span * frameNum);

				// 推帧
				ret = av_interleaved_write_frame(pOutFmtCtx, &avPacket);
				if (ret < 0)
				{
					break;
				}

				frameNum++;

				// 推流进度
				if (config.callback)
				{
					config.callback(frameNum / (videoInfo.fps * videoInfo.duration));
				}
			}
			av_packet_unref(&avPacket);
		}

		// 重新打开文件
		avio_closep(&pInFmtCtx->pb);
		avformat_close_input(&pInFmtCtx);
		ret = avformat_open_input(&pInFmtCtx, toUtf8(config.video).c_str(), NULL, NULL);
		if (ret < 0)
		{
			ret = 20;
			goto end;
		}
	}

end:
	if (pInFmtCtx)
	{
		avio_closep(&pInFmtCtx->pb);
		avformat_close_input(&pInFmtCtx);
	}

	if (pOutFmtCtx && !(pOutFmtCtx->flags & AVFMT_NOFILE))
	{
		avio_close(pOutFmtCtx->pb);
	}

	avformat_free_context(pOutFmtCtx);

	return ret;
}

void RtspSender::stop()
{
	m_stop = true;
	if (t.joinable())
	{
		t.join();
	}
}