#include <filesystem>
#include <windows.h>
#include "video_info.h"

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
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

// 解码一帧视频
static QImage decode_one_frame(AVFormatContext* pInFmtCtx, int index)
{
	QImage image;

	AVCodecParameters* codecpar = pInFmtCtx->streams[index]->codecpar;
	AVCodecContext* codecContext = NULL;
	const AVCodec* codec = NULL;

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

	AVPacket packet;
	while (av_read_frame(pInFmtCtx, &packet) >= 0)
	{
		if (packet.stream_index == index && (packet.flags & AV_PKT_FLAG_KEY))
		{
			AVFrame* frame = av_frame_alloc();
			int ret = avcodec_send_packet(codecContext, &packet);
			if (ret < 0)
			{
				continue;
			}

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

				image = QImage(static_cast<uchar*>(frameOut->data[0]), width, height, QImage::Format_RGB888).copy();

				av_frame_unref(frameOut);
				av_frame_free(&frameOut);

				av_free(outBuf);
				sws_freeContext(swsCtx);

				av_packet_unref(&packet);
				av_frame_free(&frame);

				break;
			}
		}
	}

end:
	if (codecContext)
	{
		avcodec_free_context(&codecContext);
	}

	return image;
}

VideoInfo GetVideoInfo(const std::string& video)
{
	VideoInfo info;

	AVFormatContext* pInFmtCtx = NULL;
	AVStream* pVideoStream = NULL;
	AVCodecParameters* codecpar = NULL;

	if (!std::filesystem::exists(video))
	{
		return info;
	}

	info.url = video;
	info.size = std::filesystem::file_size(video);

	// 打开文件
	int ret = avformat_open_input(&pInFmtCtx, toUtf8(video).c_str(), NULL, NULL); // ffmpeg要以UTF-8格式作为输入
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

	info.stream_num = pInFmtCtx->nb_streams;
	for (unsigned int i = 0; i < pInFmtCtx->nb_streams; i++)
	{
		if (pInFmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			info.video_index = i;
			codecpar = pInFmtCtx->streams[i]->codecpar;
			pVideoStream = pInFmtCtx->streams[i];

			break;
		}
	}

	if (info.video_index == -1)
	{
		goto end;
	}

	info.fps = av_q2d(pVideoStream->avg_frame_rate);
	info.width = codecpar->width;
	info.height = codecpar->height;

	// 视频时长
	if (pVideoStream->duration > 0)
	{
		info.duration = av_q2d(pVideoStream->time_base) * pVideoStream->duration;
	}
	else
	{
		// 无法直接通过封装信息获取时长, 通过读取帧数来计算
		int cnt = 0;
		AVPacket packet;
		while (true)
		{
			if (av_read_frame(pInFmtCtx, &packet) < 0)
			{
				av_packet_unref(&packet);
				break;
			}

			cnt++;
			av_packet_unref(&packet);
		}
		info.duration = cnt / info.fps;
	}

	// 编码格式
	info.encode = EncodeType::Other;
	if (codecpar->codec_id == AV_CODEC_ID_H264)
	{
		info.encode = EncodeType::H264;
	}
	else if (codecpar->codec_id == AV_CODEC_ID_HEVC)
	{
		info.encode = EncodeType::HEVC;
	}

	// 解码一帧视频
	info.image = decode_one_frame(pInFmtCtx, info.video_index);

end:
	if (pInFmtCtx)
	{
		avio_closep(&pInFmtCtx->pb);
		avformat_close_input(&pInFmtCtx);
	}

	return info;
}