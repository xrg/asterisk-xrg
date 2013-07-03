/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2013, Digium, Inc.
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
 * \brief Generated file - Build validators for ARI model objects.
 *
 * In addition to the normal validation functions one would normally expect,
 * each validator has a ari_validate_{id}_fn() companion function that returns
 * the validator's function pointer.
 *
 * The reason for this seamingly useless indirection is the way function
 * pointers interfere with module loading. Asterisk attempts to dlopen() each
 * module using \c RTLD_LAZY in order to read some metadata from the module.
 * Unfortunately, if you take the address of a function, the function has to be
 * resolvable at load time, even if \c RTLD_LAZY is specified. By moving the
 * function-address-taking into this module, we can once again be lazy.
 */

 /*
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * !!!!!                               DO NOT EDIT                        !!!!!
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * This file is generated by a mustache template. Please see the original
 * template in rest-api-templates/ari_model_validators.h.mustache
 */

#ifndef _ASTERISK_ARI_MODEL_H
#define _ASTERISK_ARI_MODEL_H

#include "asterisk/json.h"

/*! @{ */

/*!
 * \brief Validator for native Swagger void.
 *
 * \param json JSON object to validate.
 * \returns True (non-zero) if valid.
 * \returns False (zero) if invalid.
 */
int ari_validate_void(struct ast_json *json);

/*!
 * \brief Validator for native Swagger byte.
 *
 * \param json JSON object to validate.
 * \returns True (non-zero) if valid.
 * \returns False (zero) if invalid.
 */
int ari_validate_byte(struct ast_json *json);

/*!
 * \brief Validator for native Swagger boolean.
 *
 * \param json JSON object to validate.
 * \returns True (non-zero) if valid.
 * \returns False (zero) if invalid.
 */
int ari_validate_boolean(struct ast_json *json);

/*!
 * \brief Validator for native Swagger int.
 *
 * \param json JSON object to validate.
 * \returns True (non-zero) if valid.
 * \returns False (zero) if invalid.
 */
int ari_validate_int(struct ast_json *json);

/*!
 * \brief Validator for native Swagger long.
 *
 * \param json JSON object to validate.
 * \returns True (non-zero) if valid.
 * \returns False (zero) if invalid.
 */
int ari_validate_long(struct ast_json *json);

/*!
 * \brief Validator for native Swagger float.
 *
 * \param json JSON object to validate.
 * \returns True (non-zero) if valid.
 * \returns False (zero) if invalid.
 */
int ari_validate_float(struct ast_json *json);

/*!
 * \brief Validator for native Swagger double.
 *
 * \param json JSON object to validate.
 * \returns True (non-zero) if valid.
 * \returns False (zero) if invalid.
 */
int ari_validate_double(struct ast_json *json);

/*!
 * \brief Validator for native Swagger string.
 *
 * \param json JSON object to validate.
 * \returns True (non-zero) if valid.
 * \returns False (zero) if invalid.
 */
int ari_validate_string(struct ast_json *json);

/*!
 * \brief Validator for native Swagger date.
 *
 * \param json JSON object to validate.
 * \returns True (non-zero) if valid.
 * \returns False (zero) if invalid.
 */
int ari_validate_date(struct ast_json *json);

/*!
 * \brief Validator for a Swagger List[]/JSON array.
 *
 * \param json JSON object to validate.
 * \param fn Validator to call on every element in the array.
 * \returns True (non-zero) if valid.
 * \returns False (zero) if invalid.
 */
int ari_validate_list(struct ast_json *json, int (*fn)(struct ast_json *));

/*! @} */

/*!
 * \brief Function type for validator functions. Allows for 
 */
typedef int (*ari_validator)(struct ast_json *json);

/*!
 * \brief Validator for AsteriskInfo.
 *
 * Asterisk system information
 *
 * \param json JSON object to validate.
 * \returns True (non-zero) if valid.
 * \returns False (zero) if invalid.
 */
int ari_validate_asterisk_info(struct ast_json *json);

/*!
 * \brief Function pointer to ari_validate_asterisk_info().
 *
 * See \ref ari_model_validators.h for more details.
 */
