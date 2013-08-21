/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2012 - 2013, Digium, Inc.
 *
 * David M. Lee, II <dlee@digium.com>
 *
 * See http://www.asterisk.org for more information about
 * the Asterisk project. Please do not directly contact
 * any of the maintainers of this project for assistance;
 * the project provides a web site, mailing lists and IRC
 * channels for your use.
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 */

/*! \file
 *
 * \brief Implementation for ARI stubs.
 *
 * \author David M. Lee, II <dlee@digium.com>
 */

/*** MODULEINFO
	<depend type="module">res_stasis_app_playback</depend>
	<support_level>core</support_level>
 ***/

#include "asterisk.h"

ASTERISK_FILE_VERSION(__FILE__, "$Revision$")

#include "asterisk/file.h"
#include "asterisk/pbx.h"
#include "asterisk/dial.h"
#include "asterisk/bridge.h"
#include "asterisk/callerid.h"
#include "asterisk/stasis_app.h"
#include "asterisk/stasis_app_playback.h"
#include "asterisk/stasis_app_recording.h"
#include "asterisk/stasis_channels.h"
#include "resource_channels.h"

#include <limits.h>

/*!
 * \brief Finds the control object for a channel, filling the response with an
 * error, if appropriate.
 * \param[out] response Response to fill with an error if control is not found.
 * \param channel_id ID of the channel to lookup.
 * \return Channel control object.
 * \return \c NULL if control object does not exist.
 */
static struct stasis_app_control *find_control(
	struct ast_ari_response *response,
	const char *channel_id)
{
	RAII_VAR(struct stasis_app_control *, control, NULL, ao2_cleanup);

	ast_assert(response != NULL);

	control = stasis_app_control_find_by_channel_id(channel_id);
	if (control == NULL) {
		/* Distinguish between 404 and 409 errors */
		RAII_VAR(struct ast_channel *, chan, NULL, ao2_cleanup);
		chan = ast_channel_get_by_name(channel_id);
		if (chan == NULL) {
			ast_ari_response_error(response, 404, "Not Found",
				   "Channel not found");
			return NULL;
		}

		ast_ari_response_error(response, 409, "Conflict",
			   "Channel not in Stasis application");
		return NULL;
	}

	ao2_ref(control, +1);
	return control;
}

void ast_ari_dial(struct ast_variable *headers, struct ast_dial_args *args, struct ast_ari_response *response)
{
	struct stasis_app_control *control;

	control = find_control(response, args->channel_id);
	if (control == NULL) {
		return;
	}

	if (stasis_app_control_dial(control, args->endpoint, args->timeout)) {
		ast_ari_response_alloc_failed(response);
		return;
	}

	ast_ari_response_no_content(response);
}

void ast_ari_continue_in_dialplan(
	struct ast_variable *headers,
	struct ast_continue_in_dialplan_args *args,
	struct ast_ari_response *response)
{
	RAII_VAR(struct stasis_app_control *, control, NULL, ao2_cleanup);

	ast_assert(response != NULL);

	control = find_control(response, args->channel_id);
	if (control == NULL) {
		return;
	}

	if (stasis_app_control_continue(control, args->context, args->extension, args->priority)) {
		ast_ari_response_alloc_failed(response);
		return;
	}

	ast_ari_response_no_content(response);
}

void ast_ari_answer_channel(struct ast_variable *headers,
				struct ast_answer_channel_args *args,
				struct ast_ari_response *response)
{
	RAII_VAR(struct stasis_app_control *, control, NULL, ao2_cleanup);

	control = find_control(response, args->channel_id);
	if (control == NULL) {
		return;
	}

	if (stasis_app_control_answer(control) != 0) {
		ast_ari_response_error(
			response, 500, "Internal Server Error",
			"Failed to answer channel");
		return;
	}

	ast_ari_response_no_content(response);
}

