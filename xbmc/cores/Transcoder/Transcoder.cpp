// ConsoleApplication1.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

extern "C" {
#include <libavcodec\avcodec.h>
#include <libavformat\avformat.h>
#include <libswscale\swscale.h>
#include <libavfilter/avfiltergraph.h>
#include <libavfilter/avcodec.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>

}

void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame) {
	FILE *pFile;
	char szFilename[32];
	int  y;

	// Open file
	sprintf(szFilename, "frame%d.ppm", iFrame);
	pFile = fopen(szFilename, "wb");
	if (pFile == NULL)
		return;

	// Write header
	fprintf(pFile, "P6\n%d %d\n255\n", width, height);

	// Write pixel data
	for (y = 0; y<height; y++)
		fwrite(pFrame->data[0] + y*pFrame->linesize[0], 1, width * 3, pFile);

	// Close file
	fclose(pFile);
}

int tutorial1()
{

	_tprintf(_T("Trying av_register_all... "));

	const char* filename = "StarWarsTrailer.mp4";
	AVFormatContext *pFormatCtx = NULL;
	av_register_all();

	// Open video file
	if (avformat_open_input(&pFormatCtx, filename, NULL, 0) != 0)
		return -1; // Couldn't open file

	// Retrieve stream information
	if (avformat_find_stream_info(pFormatCtx, NULL)<0)
		return -2; // Couldn't find stream information

	// Dump information about file onto standard error
	av_dump_format(pFormatCtx, 0, filename, 0);


	// Get a pointer to the codec context for the video stream
	int i;
	AVCodecContext *pCodecCtxOrig = NULL;
	AVCodecContext *pCodecCtx = NULL;

	// Find the first video stream
	int videoStream = -1;
	for (i = 0; i<pFormatCtx->nb_streams; i++)
		if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			videoStream = i;
			break;
		}
	if (videoStream == -1)
		return -3; // Didn't find a video stream

	pCodecCtx = pFormatCtx->streams[videoStream]->codec;


	// Find the decoder for the video stream
	AVCodec *pCodec = NULL;
	pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
	if (pCodec == NULL) {
		fprintf(stderr, "Unsupported codec!\n");
		return -4; // Codec not found
	}
	// Copy context
	pCodecCtxOrig = avcodec_alloc_context3(pCodec);
	if (avcodec_copy_context(pCodecCtxOrig, pCodecCtx) != 0) {
		fprintf(stderr, "Couldn't copy codec context");
		return -5; // Error copying codec context
	}
	// Open codec
	if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0)
		return -6; // Could not open codec

	// Allocate video frame
	AVFrame *pFrame = NULL;
	pFrame = av_frame_alloc();

	// Allocate an AVFrame structure
	AVFrame *pFrameRGB = av_frame_alloc();
	if (pFrameRGB == NULL)
		return -7;

	uint8_t *buffer = NULL;
	int numBytes;
	// Determine required buffer size and allocate buffer
	numBytes = avpicture_get_size(PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height);
	buffer = (uint8_t *)av_malloc(numBytes*sizeof(uint8_t));

	// Assign appropriate parts of buffer to image planes in pFrameRGB
	// Note that pFrameRGB is an AVFrame, but AVFrame is a superset
	// of AVPicture
	avpicture_fill((AVPicture *)pFrameRGB, buffer, PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height);

	struct SwsContext *sws_ctx = NULL;
	int frameFinished;
	AVPacket packet;
	// initialize SWS context for software scaling
	sws_ctx = sws_getContext(pCodecCtx->width,
		pCodecCtx->height,
		pCodecCtx->pix_fmt,
		pCodecCtx->width,
		pCodecCtx->height,
		PIX_FMT_RGB24,
		SWS_BILINEAR,
		NULL,
		NULL,
		NULL
		);

	i = 0;
	while (av_read_frame(pFormatCtx, &packet) >= 0) {
		// Is this a packet from the video stream?
		if (packet.stream_index == videoStream) {
			// Decode video frame
			avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);

			// Did we get a video frame?
			if (frameFinished) {
				// Convert the image from its native format to RGB
				sws_scale(sws_ctx, (uint8_t const * const *)pFrame->data,
					pFrame->linesize, 0, pCodecCtx->height,
					pFrameRGB->data, pFrameRGB->linesize);

				// Save the frame to disk
				if (++i <= 5) SaveFrame(pFrameRGB, pCodecCtx->width, pCodecCtx->height, i);
				else break;
			}
		}

		// Free the packet that was allocated by av_read_frame
		av_free_packet(&packet);
	}

	// Free the RGB image
	av_free(buffer);
	av_free(pFrameRGB);

	// Free the YUV frame
	av_free(pFrame);

	// Close the codecs
	avcodec_close(pCodecCtx);
	avcodec_close(pCodecCtxOrig);

	// Close the video file
	avformat_close_input(&pFormatCtx);

	_tprintf(_T("Done.\n"));
	_sleep(5000);
}