ari_validator ari_validate_asterisk_info_fn(void);

/*!
 * \brief Validator for Endpoint.
 *
 * An external device that may offer/accept calls to/from Asterisk.
 *
 * Unlike most resources, which have a single unique identifier, an endpoint is uniquely identified by the technology/resource pair.
 *
 * \param json JSON object to validate.
 * \returns True (non-zero) if valid.
 * \returns False (zero) if invalid.
 */
int ari_validate_endpoint(struct ast_json *json);

/*!
 * \brief Function pointer to ari_validate_endpoint().
 *
 * See \ref ari_model_validators.h for more details.
 */
ari_validator ari_validate_endpoint_fn(void);

/*!
 * \brief Validator for CallerID.
 *
 * Caller identification
 *
 * \param json JSON object to validate.
 * \returns True (non-zero) if valid.
 * \returns False (zero) if invalid.
 */
int ari_validate_caller_id(struct ast_json *json);

/*!
 * \brief Function pointer to ari_validate_caller_id().
 *
 * See \ref ari_model_validators.h for more details.
 */
ari_validator ari_validate_caller_id_fn(void);

/*!
 * \brief Validator for Channel.
 *
 * A specific communication connection between Asterisk and an Endpoint.
 *
 * \param json JSON object to validate.
 * \returns True (non-zero) if valid.
 * \returns False (zero) if invalid.
 */
int ari_validate_channel(struct ast_json *json);

/*!
 * \brief Function pointer to ari_validate_channel().
 *
 * See \ref ari_model_validators.h for more details.
 */
ari_validator ari_validate_channel_fn(void);

/*!
 * \brief Validator for Dialed.
 *
 * Dialed channel information.
 *
 * \param json JSON object to validate.
 * \returns True (non-zero) if valid.
 * \returns False (zero) if invalid.
 */
int ari_validate_dialed(struct ast_json *json);

/*!
 * \brief Function pointer to ari_validate_dialed().
 *
 * See \ref ari_model_validators.h for more details.
 */
ari_validator ari_validate_dialed_fn(void);

/*!
 * \brief Validator for DialplanCEP.
 *
 * Dialplan location (context/extension/priority)
 *
 * \param json JSON object to validate.
 * \returns True (non-zero) if valid.
 * \returns False (zero) if invalid.
 */
int ari_validate_dialplan_cep(struct ast_json *json);

/*!
 * \brief Function pointer to ari_validate_dialplan_cep().
 *
 * See \ref ari_model_validators.h for more details.
 */
ari_validator ari_validate_dialplan_cep_fn(void);

/*!
 * \brief Validator for Bridge.
 *
 * The merging of media from one or more channels.
 *
 * Everyone on the bridge receives the same audio.
 *
 * \param json JSON object to validate.
 * \returns True (non-zero) if valid.
 * \returns False (zero) if invalid.
 */
int ari_validate_bridge(struct ast_json *json);

/*!
 * \brief Function pointer to ari_validate_bridge().
 *
 * See \ref ari_model_validators.h for more details.
 */
ari_validator ari_validate_bridge_fn(void);

/*!
 * \brief Validator for LiveRecording.
 *
 * A recording that is in progress
 *
 * \param json JSON object to validate.
 * \returns True (non-zero) if valid.
 * \returns False (zero) if invalid.
 */
int ari_validate_live_recording(struct ast_json *json);

/*!
 * \brief Function pointer to ari_validate_live_recording().
 *
 * See \ref ari_model_validators.h for more details.
 */
ari_validator ari_validate_live_recording_fn(void);

/*!
 * \brief Validator for StoredRecording.
 *
 * A past recording that may be played back.
 *
 * \param json JSON object to validate.
 * \returns True (non-zero) if valid.
 * \returns False (zero) if invalid.
 */
int ari_validate_stored_recording(struct ast_json *json);

/*!
 * \brief Function pointer to ari_validate_stored_recording().
 *
 * See \ref ari_model_validators.h for more details.
 */
ari_validator ari_validate_stored_recording_fn(void);

