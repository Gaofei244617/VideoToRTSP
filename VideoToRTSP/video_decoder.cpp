extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#include <thread>
#include <windows.h>
#include "video_decoder.h"

VideoDecoder::VideoDecoder(const std::string& video, ImageQueuePtr images)
{
	m_videoInfo = GetVideoInfo(video);
	m_images = images;
}

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

void VideoDecoder::decode()
{
	AVFormatContext* pInFmtCtx = NULL;
	AVStream* pVideoStream = NULL;
	AVCodecParameters* codecpar = NULL;
	AVCodecContext* codecContext = NULL;
	const AVCodec* codec = NULL;
	AVPacket* packet = NULL;

	// 打开文件
	int ret = avformat_open_input(&pInFmtCtx, toUtf8(m_videoInfo.url).c_str(), NULL, NULL); // ffmpeg要以UTF-8格式作为输入
	if (ret < 0)
	{
		goto end;
	}

	// 获取流信息
	ret = avformat_find_stream_info(pInFmtCtx, 0);
	if (ret != 0)
	{
		goto end;
	}

	codecpar = pInFmtCtx->streams[m_videoInfo.video_index]->codecpar;

	codec = avcodec_find_decoder(codecpar->codec_id);
	if (!codec)
	{
		goto end;
	}

	codecContext = avcodec_alloc_context3(codec);
	if (!codecContext)
	{
		goto end;
	}

	if (avcodec_parameters_to_context(codecContext, codecpar) < 0)
	{
		goto end;
	}

	if (avcodec_open2(codecContext, codec, nullptr) < 0)
	{
		goto end;
	}

	packet = av_packet_alloc();
	while (!m_stop)
	{
		if (m_images->size() >= 16)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}
		else
		{
			if (av_read_frame(pInFmtCtx, packet) >= 0)
			{
				if (packet->stream_index == m_videoInfo.video_index)
				{
					// 视频解码
					int ret = avcodec_send_packet(codecContext, packet);
					if (ret < 0)
					{
						continue;
					}

					while (true)
					{
						AVFrame* frame = av_frame_alloc();
						ret = avcodec_receive_frame(codecContext, frame);
						if (ret == 0)
						{
							int width = codecpar->width;
							int height = codecpar->height;

							int size = av_image_get_buffer_size(AV_PIX_FMT_RGB24, width, height, 1);
							uint8_t* outBuf = static_cast<uint8_t*>(av_malloc(size));
							AVFrame* frameOut = av_frame_alloc();
							av_image_fill_arrays(frameOut->data, frameOut->linesize, outBuf, AV_PIX_FMT_RGB24, width, height, 1);

							SwsContext* swsCtx = sws_getContext(width, height, codecContext->pix_fmt, width, height, AV_PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL);
							auto srcSlice = static_cast<const uint8_t* const*>(frame->data);
							sws_scale(swsCtx, srcSlice, frame->linesize, 0, height, frameOut->data, frameOut->linesize);

							// 解码图片
							QImage image = QImage(static_cast<uchar*>(frameOut->data[0]), width, height, QImage::Format_RGB888).copy();
							m_images->push(image);

							av_frame_unref(frameOut);
							av_frame_free(&frameOut);

							av_free(outBuf);
							sws_freeContext(swsCtx);

							av_frame_free(&frame);
						}
						else
						{
							av_frame_free(&frame);
							break;
						}
					}
				}
				av_packet_unref(packet);
			}
			else
			{
				break;
			}
		}
	}
	av_packet_free(&packet);

end:
	if (codecContext)
	{
		avcodec_free_context(&codecContext);
	}

	if (pInFmtCtx)
	{
		avio_closep(&pInFmtCtx->pb);
		avformat_close_input(&pInFmtCtx);
	}
}

void VideoDecoder::stop()
{
	m_stop = true;
}