/*
* Video encoding example
*/



int video_encode_example(const char *filename)
{
	av_register_all();
	avcodec_register_all();

	AVCodec *codec;
	AVCodecContext *c = NULL;
	int i, out_size, size, x, y, outbuf_size;
	FILE *f;
	AVFrame *picture;
	uint8_t *outbuf, *picture_buf;

	AVFormatContext *octx;
	AVStream *ostream;

	avformat_alloc_output_context2(&octx, NULL, NULL, filename);
	if (!octx) {
		av_log(NULL, AV_LOG_ERROR, "Could not create output context\n");
		return AVERROR_UNKNOWN;
	}

	ostream = avformat_new_stream(octx, NULL);
	if (!ostream) {
		av_log(NULL, AV_LOG_ERROR, "Failed allocating output stream\n");
		return AVERROR_UNKNOWN;
	}

	c = ostream->codec;
	codec = avcodec_find_encoder(AV_CODEC_ID_H264);
	if (!codec) {
		av_log(NULL, AV_LOG_FATAL, "Neccessary encoder not found\n");
		return AVERROR_INVALIDDATA;
	}

	///* put sample parameters */
	//c->bit_rate = 400000;
	///* resolution must be a multiple of two */
	c->width = 352;
	c->height = 288;
	/* frames per second */
	AVRational r = { 1, 25 };
	c->time_base = r;
	c->gop_size = 10; /* emit one intra frame every ten frames */
	c->max_b_frames = 1;
	c->pix_fmt = PIX_FMT_YUV420P;
	c->bit_rate = 500 * 1000;
	c->bit_rate_tolerance = 0;
	c->rc_max_rate = 0;
	c->rc_buffer_size = 0;
	c->gop_size = 40;
	c->max_b_frames = 3;
	c->b_frame_strategy = 1;
	c->coder_type = 1;
	c->me_cmp = 1;
	c->me_range = 16;
	c->qmin = 10;
	c->qmax = 51;
	c->scenechange_threshold = 40;
	c->flags |= CODEC_FLAG_LOOP_FILTER;
	c->me_method = ME_HEX;
	c->me_subpel_quality = 5;
	c->i_quant_factor = 0.71;
	c->qcompress = 0.6;
	c->max_qdiff = 4;
	//c->directpred = 1;
	//c->flags2 |= CODEC_FLAG2_FASTPSKIP;

	/* Third parameter can be used to pass settings to encoder */
	int ret = avcodec_open2(c, codec, NULL);
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot open video encoder for stream\n");
		return ret;
	}

	av_dump_format(octx, 0, filename, 1);

	if (!(octx->oformat->flags & AVFMT_NOFILE)) {
		ret = avio_open(&octx->pb, filename, AVIO_FLAG_WRITE);
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR, "Could not open output file '%s'", filename);
			return ret;
		}
	}

	/* init muxer, write output file header */
	ret = avformat_write_header(octx, NULL);
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, "Error occurred when opening output file\n");
		return ret;
	}

	printf("Video encoding\n");

	picture = avcodec_alloc_frame();

	/* alloc image and output buffer */
	outbuf_size = 100000;
	outbuf = (uint8_t *)malloc(outbuf_size);
	size = c->width * c->height;
	picture_buf = (uint8_t *)malloc((size * 3) / 2); /* size for YUV 420 */

	picture->data[0] = picture_buf;
	picture->data[1] = picture->data[0] + size;
	picture->data[2] = picture->data[1] + size / 4;
	picture->linesize[0] = c->width;
	picture->linesize[1] = c->width / 2;
	picture->linesize[2] = c->width / 2;
	picture->width = c->width;
	picture->height = c->height;
	picture->pict_type = AV_PICTURE_TYPE_P;
	picture->format = c->pix_fmt;

	/* encode 1 second of video */
	for (i = 0; i < 25; i++) {
		fflush(stdout);
		/* prepare a dummy image */
		/* Y */
		for (y = 0; y < c->height; y++) {
			for (x = 0; x < c->width; x++) {
				picture->data[0][y * picture->linesize[0] + x] = x + y + i * 3;
			}
		}

		/* Cb and Cr */
		for (y = 0; y < c->height / 2; y++) {
			for (x = 0; x < c->width / 2; x++) {
				picture->data[1][y * picture->linesize[1] + x] = 128 + y + i * 2;
				picture->data[2][y * picture->linesize[2] + x] = 64 + x + i * 5;
			}
		}

		///* encode the image */
		//out_size = avcodec_encode_video(c, outbuf, outbuf_size, picture);
		//printf("encoding frame %3d (size=%5d)\n", i, out_size);
		//fwrite(outbuf, 1, out_size, f);

		AVPacket enc_pkt;
		int got_frame;

		av_log(NULL, AV_LOG_INFO, "Encoding frame\n");
		/* encode filtered frame */
		enc_pkt.data = NULL;
		enc_pkt.size = 0;
		av_init_packet(&enc_pkt);
		ret = avcodec_encode_video2(octx->streams[0]->codec, &enc_pkt,
			picture, &got_frame);

		/* prepare packet for muxing */
		enc_pkt.stream_index = 0;
		av_packet_rescale_ts(&enc_pkt,
			octx->streams[0]->codec->time_base,
			octx->streams[0]->time_base);

		av_log(NULL, AV_LOG_DEBUG, "Muxing frame\n");
		/* mux encoded frame */
		ret = av_interleaved_write_frame(octx, &enc_pkt);
	}

	av_write_trailer(octx);
	avcodec_close(octx->streams[0]->codec);
	if (octx && !(octx->oformat->flags & AVFMT_NOFILE))
		avio_closep(&octx->pb);
	avformat_free_context(octx);

	if (ret < 0)
		av_log(NULL, AV_LOG_ERROR, "Error occurred: %s\n", ret/*,av_err2str(ret)*/);


	free(picture_buf);
	free(outbuf);

	av_free(picture);
	printf("\n");

	return 0;
}

