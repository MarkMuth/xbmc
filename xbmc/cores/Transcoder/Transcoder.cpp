/*
*      Copyright (C) 2005-2013 Team XBMC
*      http://xbmc.org
*
*  This Program is free software; you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 2, or (at your option)
*  any later version.
*
*  This Program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with XBMC; see the file COPYING.  If not, see
*  <http://www.gnu.org/licenses/>.
*
*/

#include "Transcoder.h"

#include <stdio.h>

void CTranscoder::LogError(int errnum)
{
	char* buf = (char*) malloc(AV_ERROR_MAX_STRING_SIZE * sizeof(char));
	av_log(NULL, AV_LOG_ERROR, "Error occurred: %s\n", av_make_error_string(buf, AV_ERROR_MAX_STRING_SIZE, errnum));
	free(buf);
}

int CTranscoder::OpenInputFile(const char *filename)
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

int CTranscoder::OpenOutputFile(const char *filename)
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

int CTranscoder::InitFilter(FilteringContext* fctx, AVCodecContext *dec_ctx,
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

int CTranscoder::InitFilters(void)
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
		ret = InitFilter(&filter_ctx[i], ifmt_ctx->streams[i]->codec,
			ofmt_ctx->streams[i]->codec, filter_spec);
		if (ret)
			return ret;
	}
	return 0;
}

int CTranscoder::EncodeWriteFrame(AVFrame *filt_frame, unsigned int stream_index, int *got_frame) {
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

int CTranscoder::FilterEncodeWriteFrame(AVFrame *frame, unsigned int stream_index)
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
		ret = EncodeWriteFrame(filt_frame, stream_index, NULL);
		if (ret < 0)
			break;
	}

	return ret;
}

int CTranscoder::FlushEncoder(unsigned int stream_index)
{
	int ret;
	int got_frame;

	if (!(ofmt_ctx->streams[stream_index]->codec->codec->capabilities &
		CODEC_CAP_DELAY))
		return 0;

	while (1) {
		av_log(NULL, AV_LOG_INFO, "Flushing stream #%u encoder\n", stream_index);
    ret = EncodeWriteFrame(NULL, stream_index, &got_frame);
		if (ret < 0)
			break;
		if (!got_frame)
			return 0;
	}
	return ret;
}

int CTranscoder::Transcode(std::string path)
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

	if (path.empty()) {
		av_log(NULL, AV_LOG_ERROR, "Path to input file must not be empty.\n");
		return 1;
	}


  std::string pathOut = TranscodePath(path);
	printf("Using input file %s and output file %s", path.c_str(), pathOut.c_str());

	av_register_all();
	avcodec_register_all();
	avfilter_register_all();

  if ((ret = OpenInputFile(path.c_str())) < 0)
		goto end;
  if ((ret = OpenOutputFile(pathOut.c_str())) < 0)
		goto end;
	if ((ret = InitFilters()) < 0)
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
				ret = FilterEncodeWriteFrame(frame, stream_index);
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
    ret = FilterEncodeWriteFrame(NULL, i);
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR, "Flushing filter failed\n");
			goto end;
		}

		/* flush encoder */
		ret = FlushEncoder(i);
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
    LogError(ret);
	}

	return ret ? 1 : 0;
}

std::string CTranscoder::TranscodePath(const std::string &path) const
{
  return path.substr(0, path.find_last_of('.')) + std::string("-transcoded.mp4");
}
