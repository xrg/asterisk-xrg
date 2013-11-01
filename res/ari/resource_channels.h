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
 * \brief Generated file - declares stubs to be implemented in
 * res/ari/resource_channels.c
 *
 * Channel resources
 *
 * \author David M. Lee, II <dlee@digium.com>
 */

/*
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * !!!!!                               DO NOT EDIT                        !!!!!
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * This file is generated by a mustache template. Please see the original
 * template in rest-api-templates/ari_resource.h.mustache
 */

#ifndef _ASTERISK_RESOURCE_CHANNELS_H
#define _ASTERISK_RESOURCE_CHANNELS_H

#include "asterisk/ari.h"

/*! \brief Argument struct for ast_ari_get_channels() */
struct ast_get_channels_args {
};
/*!
 * \brief List all active channels in Asterisk.
 *
 * \param headers HTTP headers
 * \param args Swagger parameters
 * \param[out] response HTTP response
 */
void ast_ari_get_channels(struct ast_variable *headers, struct ast_get_channels_args *args, struct ast_ari_response *response);
/*! \brief Argument struct for ast_ari_originate() */
struct ast_originate_args {
	/*! \brief Endpoint to call. */
	const char *endpoint;
	/*! \brief The extension to dial after the endpoint answers */
	const char *extension;
	/*! \brief The context to dial after the endpoint answers. If omitted, uses 'default' */
	const char *context;
	/*! \brief The priority to dial after the endpoint answers. If omitted, uses 1 */
	long priority;
	/*! \brief The application that is subscribed to the originated channel, and passed to the Stasis application. */
	const char *app;
	/*! \brief The application arguments to pass to the Stasis application. */
	const char *app_args;
	/*! \brief CallerID to use when dialing the endpoint or extension. */
	const char *caller_id;
	/*! \brief Timeout (in seconds) before giving up dialing, or -1 for no timeout. */
	int timeout;
};
/*!
 * \brief Create a new channel (originate).
 *
 * The new channel is created immediately and a snapshot of it returned. If a Stasis application is provided it will be automatically subscribed to the originated channel for further events and updates.
 *
 * \param headers HTTP headers
 * \param args Swagger parameters
 * \param[out] response HTTP response
 */
void ast_ari_originate(struct ast_variable *headers, struct ast_originate_args *args, struct ast_ari_response *response);
/*! \brief Argument struct for ast_ari_get_channel() */
struct ast_get_channel_args {
	/*! \brief Channel's id */
	const char *channel_id;
};
/*!
 * \brief Channel details.
 *
 * \param headers HTTP headers
 * \param args Swagger parameters
 * \param[out] response HTTP response
 */
void ast_ari_get_channel(struct ast_variable *headers, struct ast_get_channel_args *args, struct ast_ari_response *response);
/*! \brief Argument struct for ast_ari_delete_channel() */
struct ast_delete_channel_args {
	/*! \brief Channel's id */
	const char *channel_id;
	/*! \brief Reason for hanging up the channel */
	const char *reason;
};
/*!
 * \brief Delete (i.e. hangup) a channel.
 *
 * \param headers HTTP headers
 * \param args Swagger parameters
 * \param[out] response HTTP response
 */
void ast_ari_delete_channel(struct ast_variable *headers, struct ast_delete_channel_args *args, struct ast_ari_response *response);
/*! \brief Argument struct for ast_ari_continue_in_dialplan() */
struct ast_continue_in_dialplan_args {
	/*! \brief Channel's id */
	const char *channel_id;
	/*! \brief The context to continue to. */
	const char *context;
	/*! \brief The extension to continue to. */
	const char *extension;
	/*! \brief The priority to continue to. */
	int priority;
};
/*!
 * \brief Exit application; continue execution in the dialplan.
 *
 * \param headers HTTP headers
 * \param args Swagger parameters
 * \param[out] response HTTP response
 */
void ast_ari_continue_in_dialplan(struct ast_variable *headers, struct ast_continue_in_dialplan_args *args, struct ast_ari_response *response);
/*! \brief Argument struct for ast_ari_answer_channel() */
struct ast_answer_channel_args {
	/*! \brief Channel's id */
	const char *channel_id;
};
/*!
 * \brief Answer a channel.
 *
 * \param headers HTTP headers
 * \param args Swagger parameters
 * \param[out] response HTTP response
 */