void ast_ari_mute_channel(struct ast_variable *headers, struct ast_mute_channel_args *args, struct ast_ari_response *response)
{
	RAII_VAR(struct stasis_app_control *, control, NULL, ao2_cleanup);
	unsigned int direction = 0;
	enum ast_frame_type frametype = AST_FRAME_VOICE;

	control = find_control(response, args->channel_id);
	if (control == NULL) {
		return;
	}

	if (!strcmp(args->direction, "in")) {
		direction = AST_MUTE_DIRECTION_READ;
	} else if (!strcmp(args->direction, "out")) {
		direction = AST_MUTE_DIRECTION_WRITE;
	} else if (!strcmp(args->direction, "both")) {
		direction = AST_MUTE_DIRECTION_READ | AST_MUTE_DIRECTION_WRITE;
	} else {
		ast_ari_response_error(
			response, 400, "Bad Request",
			"Invalid direction specified");
		return;
	}

	stasis_app_control_mute(control, direction, frametype);

	ast_ari_response_no_content(response);
}

void ast_ari_unmute_channel(struct ast_variable *headers, struct ast_unmute_channel_args *args, struct ast_ari_response *response)
{
	RAII_VAR(struct stasis_app_control *, control, NULL, ao2_cleanup);
	unsigned int direction = 0;
	enum ast_frame_type frametype = AST_FRAME_VOICE;

	control = find_control(response, args->channel_id);
	if (control == NULL) {
		return;
	}

	if (!strcmp(args->direction, "in")) {
		direction = AST_MUTE_DIRECTION_READ;
	} else if (!strcmp(args->direction, "out")) {
		direction = AST_MUTE_DIRECTION_WRITE;
	} else if (!strcmp(args->direction, "both")) {
		direction = AST_MUTE_DIRECTION_READ | AST_MUTE_DIRECTION_WRITE;
	} else {
		ast_ari_response_error(
			response, 400, "Bad Request",
			"Invalid direction specified");
		return;
	}

	stasis_app_control_unmute(control, direction, frametype);

	ast_ari_response_no_content(response);
}

void ast_ari_hold_channel(struct ast_variable *headers, struct ast_hold_channel_args *args, struct ast_ari_response *response)
{
	RAII_VAR(struct stasis_app_control *, control, NULL, ao2_cleanup);

	control = find_control(response, args->channel_id);
	if (control == NULL) {
		/* Response filled in by find_control */
		return;
	}

	stasis_app_control_hold(control);

	ast_ari_response_no_content(response);
}

void ast_ari_unhold_channel(struct ast_variable *headers, struct ast_unhold_channel_args *args, struct ast_ari_response *response)
{
	RAII_VAR(struct stasis_app_control *, control, NULL, ao2_cleanup);

	control = find_control(response, args->channel_id);
	if (control == NULL) {
		/* Response filled in by find_control */
		return;
	}

	stasis_app_control_unhold(control);

	ast_ari_response_no_content(response);
}

void ast_ari_moh_start_channel(struct ast_variable *headers, struct ast_moh_start_channel_args *args, struct ast_ari_response *response)
{
	RAII_VAR(struct stasis_app_control *, control, NULL, ao2_cleanup);

	control = find_control(response, args->channel_id);
	if (control == NULL) {
		/* Response filled in by find_control */
		return;
	}

	stasis_app_control_moh_start(control, args->moh_class);
	ast_ari_response_no_content(response);
}

void ast_ari_moh_stop_channel(struct ast_variable *headers, struct ast_moh_stop_channel_args *args, struct ast_ari_response *response)
{
	RAII_VAR(struct stasis_app_control *, control, NULL, ao2_cleanup);

	control = find_control(response, args->channel_id);
	if (control == NULL) {
		/* Response filled in by find_control */
		return;
	}

	stasis_app_control_moh_stop(control);
	ast_ari_response_no_content(response);
}

void ast_ari_play_on_channel(struct ast_variable *headers,
	struct ast_play_on_channel_args *args,
	struct ast_ari_response *response)
{
	RAII_VAR(struct stasis_app_control *, control, NULL, ao2_cleanup);
	RAII_VAR(struct ast_channel_snapshot *, snapshot, NULL, ao2_cleanup);
	RAII_VAR(struct stasis_app_playback *, playback, NULL, ao2_cleanup);
	RAII_VAR(char *, playback_url, NULL, ast_free);
	RAII_VAR(struct ast_json *, json, NULL, ast_json_unref);
	const char *language;

	ast_assert(response != NULL);

	control = find_control(response, args->channel_id);
	if (control == NULL) {
		/* Response filled in by find_control */
		return;
	}

	snapshot = stasis_app_control_get_snapshot(control);
	if (!snapshot) {
		ast_ari_response_error(
			response, 404, "Not Found",
			"Channel not found");
		return;
	}