/*!
 * \brief Validator for FormatLangPair.
 *
 * Identifies the format and language of a sound file
 *
 * \param json JSON object to validate.
 * \returns True (non-zero) if valid.
 * \returns False (zero) if invalid.
 */
int ari_validate_format_lang_pair(struct ast_json *json);

/*!
 * \brief Function pointer to ari_validate_format_lang_pair().
 *
 * See \ref ari_model_validators.h for more details.
 */
ari_validator ari_validate_format_lang_pair_fn(void);

/*!
 * \brief Validator for Sound.
 *
 * A media file that may be played back.
 *
 * \param json JSON object to validate.
 * \returns True (non-zero) if valid.
 * \returns False (zero) if invalid.
 */
int ari_validate_sound(struct ast_json *json);

/*!
 * \brief Function pointer to ari_validate_sound().
 *
 * See \ref ari_model_validators.h for more details.
 */
ari_validator ari_validate_sound_fn(void);

/*!
 * \brief Validator for Playback.
 *
 * Object representing the playback of media to a channel
 *
 * \param json JSON object to validate.
 * \returns True (non-zero) if valid.
 * \returns False (zero) if invalid.
 */
int ari_validate_playback(struct ast_json *json);

/*!
 * \brief Function pointer to ari_validate_playback().
 *
 * See \ref ari_model_validators.h for more details.
 */
ari_validator ari_validate_playback_fn(void);

/*!
 * \brief Validator for ApplicationReplaced.
 *
 * Notification that another WebSocket has taken over for an application.
 *
 * An application may only be subscribed to by a single WebSocket at a time. If multiple WebSockets attempt to subscribe to the same application, the newer WebSocket wins, and the older one receives this event.
 *
 * \param json JSON object to validate.
 * \returns True (non-zero) if valid.
 * \returns False (zero) if invalid.
 */
int ari_validate_application_replaced(struct ast_json *json);

/*!
 * \brief Function pointer to ari_validate_application_replaced().
 *
 * See \ref ari_model_validators.h for more details.
 */
ari_validator ari_validate_application_replaced_fn(void);

/*!
 * \brief Validator for BridgeCreated.
 *
 * Notification that a bridge has been created.
 *
 * \param json JSON object to validate.
 * \returns True (non-zero) if valid.
 * \returns False (zero) if invalid.
 */
int ari_validate_bridge_created(struct ast_json *json);

/*!
 * \brief Function pointer to ari_validate_bridge_created().
 *
 * See \ref ari_model_validators.h for more details.
 */
ari_validator ari_validate_bridge_created_fn(void);

/*!
 * \brief Validator for BridgeDestroyed.
 *
 * Notification that a bridge has been destroyed.
 *
 * \param json JSON object to validate.
 * \returns True (non-zero) if valid.
 * \returns False (zero) if invalid.
 */
int ari_validate_bridge_destroyed(struct ast_json *json);

/*!
 * \brief Function pointer to ari_validate_bridge_destroyed().
 *
 * See \ref ari_model_validators.h for more details.
 */
ari_validator ari_validate_bridge_destroyed_fn(void);

/*!
 * \brief Validator for BridgeMerged.
 *
 * Notification that one bridge has merged into another.
 *
 * \param json JSON object to validate.
 * \returns True (non-zero) if valid.
 * \returns False (zero) if invalid.
 */
int ari_validate_bridge_merged(struct ast_json *json);

/*!
 * \brief Function pointer to ari_validate_bridge_merged().
 *
 * See \ref ari_model_validators.h for more details.
 */
ari_validator ari_validate_bridge_merged_fn(void);

/*!
 * \brief Validator for ChannelCallerId.
 *
 * Channel changed Caller ID.
 *
 * \param json JSON object to validate.
 * \returns True (non-zero) if valid.
 * \returns False (zero) if invalid.
 */
int ari_validate_channel_caller_id(struct ast_json *json);

/*!
 * \brief Function pointer to ari_validate_channel_caller_id().
 *
 * See \ref ari_model_validators.h for more details.
 */
ari_validator ari_validate_channel_caller_id_fn(void);

