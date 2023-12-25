/******************************************************************************
    Copyright (C) 2023 by Dennis Sädtler <dennis@obsproject.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include <obs-avc.h>
#ifdef ENABLE_HEVC
#include <obs-hevc.h>
#endif

#include "obs-ffmpeg-video-encoders.h"

#define do_log(level, format, ...)                                    \
	blog(level, "[FFmpeg MediaFoundation encoder: '%s'] " format, \
	     obs_encoder_get_name(enc->ffve.encoder), ##__VA_ARGS__)

#define warn(format, ...) do_log(LOG_WARNING, format, ##__VA_ARGS__)
#define info(format, ...) do_log(LOG_INFO, format, ##__VA_ARGS__)
#define debug(format, ...) do_log(LOG_DEBUG, format, ##__VA_ARGS__)

struct mf_encoder {
	struct ffmpeg_video_encoder ffve;

#ifdef ENABLE_HEVC
	bool hevc;
#endif

	DARRAY(uint8_t) header;
	DARRAY(uint8_t) sei;
};

#define ENCODER_NAME_H264 "MediaFoundation H.264 (FFmpeg)"
static const char *h264_mf_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return ENCODER_NAME_H264;
}

#ifdef ENABLE_HEVC
#define ENCODER_NAME_HEVC "MediaFoundation HEVC (FFmpeg)"
static const char *hevc_mf_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return ENCODER_NAME_HEVC;
}
#endif

static void mf_video_info(void *data, struct video_scale_info *info)
{
	UNUSED_PARAMETER(data);
	/* No 10 bit support, FFmpeg code comments say NV12 has the best
	 * compatibility, so stick with that. */
	info->format = VIDEO_FORMAT_NV12;
}

static bool mf_configure(struct mf_encoder *enc, obs_data_t *settings)
{
	const char *rc = obs_data_get_string(settings, "rate_control");
	const char *scenario = obs_data_get_string(settings, "scenario");
	const char *profile = obs_data_get_string(settings, "profile");
	int bitrate = (int)obs_data_get_int(settings, "bitrate");
	int quality = (int)obs_data_get_int(settings, "quality");
	int keyint_sec = (int)obs_data_get_int(settings, "keyint_sec");
	int bf = (int)obs_data_get_int(settings, "bf");

	video_t *video = obs_encoder_video(enc->ffve.encoder);
	const struct video_output_info *voi = video_output_get_info(video);
	struct video_scale_info info;

	/* No 10 bit support, FFmpeg code comments say NV12 has the best
	 * compatibility, so stick with that. */
	info.format = voi->format;
	info.colorspace = voi->colorspace;
	info.range = voi->range;
	mf_video_info(NULL, &info);

	/* MF doesn't allow us to set a device and will just auto-select one,
	 * only thing we can do is force hardware encoding */
	av_opt_set_int(enc->ffve.context->priv_data, "hw_encoding", true, 0);
	av_opt_set_int(enc->ffve.context->priv_data, "quality", quality, 0);

	av_opt_set(enc->ffve.context->priv_data, "rate_control", rc, 0);
	av_opt_set(enc->ffve.context->priv_data, "scenario", scenario, 0);

	enc->ffve.context->max_b_frames = bf;

	if (!enc->hevc) {
		enc->ffve.context->profile = strcmp(profile, "high") == 0
						     ? FF_PROFILE_H264_HIGH
						     : FF_PROFILE_H264_MAIN;
	}

	ffmpeg_video_encoder_update(&enc->ffve, bitrate, keyint_sec, voi, &info,
				    NULL);

	info("settings:\n"
	     "\tencoder:      %s\n"
	     "\trate_control: %s\n"
	     "\tbitrate:      %d\n"
	     "\tquality:      %d\n"
	     "\tkeyint:       %d\n"
	     "\tprofile:      %s\n"
	     "\twidth:        %d\n"
	     "\theight:       %d\n"
	     "\tb-frames:     %d\n",
	     enc->ffve.enc_name, rc, bitrate, quality,
	     enc->ffve.context->gop_size, profile, enc->ffve.context->width,
	     enc->ffve.height, enc->ffve.context->max_b_frames);

	return ffmpeg_video_encoder_init_codec(&enc->ffve);
}

static bool mf_update(void *data, obs_data_t *settings)
{
	struct mf_encoder *enc = data;
	const int64_t bitrate = obs_data_get_int(settings, "bitrate");

	// ToDo figure out if this even works
	const int64_t rate = bitrate * 1000;
	enc->ffve.context->bit_rate = rate;
	enc->ffve.context->rc_max_rate = rate;

	return true;
}

static void mf_destroy(void *data)
{
	struct mf_encoder *enc = data;
	ffmpeg_video_encoder_free(&enc->ffve);
	da_free(enc->header);
	da_free(enc->sei);
	bfree(enc);
}