	if (args->skipms < 0) {
		ast_ari_response_error(
			response, 400, "Bad Request",
			"skipms cannot be negative");
		return;
	}

	if (args->offsetms < 0) {
		ast_ari_response_error(
			response, 400, "Bad Request",
			"offsetms cannot be negative");
		return;
	}

	language = S_OR(args->lang, snapshot->language);

	playback = stasis_app_control_play_uri(control, args->media, language,
		args->channel_id, STASIS_PLAYBACK_TARGET_CHANNEL, args->skipms, args->offsetms);
	if (!playback) {
		ast_ari_response_error(
			response, 500, "Internal Server Error",
			"Failed to queue media for playback");
		return;
	}

	ast_asprintf(&playback_url, "/playback/%s",
		stasis_app_playback_get_id(playback));
	if (!playback_url) {
		ast_ari_response_error(
			response, 500, "Internal Server Error",
			"Out of memory");
		return;
	}

	json = stasis_app_playback_to_json(playback);
	if (!json) {
		ast_ari_response_error(
			response, 500, "Internal Server Error",
			"Out of memory");
		return;
	}

	ast_ari_response_created(response, playback_url, json);
}

void ast_ari_record_channel(struct ast_variable *headers,
	struct ast_record_channel_args *args,
	struct ast_ari_response *response)
{
	RAII_VAR(struct stasis_app_control *, control, NULL, ao2_cleanup);
	RAII_VAR(struct ast_channel_snapshot *, snapshot, NULL, ao2_cleanup);
	RAII_VAR(struct stasis_app_recording *, recording, NULL, ao2_cleanup);
	RAII_VAR(char *, recording_url, NULL, ast_free);
	RAII_VAR(struct ast_json *, json, NULL, ast_json_unref);
	RAII_VAR(struct stasis_app_recording_options *, options, NULL,
		ao2_cleanup);
	RAII_VAR(char *, uri_encoded_name, NULL, ast_free);
	size_t uri_name_maxlen;

	ast_assert(response != NULL);

	if (args->max_duration_seconds < 0) {
		ast_ari_response_error(
			response, 400, "Bad Request",
			"max_duration_seconds cannot be negative");
		return;
	}

	if (args->max_silence_seconds < 0) {
		ast_ari_response_error(
			response, 400, "Bad Request",
			"max_silence_seconds cannot be negative");
		return;
	}

	control = find_control(response, args->channel_id);
	if (control == NULL) {
		/* Response filled in by find_control */
		return;
	}

	options = stasis_app_recording_options_create(args->name, args->format);
	if (options == NULL) {
		ast_ari_response_error(
			response, 500, "Internal Server Error",
			"Out of memory");
	}
	options->max_silence_seconds = args->max_silence_seconds;
	options->max_duration_seconds = args->max_duration_seconds;
	options->terminate_on =
		stasis_app_recording_termination_parse(args->terminate_on);
	options->if_exists =
		stasis_app_recording_if_exists_parse(args->if_exists);
	options->beep = args->beep;

	if (options->terminate_on == STASIS_APP_RECORDING_TERMINATE_INVALID) {
		ast_ari_response_error(
			response, 400, "Bad Request",
			"terminateOn invalid");
		return;
	}

	if (options->if_exists == -1) {
		ast_ari_response_error(
			response, 400, "Bad Request",
			"ifExists invalid");
		return;
	}

	recording = stasis_app_control_record(control, options);
	if (recording == NULL) {
		switch(errno) {
		case EINVAL:
			/* While the arguments are invalid, we should have
			 * caught them prior to calling record.
			 */
			ast_ari_response_error(
				response, 500, "Internal Server Error",
				"Error parsing request");
			break;
		case EEXIST:
			ast_ari_response_error(response, 409, "Conflict",
				"Recording '%s' already in progress",
				args->name);
			break;
		case ENOMEM:
			ast_ari_response_error(
				response, 500, "Internal Server Error",
				"Out of memory");
			break;
		case EPERM:
			ast_ari_response_error(
				response, 400, "Bad Request",
				"Recording name invalid");
			break;
		default:
			ast_log(LOG_WARNING,
				"Unrecognized recording error: %s\n",
				strerror(errno));
			ast_ari_response_error(
				response, 500, "Internal Server Error",
				"Internal Server Error");
			break;
		}
		return;
	}