/*!
 * \brief Validator for ChannelCreated.
 *
 * Notification that a channel has been created.
 *
 * \param json JSON object to validate.
 * \returns True (non-zero) if valid.
 * \returns False (zero) if invalid.
 */
int ari_validate_channel_created(struct ast_json *json);

/*!
 * \brief Function pointer to ari_validate_channel_created().
 *
 * See \ref ari_model_validators.h for more details.
 */
ari_validator ari_validate_channel_created_fn(void);

/*!
 * \brief Validator for ChannelDestroyed.
 *
 * Notification that a channel has been destroyed.
 *
 * \param json JSON object to validate.
 * \returns True (non-zero) if valid.
 * \returns False (zero) if invalid.
 */
int ari_validate_channel_destroyed(struct ast_json *json);

/*!
 * \brief Function pointer to ari_validate_channel_destroyed().
 *
 * See \ref ari_model_validators.h for more details.
 */
ari_validator ari_validate_channel_destroyed_fn(void);

/*!
 * \brief Validator for ChannelDialplan.
 *
 * Channel changed location in the dialplan.
 *
 * \param json JSON object to validate.
 * \returns True (non-zero) if valid.
 * \returns False (zero) if invalid.
 */
int ari_validate_channel_dialplan(struct ast_json *json);

/*!
 * \brief Function pointer to ari_validate_channel_dialplan().
 *
 * See \ref ari_model_validators.h for more details.
 */
ari_validator ari_validate_channel_dialplan_fn(void);

/*!
 * \brief Validator for ChannelDtmfReceived.
 *
 * DTMF received on a channel.
 *
 * This event is sent when the DTMF ends. There is no notification about the start of DTMF
 *
 * \param json JSON object to validate.
 * \returns True (non-zero) if valid.
 * \returns False (zero) if invalid.
 */
int ari_validate_channel_dtmf_received(struct ast_json *json);

/*!
 * \brief Function pointer to ari_validate_channel_dtmf_received().
 *
 * See \ref ari_model_validators.h for more details.
 */
ari_validator ari_validate_channel_dtmf_received_fn(void);

/*!
 * \brief Validator for ChannelEnteredBridge.
 *
 * Notification that a channel has entered a bridge.
 *
 * \param json JSON object to validate.
 * \returns True (non-zero) if valid.
 * \returns False (zero) if invalid.
 */
int ari_validate_channel_entered_bridge(struct ast_json *json);

/*!
 * \brief Function pointer to ari_validate_channel_entered_bridge().
 *
 * See \ref ari_model_validators.h for more details.
 */
ari_validator ari_validate_channel_entered_bridge_fn(void);

/*!
 * \brief Validator for ChannelHangupRequest.
 *
 * A hangup was requested on the channel.
 *
 * \param json JSON object to validate.
 * \returns True (non-zero) if valid.
 * \returns False (zero) if invalid.
 */
int ari_validate_channel_hangup_request(struct ast_json *json);

/*!
 * \brief Function pointer to ari_validate_channel_hangup_request().
 *
 * See \ref ari_model_validators.h for more details.
 */
ari_validator ari_validate_channel_hangup_request_fn(void);

/*!
 * \brief Validator for ChannelLeftBridge.
 *
 * Notification that a channel has left a bridge.
 *
 * \param json JSON object to validate.
 * \returns True (non-zero) if valid.
 * \returns False (zero) if invalid.
 */
int ari_validate_channel_left_bridge(struct ast_json *json);

/*!
 * \brief Function pointer to ari_validate_channel_left_bridge().
 *
 * See \ref ari_model_validators.h for more details.
 */
ari_validator ari_validate_channel_left_bridge_fn(void);

/*!
 * \brief Validator for ChannelStateChange.
 *
 * Notification of a channel's state change.
 *
 * \param json JSON object to validate.
 * \returns True (non-zero) if valid.
 * \returns False (zero) if invalid.
 */
int ari_validate_channel_state_change(struct ast_json *json);

/*!
 * \brief Function pointer to ari_validate_channel_state_change().
 *
 * See \ref ari_model_validators.h for more details.
 */