static void on_init_error(void *data, int ret)
{
	struct mf_encoder *enc = data;
	struct dstr error_message = {0};

	// ToDo MF error codes
	dstr_copy(&error_message, obs_module_text("NVENC.Error"));
	dstr_replace(&error_message, "%1", av_err2str(ret));
	dstr_cat(&error_message, "<br><br>");

	if (ret == AVERROR_EXTERNAL) {
		// special case for common NVENC error
		dstr_cat(&error_message, obs_module_text("NVENC.GenericError"));
	} else {
		dstr_cat(&error_message, obs_module_text("NVENC.CheckDrivers"));
	}

	obs_encoder_set_last_error(enc->ffve.encoder, error_message.array);
	dstr_free(&error_message);
}

static void on_first_packet(void *data, AVPacket *pkt, struct darray *da)
{
	struct mf_encoder *enc = data;

	darray_free(da);
#ifdef ENABLE_HEVC
	if (enc->hevc) {
		obs_extract_hevc_headers(pkt->data, pkt->size,
					 (uint8_t **)&da->array, &da->num,
					 &enc->header.array, &enc->header.num,
					 &enc->sei.array, &enc->sei.num);
	} else
#endif
	{
		obs_extract_avc_headers(pkt->data, pkt->size,
					(uint8_t **)&da->array, &da->num,
					&enc->header.array, &enc->header.num,
					&enc->sei.array, &enc->sei.num);
	}

	da->capacity = da->num;
}

static void *mf_create_internal(obs_data_t *settings, obs_encoder_t *encoder,
				bool hevc)
{
	struct mf_encoder *enc = bzalloc(sizeof(*enc));

#ifdef ENABLE_HEVC
	enc->hevc = hevc;
	if (hevc) {
		if (!ffmpeg_video_encoder_init(
			    &enc->ffve, enc, encoder, "hevc_mf", NULL,
			    ENCODER_NAME_HEVC, on_init_error, on_first_packet))
			goto fail;
	} else
#else
	UNUSED_PARAMETER(hevc);
#endif
	{
		if (!ffmpeg_video_encoder_init(
			    &enc->ffve, enc, encoder, "h264_mf", NULL,
			    ENCODER_NAME_H264, on_init_error, on_first_packet))
			goto fail;
	}

	if (!mf_configure(enc, settings))
		goto fail;

	return enc;

fail:
	mf_destroy(enc);
	return NULL;
}

static void *h264_mf_create(obs_data_t *settings, obs_encoder_t *encoder)
{
	video_t *video = obs_encoder_video(encoder);
	const struct video_output_info *voi = video_output_get_info(video);
	switch (voi->format) {
	case VIDEO_FORMAT_I010:
	case VIDEO_FORMAT_P010: {
		const char *const text =
			obs_module_text("NVENC.10bitUnsupported");
		obs_encoder_set_last_error(encoder, text);
		blog(LOG_ERROR, "[NVENC encoder] %s", text);
		return NULL;
	}
	case VIDEO_FORMAT_P216:
	case VIDEO_FORMAT_P416: {
		const char *const text =
			obs_module_text("NVENC.16bitUnsupported");
		obs_encoder_set_last_error(encoder, text);
		blog(LOG_ERROR, "[NVENC encoder] %s", text);
		return NULL;
	}
	default:
		if (voi->colorspace == VIDEO_CS_2100_PQ ||
		    voi->colorspace == VIDEO_CS_2100_HLG) {
			const char *const text =
				obs_module_text("NVENC.8bitUnsupportedHdr");
			obs_encoder_set_last_error(encoder, text);
			blog(LOG_ERROR, "[NVENC encoder] %s", text);
			return NULL;
		}
		break;
	}

	return mf_create_internal(settings, encoder, false);
}

#ifdef ENABLE_HEVC
static void *hevc_mf_create(obs_data_t *settings, obs_encoder_t *encoder)
{
	video_t *video = obs_encoder_video(encoder);
	const struct video_output_info *voi = video_output_get_info(video);
	switch (voi->format) {
	case VIDEO_FORMAT_I010: {
		const char *const text =
			obs_module_text("NVENC.I010Unsupported");
		obs_encoder_set_last_error(encoder, text);
		blog(LOG_ERROR, "[NVENC encoder] %s", text);
		return NULL;
	}
	case VIDEO_FORMAT_P010:
		break;
	case VIDEO_FORMAT_P216:
	case VIDEO_FORMAT_P416: {
		const char *const text =
			obs_module_text("NVENC.16bitUnsupported");
		obs_encoder_set_last_error(encoder, text);
		blog(LOG_ERROR, "[NVENC encoder] %s", text);
		return NULL;
	}
	default:
		if (voi->colorspace == VIDEO_CS_2100_PQ ||
		    voi->colorspace == VIDEO_CS_2100_HLG) {
			const char *const text =
				obs_module_text("NVENC.8bitUnsupportedHdr");
			obs_encoder_set_last_error(encoder, text);
			blog(LOG_ERROR, "[NVENC encoder] %s", text);
			return NULL;
		}
		break;
	}

	return mf_create_internal(settings, encoder, true);
}
#endif

static bool mf_encode(void *data, struct encoder_frame *frame,
		      struct encoder_packet *packet, bool *received_packet)
{
	struct mf_encoder *enc = data;

	if (!ffmpeg_video_encode(&enc->ffve, frame, packet, received_packet))
		return false;