	uri_name_maxlen = strlen(args->name) * 3;
	uri_encoded_name = ast_malloc(uri_name_maxlen);
	if (!uri_encoded_name) {
		ast_ari_response_error(
			response, 500, "Internal Server Error",
			"Out of memory");
		return;
	}
	ast_uri_encode(args->name, uri_encoded_name, uri_name_maxlen,
		ast_uri_http);

	ast_asprintf(&recording_url, "/recordings/live/%s", uri_encoded_name);
	if (!recording_url) {
		ast_ari_response_error(
			response, 500, "Internal Server Error",
			"Out of memory");
		return;
	}

	json = stasis_app_recording_to_json(recording);
	if (!json) {
		ast_ari_response_error(
			response, 500, "Internal Server Error",
			"Out of memory");
		return;
	}

	ast_ari_response_created(response, recording_url, json);
}

void ast_ari_get_channel(struct ast_variable *headers,
			     struct ast_get_channel_args *args,
			     struct ast_ari_response *response)
{
	RAII_VAR(struct stasis_message *, msg, NULL, ao2_cleanup);
	struct stasis_cache *cache;
	struct ast_channel_snapshot *snapshot;

	cache = ast_channel_cache();
	if (!cache) {
		ast_ari_response_error(
			response, 500, "Internal Server Error",
			"Message bus not initialized");
		return;
	}

	msg = stasis_cache_get(cache, ast_channel_snapshot_type(),
			       args->channel_id);
	if (!msg) {
		ast_ari_response_error(
			response, 404, "Not Found",
			"Channel not found");
		return;
	}

	snapshot = stasis_message_data(msg);
	ast_assert(snapshot != NULL);

	ast_ari_response_ok(response,
				ast_channel_snapshot_to_json(snapshot));
}

void ast_ari_delete_channel(struct ast_variable *headers,
				struct ast_delete_channel_args *args,
				struct ast_ari_response *response)
{
	RAII_VAR(struct ast_channel *, chan, NULL, ao2_cleanup);

	chan = ast_channel_get_by_name(args->channel_id);
	if (chan == NULL) {
		ast_ari_response_error(
			response, 404, "Not Found",
			"Channel not found");
		return;
	}

	ast_softhangup(chan, AST_SOFTHANGUP_EXPLICIT);

	ast_ari_response_no_content(response);
}

void ast_ari_get_channels(struct ast_variable *headers,
			      struct ast_get_channels_args *args,
			      struct ast_ari_response *response)
{
	RAII_VAR(struct stasis_cache *, cache, NULL, ao2_cleanup);
	RAII_VAR(struct ao2_container *, snapshots, NULL, ao2_cleanup);
	RAII_VAR(struct ast_json *, json, NULL, ast_json_unref);
	struct ao2_iterator i;
	void *obj;

	cache = ast_channel_cache();
	if (!cache) {
		ast_ari_response_error(
			response, 500, "Internal Server Error",
			"Message bus not initialized");
		return;
	}
	ao2_ref(cache, +1);

	snapshots = stasis_cache_dump(cache, ast_channel_snapshot_type());
	if (!snapshots) {
		ast_ari_response_alloc_failed(response);
		return;
	}

	json = ast_json_array_create();
	if (!json) {
		ast_ari_response_alloc_failed(response);
		return;
	}

	i = ao2_iterator_init(snapshots, 0);
	while ((obj = ao2_iterator_next(&i))) {
		RAII_VAR(struct stasis_message *, msg, obj, ao2_cleanup);
		struct ast_channel_snapshot *snapshot = stasis_message_data(msg);
		int r = ast_json_array_append(
			json, ast_channel_snapshot_to_json(snapshot));
		if (r != 0) {
			ast_ari_response_alloc_failed(response);
			return;
		}
	}
	ao2_iterator_destroy(&i);

	ast_ari_response_ok(response, ast_json_ref(json));
}

void ast_ari_originate(struct ast_variable *headers,
			   struct ast_originate_args *args,
			   struct ast_ari_response *response)
{
	char *dialtech;
	char dialdevice[AST_CHANNEL_NAME];
	char *caller_id = NULL;
	char *cid_num = NULL;
	char *cid_name = NULL;
	int timeout = 30000;