/*
* Copyright (c) 2010 Nicolas George
* Copyright (c) 2011 Stefano Sabatini
* Copyright (c) 2014 Andrey Utkin
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
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
*/

/**
* @file
* API example for demuxing, decoding, filtering, encoding and muxing
* @example transcoding.c
*/

static void av_log_error(int errnum)
{
	char* buf = (char*) malloc(AV_ERROR_MAX_STRING_SIZE * sizeof(char));
	av_log(NULL, AV_LOG_ERROR, "Error occurred: %s\n", av_make_error_string(buf, AV_ERROR_MAX_STRING_SIZE, errnum));
	free(buf);
}

static AVFormatContext *ifmt_ctx;
static AVFormatContext *ofmt_ctx;
typedef struct FilteringContext {
	AVFilterContext *buffersink_ctx;
	AVFilterContext *buffersrc_ctx;
	AVFilterGraph *filter_graph;
} FilteringContext;
static FilteringContext *filter_ctx;

static int open_input_file(const char *filename)
{
	int ret;
	unsigned int i;

	ifmt_ctx = NULL;
	if ((ret = avformat_open_input(&ifmt_ctx, filename, NULL, NULL)) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
		return ret;
	}

	if ((ret = avformat_find_stream_info(ifmt_ctx, NULL)) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
		return ret;
	}

	for (i = 0; i < ifmt_ctx->nb_streams; i++) {
		AVStream *stream;
		AVCodecContext *codec_ctx;
		stream = ifmt_ctx->streams[i];
		codec_ctx = stream->codec;
		/* Reencode video & audio and remux subtitles etc. */
		if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO
			|| codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
			/* Open decoder */
			ret = avcodec_open2(codec_ctx,
				avcodec_find_decoder(codec_ctx->codec_id), NULL);
			if (ret < 0) {
				av_log(NULL, AV_LOG_ERROR, "Failed to open decoder for stream #%u\n", i);
				return ret;
			}
		}
	}

	av_dump_format(ifmt_ctx, 0, filename, 0);
	return 0;
}