	return true;
}

static void mf_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, "bitrate", 2500);
	obs_data_set_default_int(settings, "keyint_sec", 0);
	obs_data_set_default_int(settings, "quality", -1);
	obs_data_set_default_string(settings, "rate_control", "cbr");
	obs_data_set_default_string(settings, "scenario", "default");
	obs_data_set_default_string(settings, "profile", "main");
	obs_data_set_default_int(settings, "bf", 2);
}

static bool rate_control_modified(obs_properties_t *ppts, obs_property_t *p,
				  obs_data_t *settings)
{
	const char *rc = obs_data_get_string(settings, "rate_control");
	bool quality_mode = strcmp(rc, "quality") == 0;

	// ToDo does bitrate do anything in quality mode?
	p = obs_properties_get(ppts, "quality");
	obs_property_set_visible(p, quality_mode);

	return true;
}

obs_properties_t *mf_properties(void *unused)
{
	obs_properties_t *props = obs_properties_create();
	obs_property_t *p;

	p = obs_properties_add_list(props, "rate_control",
				    obs_module_text("RateControl"),
				    OBS_COMBO_TYPE_LIST,
				    OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(p, "CBR", "cbr");
	obs_property_list_add_string(p, "Peak constrained VBR", "pc_vbr");
	obs_property_list_add_string(p, "Unconstrained VBR", "u_vbr");
	obs_property_list_add_string(p, "Low delay VBR", "ld_vbr");
	obs_property_list_add_string(p, "Global VBR", "g_vbr");
	obs_property_list_add_string(p, "Global low delay VBR", "gld_vbr");
	obs_property_list_add_string(p, "Quality", "quality");

	obs_property_set_modified_callback(p, rate_control_modified);

	p = obs_properties_add_int(props, "bitrate", obs_module_text("Bitrate"),
				   50, 300000, 50);
	obs_property_int_set_suffix(p, " Kbps");

	obs_properties_add_int(props, "quality", obs_module_text("Quality"), -1,
			       100, 1);

	p = obs_properties_add_int(props, "keyint_sec",
				   obs_module_text("KeyframeIntervalSec"), 0,
				   10, 1);
	obs_property_int_set_suffix(p, " s");

	p = obs_properties_add_list(props, "scenario",
				    obs_module_text("Scenario"),
				    OBS_COMBO_TYPE_LIST,
				    OBS_COMBO_FORMAT_STRING);

#define add_scenario(val)                                                    \
	obs_property_list_add_string(p, obs_module_text("MF.Scenario." val), \
				     val)

	add_scenario("default");
	add_scenario("display_remoting");
	add_scenario("video_conference");
	add_scenario("archive");
	add_scenario("live_streaming");
	add_scenario("camera_record");
	// ROI stuff?
	// add_scenario("display_remoting_with_feature_map");
#undef add_preset

	p = obs_properties_add_list(props, "profile",
				    obs_module_text("Profile"),
				    OBS_COMBO_TYPE_LIST,
				    OBS_COMBO_FORMAT_STRING);

#define add_profile(val) obs_property_list_add_string(p, val, val)

	// ToDo HEVC
	add_profile("high");
	add_profile("main");
	add_profile("baseline");
#undef add_profile

	obs_properties_add_int(props, "bf", obs_module_text("BFrames"), 0, 4,
			       1);

	return props;
}

static bool mf_extra_data(void *data, uint8_t **extra_data, size_t *size)
{
	struct mf_encoder *enc = data;

	*extra_data = enc->header.array;
	*size = enc->header.num;
	return true;
}

static bool mf_sei_data(void *data, uint8_t **extra_data, size_t *size)
{
	struct mf_encoder *enc = data;

	*extra_data = enc->sei.array;
	*size = enc->sei.num;
	return true;
}

struct obs_encoder_info h264_mf_encoder_info = {
	.id = "ffmpeg_h264_mf",
	.type = OBS_ENCODER_VIDEO,
	.codec = "h264",
	.get_name = h264_mf_getname,
	.create = h264_mf_create,
	.destroy = mf_destroy,
	.encode = mf_encode,
	.update = mf_update,
	.get_defaults = mf_defaults,
	.get_properties = mf_properties,
	.get_extra_data = mf_extra_data,
	.get_sei_data = mf_sei_data,
	.get_video_info = mf_video_info,
	.caps = OBS_ENCODER_CAP_DYN_BITRATE,
};

#ifdef ENABLE_HEVC
struct obs_encoder_info hevc_mf_encoder_info = {
	.id = "ffmpeg_hevc_mf",
	.type = OBS_ENCODER_VIDEO,
	.codec = "hevc",
	.get_name = hevc_mf_getname,
	.create = hevc_mf_create,
	.destroy = mf_destroy,
	.encode = mf_encode,
	.update = mf_update,
	.get_defaults = mf_defaults,
	.get_properties = mf_properties,
	.get_extra_data = mf_extra_data,
	.get_sei_data = mf_sei_data,
	.get_video_info = mf_video_info,
	.caps = OBS_ENCODER_CAP_DYN_BITRATE,
};
#endif
