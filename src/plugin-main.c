/*
 * StreamAssistant Camera
 * Copyright (C) 2026 Mitchell
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <obs-module.h>
#include <plugin-support.h>

#include "camera-config.h"
#include "platform-services.h"
#include "signaling-metadata.h"

#include <stdio.h>
#include <string.h>

#define CAMERA_SOURCE_ID "streamassistant_camera"
#define CAMERA_DEFAULT_WIDTH 1920
#define CAMERA_DEFAULT_HEIGHT 1080

#define SETTING_SESSION "session_id"
#define SETTING_SOURCE "source_id"
#define SETTING_HOST "host_secret"
#define SETTING_GUEST "guest_secret"
#define SETTING_PUBLISHER "publisher_secret"
#define SETTING_VIEWER "viewer_secret"
#define SETTING_PAIRING_OPENED "pairing_opened"
#define SETTING_AUDIO_ROUTING_MIGRATED "audio_routing_migrated"
#define SETTING_OUTPUT_WIDTH "output_width"
#define SETTING_OUTPUT_HEIGHT "output_height"
#define SETTING_STATUS "status"
#define SETTING_PAIRING_HELP "pairing_help"
#define SETTING_AUDIO_HELP "audio_help"

struct streamassistant_camera {
	obs_source_t *source;
	obs_source_t *browser;
	uint32_t audio_sample_rate;
	enum speaker_layout audio_speakers;
	bool audio_received_logged;
	uint32_t width;
	uint32_t height;
	struct camera_config config;
	struct signaling_metadata_client *metadata;
};

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

static void copy_setting(char *destination, size_t capacity, obs_data_t *settings, const char *name)
{
	const char *value = obs_data_get_string(settings, name);
	snprintf(destination, capacity, "%s", value ? value : "");
}

static void read_config(obs_data_t *settings, struct camera_config *config)
{
	copy_setting(config->session_id, sizeof(config->session_id), settings, SETTING_SESSION);
	copy_setting(config->source_id, sizeof(config->source_id), settings, SETTING_SOURCE);
	copy_setting(config->host_secret, sizeof(config->host_secret), settings, SETTING_HOST);
	copy_setting(config->guest_secret, sizeof(config->guest_secret), settings, SETTING_GUEST);
	copy_setting(config->publisher_secret, sizeof(config->publisher_secret), settings, SETTING_PUBLISHER);
	copy_setting(config->viewer_secret, sizeof(config->viewer_secret), settings, SETTING_VIEWER);
}

static void write_config(obs_data_t *settings, const struct camera_config *config)
{
	obs_data_set_string(settings, SETTING_SESSION, config->session_id);
	obs_data_set_string(settings, SETTING_SOURCE, config->source_id);
	obs_data_set_string(settings, SETTING_HOST, config->host_secret);
	obs_data_set_string(settings, SETTING_GUEST, config->guest_secret);
	obs_data_set_string(settings, SETTING_PUBLISHER, config->publisher_secret);
	obs_data_set_string(settings, SETTING_VIEWER, config->viewer_secret);
}

static bool ensure_config(obs_data_t *settings, struct camera_config *config)
{
	read_config(settings, config);
	if (camera_config_is_valid(config))
		return false;

	if (!camera_config_generate(config)) {
		obs_log(LOG_ERROR, "Failed to generate secure pairing credentials");
		memset(config, 0, sizeof(*config));
		return false;
	}

	write_config(settings, config);
	return true;
}

static bool source_type_available(const char *source_id)
{
	const char *candidate = NULL;

	for (size_t index = 0; obs_enum_source_types(index, &candidate); index++) {
		if (candidate && strcmp(candidate, source_id) == 0)
			return true;
	}

	return false;
}

static bool valid_resolution(uint32_t width, uint32_t height)
{
	return (width == 1280 && height == 720) || (width == 720 && height == 1280) ||
	       (width == 1920 && height == 1080) || (width == 1080 && height == 1920);
}

static void read_resolution(obs_data_t *settings, uint32_t *width, uint32_t *height)
{
	const uint32_t candidate_width = (uint32_t)obs_data_get_int(settings, SETTING_OUTPUT_WIDTH);
	const uint32_t candidate_height = (uint32_t)obs_data_get_int(settings, SETTING_OUTPUT_HEIGHT);

	if (valid_resolution(candidate_width, candidate_height)) {
		*width = candidate_width;
		*height = candidate_height;
	} else {
		*width = CAMERA_DEFAULT_WIDTH;
		*height = CAMERA_DEFAULT_HEIGHT;
		obs_data_set_int(settings, SETTING_OUTPUT_WIDTH, *width);
		obs_data_set_int(settings, SETTING_OUTPUT_HEIGHT, *height);
	}
}

static void camera_resolution_received(uint32_t width, uint32_t height, void *context)
{
	struct streamassistant_camera *camera = context;
	obs_source_t *source;
	obs_data_t *settings;

	if (!camera || !valid_resolution(width, height))
		return;

	source = obs_source_get_ref(camera->source);
	if (!source)
		return;

	settings = obs_source_get_settings(source);
	obs_data_set_int(settings, SETTING_OUTPUT_WIDTH, width);
	obs_data_set_int(settings, SETTING_OUTPUT_HEIGHT, height);
	obs_source_update(source, settings);
	obs_data_release(settings);
	obs_log(LOG_INFO, "Source '%s' changed to %ux%u", obs_source_get_name(source), width, height);
	obs_source_release(source);
}

static void ensure_metadata_connection(struct streamassistant_camera *camera)
{
	if (signaling_metadata_matches(camera->metadata, &camera->config))
		return;

	signaling_metadata_stop(camera->metadata);
	camera->metadata = signaling_metadata_start(&camera->config, camera_resolution_received, camera);
	if (!camera->metadata)
		obs_log(LOG_WARNING, "Could not start the camera resolution metadata connection");
}

static obs_data_t *browser_settings(const struct camera_config *config, uint32_t width, uint32_t height)
{
	char receiver_url[CAMERA_URL_CAPACITY];
	obs_data_t *settings;

	if (!camera_build_receiver_url(config, receiver_url, sizeof(receiver_url)))
		return NULL;

	settings = obs_data_create();
	obs_data_set_bool(settings, "is_local_file", false);
	obs_data_set_string(settings, "local_file", "");
	obs_data_set_string(settings, "url", receiver_url);
	obs_data_set_int(settings, "width", width);
	obs_data_set_int(settings, "height", height);
	obs_data_set_bool(settings, "reroute_audio", true);
	obs_data_set_bool(settings, "fps_custom", true);
	obs_data_set_int(settings, "fps", 60);
	obs_data_set_string(settings, "css", "body { background-color: #000; margin: 0; overflow: hidden; }");
	obs_data_set_bool(settings, "shutdown", false);
	obs_data_set_bool(settings, "restart_when_active", false);
	obs_data_set_int(settings, "webpage_control_level", 0);
	return settings;
}

static void update_browser(struct streamassistant_camera *camera)
{
	obs_data_t *settings;

	if (!camera->browser)
		return;

	settings = browser_settings(&camera->config, camera->width, camera->height);
	if (!settings)
		return;

	obs_source_update(camera->browser, settings);
	obs_data_release(settings);
}

static bool open_pairing_page(struct streamassistant_camera *camera)
{
	char pairing_url[CAMERA_URL_CAPACITY];

	if (!camera_build_pairing_url(&camera->config, PLUGIN_VERSION, pairing_url, sizeof(pairing_url)))
		return false;

	if (!camera_platform_open_url(pairing_url)) {
		obs_log(LOG_ERROR, "Failed to open the pairing page");
		return false;
	}

	return true;
}

static const char *camera_get_name(void *type_data)
{
	UNUSED_PARAMETER(type_data);
	return obs_module_text("StreamAssistantCamera");
}

static void camera_browser_audio(void *data, obs_source_t *browser, const struct audio_data *audio_data, bool muted)
{
	struct streamassistant_camera *camera = data;
	struct obs_source_audio output = {0};

	if (!camera || !camera->source || browser != camera->browser || !audio_data || muted ||
	    camera->audio_sample_rate == 0 || audio_data->frames == 0)
		return;

	for (size_t plane = 0; plane < MAX_AV_PLANES; plane++)
		output.data[plane] = audio_data->data[plane];

	output.frames = audio_data->frames;
	output.speakers = camera->audio_speakers;
	output.format = AUDIO_FORMAT_FLOAT_PLANAR;
	output.samples_per_sec = camera->audio_sample_rate;
	output.timestamp = audio_data->timestamp;
	obs_source_output_audio(camera->source, &output);

	if (!camera->audio_received_logged) {
		camera->audio_received_logged = true;
		obs_log(LOG_INFO, "Audio received from the phone");
	}
}

static void camera_update(void *data, obs_data_t *settings)
{
	struct streamassistant_camera *camera = data;

	ensure_config(settings, &camera->config);
	read_resolution(settings, &camera->width, &camera->height);
	ensure_metadata_connection(camera);
	obs_data_set_string(settings, SETTING_STATUS,
			    camera->browser ? obs_module_text("StatusReady") : obs_module_text("StatusBrowserMissing"));
	update_browser(camera);
}

static void *camera_create(obs_data_t *settings, obs_source_t *source)
{
	struct streamassistant_camera *camera = bzalloc(sizeof(*camera));
	struct obs_audio_info audio_info = {0};
	const bool generated = ensure_config(settings, &camera->config);

	camera->source = source;
	read_resolution(settings, &camera->width, &camera->height);
	ensure_metadata_connection(camera);
	if (obs_get_audio_info(&audio_info)) {
		camera->audio_sample_rate = audio_info.samples_per_sec;
		camera->audio_speakers = audio_info.speakers;
	}

	if (source_type_available("browser_source")) {
		obs_data_t *child_settings = browser_settings(&camera->config, camera->width, camera->height);
		if (child_settings) {
			camera->browser = obs_source_create_private("browser_source", "StreamAssistant Camera Receiver",
								    child_settings);
			obs_data_release(child_settings);
		}
	}

	if (camera->browser) {
		obs_source_add_audio_capture_callback(camera->browser, camera_browser_audio, camera);
		if (!obs_source_add_active_child(camera->source, camera->browser)) {
			obs_log(LOG_ERROR, "Failed to activate the private Browser Source");
			obs_source_remove_audio_capture_callback(camera->browser, camera_browser_audio, camera);
			obs_source_release(camera->browser);
			camera->browser = NULL;
		}
	}

	obs_data_set_string(settings, SETTING_STATUS,
			    camera->browser ? obs_module_text("StatusReady") : obs_module_text("StatusBrowserMissing"));

	if (generated && !obs_data_get_bool(settings, SETTING_PAIRING_OPENED)) {
		obs_data_set_bool(settings, SETTING_PAIRING_OPENED, true);
		open_pairing_page(camera);
	}

	obs_log(LOG_INFO, "Camera source created");
	return camera;
}

static void camera_load(void *data, obs_data_t *settings)
{
	struct streamassistant_camera *camera = data;

	if (!camera || obs_data_get_bool(settings, SETTING_AUDIO_ROUTING_MIGRATED))
		return;

	if (obs_source_get_audio_mixers(camera->source) == 0)
		obs_source_set_audio_mixers(camera->source, 0x3F);

	obs_data_set_bool(settings, SETTING_AUDIO_ROUTING_MIGRATED, true);
}

static void camera_destroy(void *data)
{
	struct streamassistant_camera *camera = data;

	if (!camera)
		return;

	signaling_metadata_stop(camera->metadata);
	camera->metadata = NULL;

	if (camera->browser) {
		obs_source_remove_audio_capture_callback(camera->browser, camera_browser_audio, camera);
		obs_source_remove_active_child(camera->source, camera->browser);
		obs_source_release(camera->browser);
	}

	camera_platform_secure_zero(&camera->config, sizeof(camera->config));
	bfree(camera);
}

static uint32_t camera_get_width(void *data)
{
	const struct streamassistant_camera *camera = data;
	return camera ? camera->width : CAMERA_DEFAULT_WIDTH;
}

static uint32_t camera_get_height(void *data)
{
	const struct streamassistant_camera *camera = data;
	return camera ? camera->height : CAMERA_DEFAULT_HEIGHT;
}

static void camera_video_render(void *data, gs_effect_t *effect)
{
	struct streamassistant_camera *camera = data;
	UNUSED_PARAMETER(effect);

	if (camera->browser)
		obs_source_video_render(camera->browser);
}

static void camera_enum_sources(void *data, obs_source_enum_proc_t callback, void *param)
{
	struct streamassistant_camera *camera = data;
	if (camera->browser)
		callback(camera->source, camera->browser, param);
}

static bool open_pairing_clicked(obs_properties_t *properties, obs_property_t *property, void *data)
{
	UNUSED_PARAMETER(properties);
	UNUSED_PARAMETER(property);
	return open_pairing_page(data);
}

static bool reset_pairing_clicked(obs_properties_t *properties, obs_property_t *property, void *data)
{
	struct streamassistant_camera *camera = data;
	obs_data_t *settings;
	UNUSED_PARAMETER(properties);
	UNUSED_PARAMETER(property);

	if (!camera_platform_confirm_pairing_reset())
		return false;

	if (!camera_config_generate(&camera->config)) {
		obs_log(LOG_ERROR, "Failed to rotate pairing credentials");
		return false;
	}

	settings = obs_source_get_settings(camera->source);
	write_config(settings, &camera->config);
	obs_data_set_bool(settings, SETTING_PAIRING_OPENED, true);
	obs_source_update(camera->source, settings);
	obs_data_release(settings);
	open_pairing_page(camera);
	return true;
}

static obs_properties_t *camera_properties(void *data)
{
	struct streamassistant_camera *camera = data;
	obs_properties_t *properties = obs_properties_create();
	obs_properties_t *connection = obs_properties_create();
	obs_properties_t *pairing = obs_properties_create();
	obs_property_t *status =
		obs_properties_add_text(connection, SETTING_STATUS, obs_module_text("Status"), OBS_TEXT_INFO);

	obs_property_text_set_info_type(status, camera && camera->browser ? OBS_TEXT_INFO_NORMAL : OBS_TEXT_INFO_ERROR);
	obs_property_set_enabled(status, false);
	obs_properties_add_group(properties, "connection_group", obs_module_text("ConnectionGroup"), OBS_GROUP_NORMAL,
				 connection);

	obs_properties_add_text(pairing, SETTING_PAIRING_HELP, "", OBS_TEXT_INFO);
	obs_properties_add_button2(pairing, "open_pairing", obs_module_text("OpenPairingPage"), open_pairing_clicked,
				   camera);
	obs_properties_add_button2(pairing, "reset_pairing", obs_module_text("ResetPairing"), reset_pairing_clicked,
				   camera);
	obs_properties_add_text(pairing, SETTING_AUDIO_HELP, "", OBS_TEXT_INFO);
	obs_properties_add_group(properties, "pairing_group", obs_module_text("PairingGroup"), OBS_GROUP_NORMAL,
				 pairing);
	return properties;
}

static void camera_defaults(obs_data_t *settings)
{
	obs_data_set_default_bool(settings, SETTING_PAIRING_OPENED, false);
	obs_data_set_default_bool(settings, SETTING_AUDIO_ROUTING_MIGRATED, false);
	obs_data_set_default_int(settings, SETTING_OUTPUT_WIDTH, CAMERA_DEFAULT_WIDTH);
	obs_data_set_default_int(settings, SETTING_OUTPUT_HEIGHT, CAMERA_DEFAULT_HEIGHT);
	obs_data_set_default_string(settings, SETTING_STATUS, obs_module_text("StatusReady"));
	obs_data_set_default_string(settings, SETTING_PAIRING_HELP, obs_module_text("PairingHelp"));
	obs_data_set_default_string(settings, SETTING_AUDIO_HELP, obs_module_text("AudioHelp"));
}

#define CAMERA_OUTPUT_FLAGS                                                                                         \
	(OBS_SOURCE_VIDEO | OBS_SOURCE_AUDIO | OBS_SOURCE_CUSTOM_DRAW | OBS_SOURCE_DO_NOT_DUPLICATE)

_Static_assert((CAMERA_OUTPUT_FLAGS & OBS_SOURCE_COMPOSITE) == 0,
	       "Audio-forwarding StreamAssistant Camera source must not be composite");

static struct obs_source_info camera_source_info = {
	.id = CAMERA_SOURCE_ID,
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = CAMERA_OUTPUT_FLAGS,
	.get_name = camera_get_name,
	.create = camera_create,
	.destroy = camera_destroy,
	.load = camera_load,
	.update = camera_update,
	.get_defaults = camera_defaults,
	.get_properties = camera_properties,
	.get_width = camera_get_width,
	.get_height = camera_get_height,
	.video_render = camera_video_render,
	.enum_active_sources = camera_enum_sources,
	.enum_all_sources = camera_enum_sources,
	.icon_type = OBS_ICON_TYPE_CAMERA,
};

bool obs_module_load(void)
{
	obs_register_source(&camera_source_info);
	obs_log(LOG_INFO, "StreamAssistant Camera loaded (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	obs_log(LOG_INFO, "StreamAssistant Camera unloaded");
}