ari_validator ari_validate_channel_state_change_fn(void);

/*!
 * \brief Validator for ChannelUserevent.
 *
 * User-generated event with additional user-defined fields in the object.
 *
 * \param json JSON object to validate.
 * \returns True (non-zero) if valid.
 * \returns False (zero) if invalid.
 */
int ari_validate_channel_userevent(struct ast_json *json);

/*!
 * \brief Function pointer to ari_validate_channel_userevent().
 *
 * See \ref ari_model_validators.h for more details.
 */
ari_validator ari_validate_channel_userevent_fn(void);

/*!
 * \brief Validator for ChannelVarset.
 *
 * Channel variable changed.
 *
 * \param json JSON object to validate.
 * \returns True (non-zero) if valid.
 * \returns False (zero) if invalid.
 */
int ari_validate_channel_varset(struct ast_json *json);

/*!
 * \brief Function pointer to ari_validate_channel_varset().
 *
 * See \ref ari_model_validators.h for more details.
 */
ari_validator ari_validate_channel_varset_fn(void);

/*!
 * \brief Validator for Event.
 *
 * Base type for asynchronous events from Asterisk.
 *
 * \param json JSON object to validate.
 * \returns True (non-zero) if valid.
 * \returns False (zero) if invalid.
 */
int ari_validate_event(struct ast_json *json);

/*!
 * \brief Function pointer to ari_validate_event().
 *
 * See \ref ari_model_validators.h for more details.
 */
ari_validator ari_validate_event_fn(void);

/*!
 * \brief Validator for PlaybackFinished.
 *
 * Event showing the completion of a media playback operation.
 *
 * \param json JSON object to validate.
 * \returns True (non-zero) if valid.
 * \returns False (zero) if invalid.
 */
int ari_validate_playback_finished(struct ast_json *json);

/*!
 * \brief Function pointer to ari_validate_playback_finished().
 *
 * See \ref ari_model_validators.h for more details.
 */
ari_validator ari_validate_playback_finished_fn(void);

/*!
 * \brief Validator for PlaybackStarted.
 *
 * Event showing the start of a media playback operation.
 *
 * \param json JSON object to validate.
 * \returns True (non-zero) if valid.
 * \returns False (zero) if invalid.
 */
int ari_validate_playback_started(struct ast_json *json);

/*!
 * \brief Function pointer to ari_validate_playback_started().
 *
 * See \ref ari_model_validators.h for more details.
 */
ari_validator ari_validate_playback_started_fn(void);

/*!
 * \brief Validator for StasisEnd.
 *
 * Notification that a channel has left a Stasis appliction.
 *
 * \param json JSON object to validate.
 * \returns True (non-zero) if valid.
 * \returns False (zero) if invalid.
 */
int ari_validate_stasis_end(struct ast_json *json);

/*!
 * \brief Function pointer to ari_validate_stasis_end().
 *
 * See \ref ari_model_validators.h for more details.
 */
ari_validator ari_validate_stasis_end_fn(void);

/*!
 * \brief Validator for StasisStart.
 *
 * Notification that a channel has entered a Stasis appliction.
 *
 * \param json JSON object to validate.
 * \returns True (non-zero) if valid.
 * \returns False (zero) if invalid.
 */
int ari_validate_stasis_start(struct ast_json *json);

/*!
 * \brief Function pointer to ari_validate_stasis_start().
 *
 * See \ref ari_model_validators.h for more details.
 */
ari_validator ari_validate_stasis_start_fn(void);