static int open_output_file(const char *filename)
{
	AVStream *out_stream;
	AVStream *in_stream;
	AVCodecContext *dec_ctx, *enc_ctx;
	AVCodec *encoder;

	AVDictionary *dict = NULL;
	int ret;
	unsigned int i;

	ofmt_ctx = NULL;
	avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, filename);
	if (!ofmt_ctx) {
		av_log(NULL, AV_LOG_ERROR, "Could not create output context\n");
		return AVERROR_UNKNOWN;
	}

	AVOutputFormat* fmt = av_guess_format(0, filename, 0);
	if (!fmt) {
		av_log(NULL, AV_LOG_ERROR, "Could not guess format\n");
		return AVERROR_UNKNOWN;
	}
	
	ofmt_ctx->oformat = fmt;

	for (i = 0; i < ifmt_ctx->nb_streams; i++) {
		out_stream = avformat_new_stream(ofmt_ctx, NULL);
		if (!out_stream) {
			av_log(NULL, AV_LOG_ERROR, "Failed allocating output stream\n");
			return AVERROR_UNKNOWN;
		}

		in_stream = ifmt_ctx->streams[i];
		dec_ctx = in_stream->codec;
		enc_ctx = out_stream->codec;

		if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO
			|| dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
			/* in this example, we choose transcoding to same codec */
			if (dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO)
				//encoder = avcodec_find_encoder(dec_ctx->codec_id);
				encoder = avcodec_find_encoder(AV_CODEC_ID_AAC);
			else 
				encoder = avcodec_find_encoder(AV_CODEC_ID_H264);
			if (!encoder) {
				av_log(NULL, AV_LOG_FATAL, "Neccessary encoder not found\n");
				return AVERROR_INVALIDDATA;
			}

			/* In this example, we transcode to same properties (picture size,
			* sample rate etc.). These properties can be changed for output
			* streams easily using filters */

			if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
				if (dict) { av_free(dict); dict = NULL; }
				av_dict_set(&dict, "preset", "slow", 0);
				av_dict_set(&dict, "vprofile", "baseline", 0);
				av_opt_set(enc_ctx->priv_data, "profile", "baseline", AV_OPT_SEARCH_CHILDREN);
				enc_ctx->profile = FF_PROFILE_H264_BASELINE;
				enc_ctx->height = dec_ctx->height;
				enc_ctx->width = dec_ctx->width;
				enc_ctx->sample_aspect_ratio = dec_ctx->sample_aspect_ratio;
				av_log(NULL, AV_LOG_DEBUG, "Video framerate: %u/%u", dec_ctx->framerate.num, dec_ctx->framerate.den);
				/* take first format from list of supported formats */
				enc_ctx->pix_fmt = encoder->pix_fmts[0];
				/* video time_base can be set to whatever is handy and supported by encoder */
				//AVRational enc_time_base = dec_ctx->time_base;
				//enc_time_base.den /= 2;
				enc_ctx->time_base = dec_ctx->time_base;
				/* Dirty hack*/
				//AVRational r = { 1, 24 };
				//enc_ctx->time_base = r;
				enc_ctx->gop_size = 12; /* emit one intra frame every ten frames */
				enc_ctx->max_b_frames = 1;
				enc_ctx->bit_rate = 500 * 1000;
				enc_ctx->bit_rate_tolerance = 0;
				enc_ctx->rc_max_rate = 0;
				enc_ctx->rc_buffer_size = 0;
				enc_ctx->gop_size = 12;
				enc_ctx->max_b_frames = 3;
				enc_ctx->b_frame_strategy = 1;
				enc_ctx->coder_type = 1;
				enc_ctx->me_method = ME_HEX;
				enc_ctx->me_subpel_quality = 5;
				enc_ctx->me_cmp = 1;
				enc_ctx->me_range = 16;
				enc_ctx->qmin = 10;
				enc_ctx->qmax = 51;
				enc_ctx->scenechange_threshold = 40;
				enc_ctx->flags |= CODEC_FLAG_LOOP_FILTER;
				enc_ctx->i_quant_factor = 0.71;
				enc_ctx->qcompress = 0.6;
				enc_ctx->max_qdiff = 4;
				//c->directpred = 1;
				//c->flags2 |= CODEC_FLAG2_FASTPSKIP;
			}
			else {
				if (dict) { av_free(dict); dict = NULL; }
				enc_ctx->sample_rate = dec_ctx->sample_rate;
				enc_ctx->channel_layout = dec_ctx->channel_layout;
				enc_ctx->channels = av_get_channel_layout_nb_channels(enc_ctx->channel_layout);
				/* take first format from list of supported formats */
				enc_ctx->sample_fmt = encoder->sample_fmts[0];
				//AVRational r = { 1, enc_ctx->sample_rate };
				enc_ctx->time_base = dec_ctx->time_base;
				/* Even dirtier hack*/
				enc_ctx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
			}

			/* Third parameter can be used to pass settings to encoder */
			ret = avcodec_open2(enc_ctx, encoder, &dict);
			if (ret < 0) {
				av_log(NULL, AV_LOG_ERROR, "Cannot open audio or video encoder for stream #%u\n", i);
				return ret;
			}
		}
		else if (dec_ctx->codec_type == AVMEDIA_TYPE_UNKNOWN) {
			av_log(NULL, AV_LOG_FATAL, "Elementary stream #%d is of unknown type, cannot proceed\n", i);
			return AVERROR_INVALIDDATA;
		}
		else {
			/* if this stream must be remuxed */
			ret = avcodec_copy_context(ofmt_ctx->streams[i]->codec,
				ifmt_ctx->streams[i]->codec);
			if (ret < 0) {
				av_log(NULL, AV_LOG_ERROR, "Copying stream context failed\n");
				return ret;
			}
		}

		if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
			enc_ctx->flags |= CODEC_FLAG_GLOBAL_HEADER;

	}
	av_dump_format(ofmt_ctx, 0, filename, 1);

	if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
		ret = avio_open(&ofmt_ctx->pb, filename, AVIO_FLAG_WRITE);
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR, "Could not open output file '%s'", filename);
			return ret;
		}
	}

	/* init muxer, write output file header */
	if (dict) { av_free(dict); dict = NULL; }
	av_dict_set(&dict, "movflags", "faststart", 0);
	ret = avformat_write_header(ofmt_ctx, &dict);
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, "Error occurred when opening output file\n");
		return ret;
	}

	return 0;
}