void ast_ari_answer_channel(struct ast_variable *headers, struct ast_answer_channel_args *args, struct ast_ari_response *response);
/*! \brief Argument struct for ast_ari_ring_channel() */
struct ast_ring_channel_args {
	/*! \brief Channel's id */
	const char *channel_id;
};
/*!
 * \brief Indicate ringing to a channel.
 *
 * \param headers HTTP headers
 * \param args Swagger parameters
 * \param[out] response HTTP response
 */
void ast_ari_ring_channel(struct ast_variable *headers, struct ast_ring_channel_args *args, struct ast_ari_response *response);
/*! \brief Argument struct for ast_ari_send_dtmfchannel() */
struct ast_send_dtmfchannel_args {
	/*! \brief Channel's id */
	const char *channel_id;
	/*! \brief DTMF To send. */
	const char *dtmf;
	/*! \brief Amount of time to wait before DTMF digits (specified in milliseconds) start. */
	int before;
	/*! \brief Amount of time in between DTMF digits (specified in milliseconds). */
	int between;
	/*! \brief Length of each DTMF digit (specified in milliseconds). */
	int duration;
	/*! \brief Amount of time to wait after DTMF digits (specified in milliseconds) end. */
	int after;
};
/*!
 * \brief Send provided DTMF to a given channel.
 *
 * \param headers HTTP headers
 * \param args Swagger parameters
 * \param[out] response HTTP response
 */
void ast_ari_send_dtmfchannel(struct ast_variable *headers, struct ast_send_dtmfchannel_args *args, struct ast_ari_response *response);
/*! \brief Argument struct for ast_ari_mute_channel() */
struct ast_mute_channel_args {
	/*! \brief Channel's id */
	const char *channel_id;
	/*! \brief Direction in which to mute audio */
	const char *direction;
};
/*!
 * \brief Mute a channel.
 *
 * \param headers HTTP headers
 * \param args Swagger parameters
 * \param[out] response HTTP response
 */
void ast_ari_mute_channel(struct ast_variable *headers, struct ast_mute_channel_args *args, struct ast_ari_response *response);
/*! \brief Argument struct for ast_ari_unmute_channel() */
struct ast_unmute_channel_args {
	/*! \brief Channel's id */
	const char *channel_id;
	/*! \brief Direction in which to unmute audio */
	const char *direction;
};
/*!
 * \brief Unmute a channel.
 *
 * \param headers HTTP headers
 * \param args Swagger parameters
 * \param[out] response HTTP response
 */
void ast_ari_unmute_channel(struct ast_variable *headers, struct ast_unmute_channel_args *args, struct ast_ari_response *response);
/*! \brief Argument struct for ast_ari_hold_channel() */
struct ast_hold_channel_args {
	/*! \brief Channel's id */
	const char *channel_id;
};
/*!
 * \brief Hold a channel.
 *
 * \param headers HTTP headers
 * \param args Swagger parameters
 * \param[out] response HTTP response
 */
void ast_ari_hold_channel(struct ast_variable *headers, struct ast_hold_channel_args *args, struct ast_ari_response *response);
/*! \brief Argument struct for ast_ari_unhold_channel() */
struct ast_unhold_channel_args {
	/*! \brief Channel's id */
	const char *channel_id;
};
/*!
 * \brief Remove a channel from hold.
 *
 * \param headers HTTP headers
 * \param args Swagger parameters
 * \param[out] response HTTP response
 */
void ast_ari_unhold_channel(struct ast_variable *headers, struct ast_unhold_channel_args *args, struct ast_ari_response *response);
/*! \brief Argument struct for ast_ari_moh_start_channel() */
struct ast_moh_start_channel_args {
	/*! \brief Channel's id */
	const char *channel_id;
	/*! \brief Music on hold class to use */
	const char *moh_class;
};
/*!
 * \brief Play music on hold to a channel.
 *
 * Using media operations such as playOnChannel on a channel playing MOH in this manner will suspend MOH without resuming automatically. If continuing music on hold is desired, the stasis application must reinitiate music on hold.
 *
 * \param headers HTTP headers
 * \param args Swagger parameters
 * \param[out] response HTTP response
 */