/*
 * JSON models
 *
 * AsteriskInfo
 * Endpoint
 * - channel_ids: List[string] (required)
 * - resource: string (required)
 * - state: string
 * - technology: string (required)
 * CallerID
 * - name: string (required)
 * - number: string (required)
 * Channel
 * - accountcode: string (required)
 * - caller: CallerID (required)
 * - connected: CallerID (required)
 * - creationtime: Date (required)
 * - dialplan: DialplanCEP (required)
 * - id: string (required)
 * - name: string (required)
 * - state: string (required)
 * Dialed
 * DialplanCEP
 * - context: string (required)
 * - exten: string (required)
 * - priority: long (required)
 * Bridge
 * - bridge_class: string (required)
 * - bridge_type: string (required)
 * - channels: List[string] (required)
 * - id: string (required)
 * - technology: string (required)
 * LiveRecording
 * - id: string (required)
 * StoredRecording
 * - duration_seconds: int
 * - formats: List[string] (required)
 * - id: string (required)
 * - time: Date
 * FormatLangPair
 * - format: string (required)
 * - language: string (required)
 * Sound
 * - formats: List[FormatLangPair] (required)
 * - id: string (required)
 * - text: string
 * Playback
 * - id: string (required)
 * - language: string
 * - media_uri: string (required)
 * - state: string (required)
 * - target_uri: string (required)
 * ApplicationReplaced
 * - application: string (required)
 * - timestamp: Date
 * - type: string (required)
 * BridgeCreated
 * - application: string (required)
 * - timestamp: Date
 * - type: string (required)
 * - bridge: Bridge (required)
 * BridgeDestroyed
 * - application: string (required)
 * - timestamp: Date
 * - type: string (required)
 * - bridge: Bridge (required)
 * BridgeMerged
 * - application: string (required)
 * - timestamp: Date
 * - type: string (required)
 * - bridge: Bridge (required)
 * - bridge_from: Bridge (required)
 * ChannelCallerId
 * - application: string (required)
 * - timestamp: Date
 * - type: string (required)
 * - caller_presentation: int (required)
 * - caller_presentation_txt: string (required)
 * - channel: Channel (required)
 * ChannelCreated
 * - application: string (required)
 * - timestamp: Date
 * - type: string (required)
 * - channel: Channel (required)
 * ChannelDestroyed
 * - application: string (required)
 * - timestamp: Date
 * - type: string (required)
 * - cause: int (required)
 * - cause_txt: string (required)
 * - channel: Channel (required)
 * ChannelDialplan
 * - application: string (required)
 * - timestamp: Date
 * - type: string (required)
 * - channel: Channel (required)
 * - dialplan_app: string (required)
 * - dialplan_app_data: string (required)
 * ChannelDtmfReceived
 * - application: string (required)
 * - timestamp: Date
 * - type: string (required)
 * - channel: Channel (required)
 * - digit: string (required)
 * - duration_ms: int (required)
 * ChannelEnteredBridge
 * - application: string (required)
 * - timestamp: Date
 * - type: string (required)
 * - bridge: Bridge (required)
 * - channel: Channel
 * ChannelHangupRequest
 * - application: string (required)
 * - timestamp: Date
 * - type: string (required)
 * - cause: int
 * - channel: Channel (required)
 * - soft: boolean
 * ChannelLeftBridge
 * - application: string (required)
 * - timestamp: Date
 * - type: string (required)
 * - bridge: Bridge (required)
 * - channel: Channel (required)
 * ChannelStateChange
 * - application: string (required)
 * - timestamp: Date
 * - type: string (required)
 * - channel: Channel (required)
 * ChannelUserevent
 * - application: string (required)
 * - timestamp: Date
 * - type: string (required)
 * - channel: Channel (required)
 * - eventname: string (required)
 * ChannelVarset
 * - application: string (required)
 * - timestamp: Date
 * - type: string (required)
 * - channel: Channel
 * - value: string (required)
 * - variable: string (required)
 * Event
 * - application: string (required)
 * - timestamp: Date
 * - type: string (required)
 * PlaybackFinished
 * - application: string (required)
 * - timestamp: Date
 * - type: string (required)
 * - playback: Playback (required)
 * PlaybackStarted
 * - application: string (required)
 * - timestamp: Date
 * - type: string (required)
 * - playback: Playback (required)
 * StasisEnd
 * - application: string (required)
 * - timestamp: Date
 * - type: string (required)
 * - channel: Channel (required)
 * StasisStart
 * - application: string (required)
 * - timestamp: Date
 * - type: string (required)
 * - args: List[string] (required)
 * - channel: Channel (required)
 */

#endif /* _ASTERISK_ARI_MODEL_H */