static int init_filter(FilteringContext* fctx, AVCodecContext *dec_ctx,
	AVCodecContext *enc_ctx, const char *filter_spec)
{
	char args[512];
	int ret = 0;
	AVFilter *buffersrc = NULL;
	AVFilter *buffersink = NULL;
	AVFilterContext *buffersrc_ctx = NULL;
	AVFilterContext *buffersink_ctx = NULL;
	AVFilterInOut *outputs = avfilter_inout_alloc();
	AVFilterInOut *inputs = avfilter_inout_alloc();
	AVFilterGraph *filter_graph = avfilter_graph_alloc();

	if (!outputs || !inputs || !filter_graph) {
		ret = AVERROR(ENOMEM);
		goto end;
	}

	if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
		buffersrc = avfilter_get_by_name("buffer");
		buffersink = avfilter_get_by_name("buffersink");
		if (!buffersrc || !buffersink) {
			av_log(NULL, AV_LOG_ERROR, "filtering source or sink element not found\n");
			ret = AVERROR_UNKNOWN;
			goto end;
		}

		_snprintf(args, sizeof(args),
			"video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
			dec_ctx->width, dec_ctx->height, dec_ctx->pix_fmt,
			dec_ctx->time_base.num, dec_ctx->time_base.den,
			dec_ctx->sample_aspect_ratio.num,
			dec_ctx->sample_aspect_ratio.den);

		ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
			args, NULL, filter_graph);
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR, "Cannot create buffer source\n");
			goto end;
		}

		ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
			NULL, NULL, filter_graph);
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR, "Cannot create buffer sink\n");
			goto end;
		}

		ret = av_opt_set_bin(buffersink_ctx, "pix_fmts",
			(uint8_t*)&enc_ctx->pix_fmt, sizeof(enc_ctx->pix_fmt),
			AV_OPT_SEARCH_CHILDREN);
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR, "Cannot set output pixel format\n");
			goto end;
		}
	}
	else if (dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
		buffersrc = avfilter_get_by_name("abuffer");
		buffersink = avfilter_get_by_name("abuffersink");
		if (!buffersrc || !buffersink) {
			av_log(NULL, AV_LOG_ERROR, "filtering source or sink element not found\n");
			ret = AVERROR_UNKNOWN;
			goto end;
		}

		if (!dec_ctx->channel_layout)
			dec_ctx->channel_layout =
			av_get_default_channel_layout(dec_ctx->channels);
		_snprintf(args, sizeof(args),
			"time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%"PRIx64,
			dec_ctx->time_base.num, dec_ctx->time_base.den, dec_ctx->sample_rate,
			av_get_sample_fmt_name(dec_ctx->sample_fmt),
			dec_ctx->channel_layout);
		ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
			args, NULL, filter_graph);
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer source\n");
			goto end;
		}

		ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
			NULL, NULL, filter_graph);
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer sink\n");
			goto end;
		}

		ret = av_opt_set_bin(buffersink_ctx, "sample_fmts",
			(uint8_t*)&enc_ctx->sample_fmt, sizeof(enc_ctx->sample_fmt),
			AV_OPT_SEARCH_CHILDREN);
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR, "Cannot set output sample format\n");
			goto end;
		}

		ret = av_opt_set_bin(buffersink_ctx, "channel_layouts",
			(uint8_t*)&enc_ctx->channel_layout,
			sizeof(enc_ctx->channel_layout), AV_OPT_SEARCH_CHILDREN);
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR, "Cannot set output channel layout\n");
			goto end;
		}

		ret = av_opt_set_bin(buffersink_ctx, "sample_rates",
			(uint8_t*)&enc_ctx->sample_rate, sizeof(enc_ctx->sample_rate),
			AV_OPT_SEARCH_CHILDREN);
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR, "Cannot set output sample rate\n");
			goto end;
		}
	}
	else {
		ret = AVERROR_UNKNOWN;
		goto end;
	}

	/* Endpoints for the filter graph. */
	outputs->name = av_strdup("in");
	outputs->filter_ctx = buffersrc_ctx;
	outputs->pad_idx = 0;
	outputs->next = NULL;

	inputs->name = av_strdup("out");
	inputs->filter_ctx = buffersink_ctx;
	inputs->pad_idx = 0;
	inputs->next = NULL;

	if (!outputs->name || !inputs->name) {
		ret = AVERROR(ENOMEM);
		goto end;
	}

	if ((ret = avfilter_graph_parse_ptr(filter_graph, filter_spec,
		&inputs, &outputs, NULL)) < 0)
		goto end;

	if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0)
		goto end;

	/* Fill FilteringContext */
	fctx->buffersrc_ctx = buffersrc_ctx;
	fctx->buffersink_ctx = buffersink_ctx;
	fctx->filter_graph = filter_graph;