	char *stuff;

	if (ast_strlen_zero(args->endpoint)) {
		ast_ari_response_error(response, 400, "Bad Request",
			"Endpoint must be specified");
		return;
	}

	dialtech = ast_strdupa(args->endpoint);
	if ((stuff = strchr(dialtech, '/'))) {
		*stuff++ = '\0';
		ast_copy_string(dialdevice, stuff, sizeof(dialdevice));
	}

	if (ast_strlen_zero(dialtech) || ast_strlen_zero(dialdevice)) {
		ast_ari_response_error(response, 400, "Bad Request",
			"Invalid endpoint specified");
		return;
	}

	if (args->timeout > 0) {
		timeout = args->timeout * 1000;
	} else if (args->timeout == -1) {
		timeout = -1;
	}

	if (!ast_strlen_zero(args->caller_id)) {
		caller_id = ast_strdupa(args->caller_id);
		ast_callerid_parse(caller_id, &cid_name, &cid_num);

		if (ast_is_shrinkable_phonenumber(cid_num)) {
			ast_shrink_phone_number(cid_num);
		}
	}

	if (!ast_strlen_zero(args->app)) {
		const char *app = "Stasis";

		RAII_VAR(struct ast_str *, appdata, ast_str_create(64), ast_free);

		if (!appdata) {
			ast_ari_response_alloc_failed(response);
			return;
		}

		ast_str_set(&appdata, 0, "%s", args->app);
		if (!ast_strlen_zero(args->app_args)) {
			ast_str_append(&appdata, 0, ",%s", args->app_args);
		}

		/* originate a channel, putting it into an application */
		if (ast_pbx_outgoing_app(dialtech, NULL, dialdevice, timeout, app, ast_str_buffer(appdata), NULL, 0, cid_num, cid_name, NULL, NULL, NULL)) {
			ast_ari_response_alloc_failed(response);
			return;
		}
	} else if (!ast_strlen_zero(args->extension)) {
		/* originate a channel, sending it to an extension */
		if (ast_pbx_outgoing_exten(dialtech, NULL, dialdevice, timeout, S_OR(args->context, "default"), args->extension, args->priority ? args->priority : 1, NULL, 0, cid_num, cid_name, NULL, NULL, NULL, 0)) {
			ast_ari_response_alloc_failed(response);
			return;
		}
	} else {
		ast_ari_response_error(response, 400, "Bad Request",
			"Application or extension must be specified");
		return;
	}

	ast_ari_response_no_content(response);
}

void ast_ari_get_channel_var(struct ast_variable *headers, struct ast_get_channel_var_args *args, struct ast_ari_response *response)
{
	RAII_VAR(struct ast_json *, json, NULL, ast_json_unref);
	RAII_VAR(struct stasis_app_control *, control, NULL, ao2_cleanup);
	RAII_VAR(char *, value, NULL, ast_free);

	ast_assert(response != NULL);

	if (ast_strlen_zero(args->variable)) {
		ast_ari_response_error(
			response, 400, "Bad Request",
			"Variable name is required");
		return;
	}

	control = find_control(response, args->channel_id);
	if (control == NULL) {
		/* response filled in by find_control */
		return;
	}

	value = stasis_app_control_get_channel_var(control, args->variable);

	if (!(json = ast_json_pack("{s: s}", "value", S_OR(value, "")))) {
		ast_ari_response_alloc_failed(response);
		return;
	}

	ast_ari_response_ok(response, ast_json_ref(json));
}

void ast_ari_set_channel_var(struct ast_variable *headers, struct ast_set_channel_var_args *args, struct ast_ari_response *response)
{
	RAII_VAR(struct stasis_app_control *, control, NULL, ao2_cleanup);

	ast_assert(response != NULL);

	if (ast_strlen_zero(args->variable)) {
		ast_ari_response_error(
			response, 400, "Bad Request",
			"Variable name is required");
		return;
	}

	control = find_control(response, args->channel_id);
	if (control == NULL) {
		/* response filled in by find_control */
		return;
	}

	if (stasis_app_control_set_channel_var(control, args->variable, args->value)) {
		ast_ari_response_error(
			response, 400, "Bad Request",
			"Failed to execute function");
		return;
	}

	ast_ari_response_no_content(response);
}