void ast_ari_moh_start_channel(struct ast_variable *headers, struct ast_moh_start_channel_args *args, struct ast_ari_response *response);
/*! \brief Argument struct for ast_ari_moh_stop_channel() */
struct ast_moh_stop_channel_args {
	/*! \brief Channel's id */
	const char *channel_id;
};
/*!
 * \brief Stop playing music on hold to a channel.
 *
 * \param headers HTTP headers
 * \param args Swagger parameters
 * \param[out] response HTTP response
 */
void ast_ari_moh_stop_channel(struct ast_variable *headers, struct ast_moh_stop_channel_args *args, struct ast_ari_response *response);
/*! \brief Argument struct for ast_ari_play_on_channel() */
struct ast_play_on_channel_args {
	/*! \brief Channel's id */
	const char *channel_id;
	/*! \brief Media's URI to play. */
	const char *media;
	/*! \brief For sounds, selects language for sound. */
	const char *lang;
	/*! \brief Number of media to skip before playing. */
	int offsetms;
	/*! \brief Number of milliseconds to skip for forward/reverse operations. */
	int skipms;
};
/*!
 * \brief Start playback of media.
 *
 * The media URI may be any of a number of URI's. Currently sound: and recording: URI's are supported. This operation creates a playback resource that can be used to control the playback of media (pause, rewind, fast forward, etc.)
 *
 * \param headers HTTP headers
 * \param args Swagger parameters
 * \param[out] response HTTP response
 */
void ast_ari_play_on_channel(struct ast_variable *headers, struct ast_play_on_channel_args *args, struct ast_ari_response *response);
/*! \brief Argument struct for ast_ari_record_channel() */
struct ast_record_channel_args {
	/*! \brief Channel's id */
	const char *channel_id;
	/*! \brief Recording's filename */
	const char *name;
	/*! \brief Format to encode audio in */
	const char *format;
	/*! \brief Maximum duration of the recording, in seconds. 0 for no limit */
	int max_duration_seconds;
	/*! \brief Maximum duration of silence, in seconds. 0 for no limit */
	int max_silence_seconds;
	/*! \brief Action to take if a recording with the same name already exists. */
	const char *if_exists;
	/*! \brief Play beep when recording begins */
	int beep;
	/*! \brief DTMF input to terminate recording */
	const char *terminate_on;
};
/*!
 * \brief Start a recording.
 *
 * Record audio from a channel. Note that this will not capture audio sent to the channel. The bridge itself has a record feature if that's what you want.
 *
 * \param headers HTTP headers
 * \param args Swagger parameters
 * \param[out] response HTTP response
 */
void ast_ari_record_channel(struct ast_variable *headers, struct ast_record_channel_args *args, struct ast_ari_response *response);
/*! \brief Argument struct for ast_ari_get_channel_var() */
struct ast_get_channel_var_args {
	/*! \brief Channel's id */
	const char *channel_id;
	/*! \brief The channel variable or function to get */
	const char *variable;
};
/*!
 * \brief Get the value of a channel variable or function.
 *
 * \param headers HTTP headers
 * \param args Swagger parameters
 * \param[out] response HTTP response
 */
void ast_ari_get_channel_var(struct ast_variable *headers, struct ast_get_channel_var_args *args, struct ast_ari_response *response);
/*! \brief Argument struct for ast_ari_set_channel_var() */
struct ast_set_channel_var_args {
	/*! \brief Channel's id */
	const char *channel_id;
	/*! \brief The channel variable or function to set */
	const char *variable;
	/*! \brief The value to set the variable to */
	const char *value;
};
/*!
 * \brief Set the value of a channel variable or function.
 *
 * \param headers HTTP headers
 * \param args Swagger parameters
 * \param[out] response HTTP response
 */
void ast_ari_set_channel_var(struct ast_variable *headers, struct ast_set_channel_var_args *args, struct ast_ari_response *response);

#endif /* _ASTERISK_RESOURCE_CHANNELS_H */