end:
	avfilter_inout_free(&inputs);
	avfilter_inout_free(&outputs);

	return ret;
}

static int init_filters(void)
{
	const char *filter_spec;
	unsigned int i;
	int ret;
	filter_ctx = (FilteringContext*) av_malloc_array(ifmt_ctx->nb_streams, sizeof(*filter_ctx));
	if (!filter_ctx)
		return AVERROR(ENOMEM);

	for (i = 0; i < ifmt_ctx->nb_streams; i++) {
		filter_ctx[i].buffersrc_ctx = NULL;
		filter_ctx[i].buffersink_ctx = NULL;
		filter_ctx[i].filter_graph = NULL;
		if (!(ifmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO
			|| ifmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO))
			continue;


		if (ifmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
			filter_spec = "null"; /* passthrough (dummy) filter for video */
		else
			filter_spec = "anull"; /* passthrough (dummy) filter for audio */
		ret = init_filter(&filter_ctx[i], ifmt_ctx->streams[i]->codec,
			ofmt_ctx->streams[i]->codec, filter_spec);
		if (ret)
			return ret;
	}
	return 0;
}

static int encode_write_frame(AVFrame *filt_frame, unsigned int stream_index, int *got_frame) {
	int ret;
	int got_frame_local;
	AVPacket enc_pkt;
	int(*enc_func)(AVCodecContext *, AVPacket *, const AVFrame *, int *) =
		(ifmt_ctx->streams[stream_index]->codec->codec_type ==
		AVMEDIA_TYPE_VIDEO) ? avcodec_encode_video2 : avcodec_encode_audio2;

	if (!got_frame)
		got_frame = &got_frame_local;

	av_log(NULL, AV_LOG_INFO, "Encoding frame\n");
	/* encode filtered frame */
	enc_pkt.data = NULL;
	enc_pkt.size = 0;
	av_init_packet(&enc_pkt);
	ret = enc_func(ofmt_ctx->streams[stream_index]->codec, &enc_pkt,
		filt_frame, got_frame);
	av_frame_free(&filt_frame);
	if (ret < 0)
		return ret;
	if (!(*got_frame))
		return 0;

	/* prepare packet for muxing */
	enc_pkt.stream_index = stream_index;
	av_packet_rescale_ts(&enc_pkt,
		ofmt_ctx->streams[stream_index]->codec->time_base,
		ofmt_ctx->streams[stream_index]->time_base);

	av_log(NULL, AV_LOG_DEBUG, "Muxing frame\n");
	/* mux encoded frame */
	ret = av_interleaved_write_frame(ofmt_ctx, &enc_pkt);
	return ret;
}

static int filter_encode_write_frame(AVFrame *frame, unsigned int stream_index)
{
	int ret;
	AVFrame *filt_frame;

	av_log(NULL, AV_LOG_INFO, "Pushing decoded frame to filters\n");
	/* push the decoded frame into the filtergraph */
	ret = av_buffersrc_add_frame_flags(filter_ctx[stream_index].buffersrc_ctx,
		frame, 0);
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, "Error while feeding the filtergraph\n");
		return ret;
	}

	/* pull filtered frames from the filtergraph */
	while (1) {
		filt_frame = av_frame_alloc();
		if (!filt_frame) {
			ret = AVERROR(ENOMEM);
			break;
		}
		av_log(NULL, AV_LOG_INFO, "Pulling filtered frame from filters\n");
		ret = av_buffersink_get_frame(filter_ctx[stream_index].buffersink_ctx,
			filt_frame);
		if (ret < 0) {
			/* if no more frames for output - returns AVERROR(EAGAIN)
			* if flushed and no more frames for output - returns AVERROR_EOF
			* rewrite retcode to 0 to show it as normal procedure completion
			*/
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
				ret = 0;
			av_frame_free(&filt_frame);
			break;
		}

		filt_frame->pict_type = AV_PICTURE_TYPE_NONE;
		ret = encode_write_frame(filt_frame, stream_index, NULL);
		if (ret < 0)
			break;
	}

	return ret;
}

static int flush_encoder(unsigned int stream_index)
{
	int ret;
	int got_frame;

	if (!(ofmt_ctx->streams[stream_index]->codec->codec->capabilities &
		CODEC_CAP_DELAY))
		return 0;

	while (1) {
		av_log(NULL, AV_LOG_INFO, "Flushing stream #%u encoder\n", stream_index);
		ret = encode_write_frame(NULL, stream_index, &got_frame);
		if (ret < 0)
			break;
		if (!got_frame)
			return 0;
	}
	return ret;
}

int transcoding(int argc, char **argv)
{
	int ret;
	AVPacket packet;
	packet.data = NULL;
	packet.size = 0;
	AVFrame *frame = NULL;
	enum AVMediaType type;
	unsigned int stream_index;
	unsigned int i;
	int got_frame;
	int(*dec_func)(AVCodecContext *, AVFrame *, int *, const AVPacket *);

	if (argc != 3) {
		av_log(NULL, AV_LOG_ERROR, "Usage: %s <input file> <output file>\n", argv[0]);
		return 1;
	}

	printf("Using input file %s and output file %s", argv[1], argv[2]);

	av_register_all();
	avcodec_register_all();
	avfilter_register_all();

	if ((ret = open_input_file(argv[1])) < 0)
		goto end;
	if ((ret = open_output_file(argv[2])) < 0)
		goto end;
	if ((ret = init_filters()) < 0)
		goto end;

	/* read all packets */
	while (1) {
		if ((ret = av_read_frame(ifmt_ctx, &packet)) < 0)
			break;
		stream_index = packet.stream_index;
		type = ifmt_ctx->streams[packet.stream_index]->codec->codec_type;
		av_log(NULL, AV_LOG_DEBUG, "Demuxer gave frame of stream_index %u\n",
			stream_index);

		if (filter_ctx[stream_index].filter_graph) {
			av_log(NULL, AV_LOG_DEBUG, "Going to reencode&filter the frame\n");
			frame = av_frame_alloc();
			if (!frame) {
				ret = AVERROR(ENOMEM);
				break;
			}
			av_packet_rescale_ts(&packet,
				ifmt_ctx->streams[stream_index]->time_base,
				ifmt_ctx->streams[stream_index]->codec->time_base);
			dec_func = (type == AVMEDIA_TYPE_VIDEO) ? avcodec_decode_video2 :
				avcodec_decode_audio4;
			ret = dec_func(ifmt_ctx->streams[stream_index]->codec, frame,
				&got_frame, &packet);
			if (ret < 0) {
				av_frame_free(&frame);
				av_log(NULL, AV_LOG_ERROR, "Decoding failed\n");
				break;
			}

			if (got_frame) {
				frame->pts = av_frame_get_best_effort_timestamp(frame);
				ret = filter_encode_write_frame(frame, stream_index);
				av_frame_free(&frame);
				if (ret < 0)
					goto end;
			}
			else {
				av_frame_free(&frame);
			}
		}
		else {
			/* remux this frame without reencoding */
			av_packet_rescale_ts(&packet,
				ifmt_ctx->streams[stream_index]->time_base,
				ofmt_ctx->streams[stream_index]->time_base);

			ret = av_interleaved_write_frame(ofmt_ctx, &packet);
			if (ret < 0)
				goto end;
		}
		av_free_packet(&packet);
	}

	/* flush filters and encoders */
	for (i = 0; i < ifmt_ctx->nb_streams; i++) {
		/* flush filter */
		if (!filter_ctx[i].filter_graph)
			continue;
		ret = filter_encode_write_frame(NULL, i);
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR, "Flushing filter failed\n");
			goto end;
		}

		/* flush encoder */
		ret = flush_encoder(i);
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR, "Flushing encoder failed\n");
			goto end;
		}
	}

	av_write_trailer(ofmt_ctx);
end:
	av_free_packet(&packet);
	av_frame_free(&frame);
	for (i = 0; i < ifmt_ctx->nb_streams; i++) {
		avcodec_close(ifmt_ctx->streams[i]->codec);
		if (ofmt_ctx && ofmt_ctx->nb_streams > i && ofmt_ctx->streams[i] && ofmt_ctx->streams[i]->codec)
			avcodec_close(ofmt_ctx->streams[i]->codec);
		if (filter_ctx && filter_ctx[i].filter_graph)
			avfilter_graph_free(&filter_ctx[i].filter_graph);
	}
	av_free(filter_ctx);
	avformat_close_input(&ifmt_ctx);
	if (ofmt_ctx && !(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
		avio_closep(&ofmt_ctx->pb);
	avformat_free_context(ofmt_ctx);

	if (ret < 0) {
		av_log_error(ret);
	}

	return ret ? 1 : 0;
}

int _tmain(int argc, _TCHAR* argv[])
{
	//return tutorial1();
	//return video_encode_example("video_encode_example.mp4");
	return transcoding(argc, (char**)argv);
}
