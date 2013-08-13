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

#ifndef _ASTERISK_STASIS_APP_H
#define _ASTERISK_STASIS_APP_H

/*! \file
 *
 * \brief Stasis Application API. See \ref res_stasis "Stasis Application API"
 * for detailed documentation.
 *
 * \author David M. Lee, II <dlee@digium.com>
 * \since 12
 *
 * \page res_stasis Stasis Application API
 *
 * This is the API that binds the Stasis dialplan application to external
 * Stasis applications, such as \c res_stasis_websocket.
 *
 * The associated \c res_stasis module registers a dialplan function named \c
 * Stasis, which uses \c res_stasis to put a channel into the named Stasis
 * app. As a channel enters and leaves the Stasis diaplan application, the
 * Stasis app receives a \c 'stasis-start' and \c 'stasis-end' events.
 *
 * Stasis apps register themselves using the \ref stasis_app_register and
 * stasis_app_unregister functions. Messages are sent to an appliction using
 * \ref stasis_app_send.
 *
 * Finally, Stasis apps control channels through the use of the \ref
 * stasis_app_control object, and the family of \c stasis_app_control_*
 * functions.
 *
 * Since module unload order is based on reference counting, any module that
 * uses the API defined in this file must call stasis_app_ref() when loaded,
 * and stasis_app_unref() when unloaded.
 */

#include "asterisk/channel.h"
#include "asterisk/json.h"

/*! @{ */

/*!
 * \brief Callback for Stasis application handler.
 *
 * The message given to the handler is a borrowed copy. If you want to keep a
 * reference to it, you should use \c ao2_ref() to keep it around.
 *
 * \param data Data ptr given when registered.
 * \param app_name Name of the application being dispatched to.
 * \param message Message to handle. (borrowed copy)
 */
typedef void (*stasis_app_cb)(void *data, const char *app_name,
	struct ast_json *message);

/*!
 * \brief Register a new Stasis application.
 *
 * If an application is already registered with the given name, the old
 * application is sent a 'replaced' message and unregistered.
 *
 * \param app_name Name of this application.
 * \param handler Callback for application messages.
 * \param data Data blob to pass to the callback. Must be AO2 managed.
 * \return 0 for success
 * \return -1 for error.
 */
int stasis_app_register(const char *app_name, stasis_app_cb handler, void *data);

/*!
 * \brief Unregister a Stasis application.
 * \param app_name Name of the application to unregister.
 */
void stasis_app_unregister(const char *app_name);

/*!
 * \brief Send a message to the given Stasis application.
 *
 * The message given to the handler is a borrowed copy. If you want to keep a
 * reference to it, you should use \c ao2_ref() to keep it around.
 *
 * \param app_name Name of the application to invoke.
 * \param message Message to send (borrowed reference)
 * \return 0 for success.
 * \return -1 for error.
 */
int stasis_app_send(const char *app_name, struct ast_json *message);

/*! @} */

/*! @{ */

/*! \brief Handler for controlling a channel that's in a Stasis application */
struct stasis_app_control;

/*!
 * \brief Returns the handler for the given channel.
 * \param chan Channel to handle.
 * \return NULL channel not in Stasis application.
 * \return Pointer to \c res_stasis handler.
 */
struct stasis_app_control *stasis_app_control_find_by_channel(
	const struct ast_channel *chan);

/*!
 * \brief Returns the handler for the channel with the given id.
 * \param channel_id Uniqueid of the channel.
 * \return NULL channel not in Stasis application, or channel does not exist.
 * \return Pointer to \c res_stasis handler.
 */
struct stasis_app_control *stasis_app_control_find_by_channel_id(
	const char *channel_id);

/*!
 * \brief Creates a control handler for a channel that isn't in a stasis app.
 * \since 12.0.0
 *
 * \param chan Channel to create controller handle for
 *
 * \return NULL on failure to create the handle
 * \return Pointer to \c res_stasis handler.
 */
struct stasis_app_control *stasis_app_control_create(
	struct ast_channel *chan);

/*!
 * \brief Act on a stasis app control queue until it is empty
 * \since 12.0.0
 *
 * \param chan Channel to handle
 * \param control Control object to execute
 */
void stasis_app_control_execute_until_exhausted(
	struct ast_channel *chan,
	struct stasis_app_control *control);

/*!
 * \brief Returns the uniqueid of the channel associated with this control
 *
 * \param control Control object.
 * \return Uniqueid of the associate channel.
 * \return \c NULL if \a control is \c NULL.
 */
const char *stasis_app_control_get_channel_id(
	const struct stasis_app_control *control);

/*!
 * \brief Dial an endpoint and bridge it to a channel in \c res_stasis
 *
 * If the channel is no longer in \c res_stasis, this function does nothing.
 *
 * \param control Control for \c res_stasis
 * \param endpoint The endpoint to dial.
 * \param timeout The amount of time to wait for answer, before giving up.
 *
 * \return 0 for success
 * \return -1 for error.
 */
int stasis_app_control_dial(struct stasis_app_control *control, const char *endpoint, int timeout);

/*!
 * \brief Apply a bridge role to a channel controlled by a stasis app control
 *
 * \param control Control for \c res_stasis
 * \param role Role to apply
 *
 * \return 0 for success
 * \return -1 for error.
 */
int stasis_app_control_add_role(struct stasis_app_control *control, const char *role);

/*!
 * \brief Clear bridge roles currently applied to a channel controlled by a stasis app control
 *
 * \param control Control for \c res_stasis
 */
void stasis_app_control_clear_roles(struct stasis_app_control *control);

/*!
 * \brief Exit \c res_stasis and continue execution in the dialplan.
 *
 * If the channel is no longer in \c res_stasis, this function does nothing.
 *
 * \param control Control for \c res_stasis
 * \param context An optional context to continue to
 * \param extension An optional extension to continue to
 * \param priority An optional priority to continue to
 *
 * \return 0 for success
 * \return -1 for error.
 */
int stasis_app_control_continue(struct stasis_app_control *control, const char *context, const char *extension, int priority);

/*!
 * \brief Mute the channel associated with this control.
 *
 * \param control Control for \c res_stasis.
 * \param direction The direction in which the audio should be muted.
 * \param frametype The type of stream that should be muted.
 *
 * \return 0 for success
 * \return -1 for error.
 */
int stasis_app_control_mute(struct stasis_app_control *control, unsigned int direction, enum ast_frame_type frametype);

/*!
 * \brief Unmute the channel associated with this control.
 *
 * \param control Control for \c res_stasis.
 * \param direction The direction in which the audio should be unmuted.
 * \param frametype The type of stream that should be unmuted.
 *
 * \return 0 for success
 * \return -1 for error.
 */
int stasis_app_control_unmute(struct stasis_app_control *control, unsigned int direction, enum ast_frame_type frametype);

/*!
 * \brief Answer the channel associated with this control.
 * \param control Control for \c res_stasis.
 * \return 0 for success.
 * \return Non-zero for error.
 */
int stasis_app_control_answer(struct stasis_app_control *control);

/*!
 * \brief Get the value of a variable on the channel associated with this control.
 * \param control Control for \c res_stasis.
 * \param variable The name of the variable.
 * \return The value of the variable.  The returned variable must be freed.
 */
char *stasis_app_control_get_channel_var(struct stasis_app_control *control, const char *variable);

/*!
 * \brief Set a variable on the channel associated with this control to value.
 * \param control Control for \c res_stasis.
 * \param variable The name of the variable
 * \param value The value to set the variable to
 *
 * \return 0 for success.
 * \return -1 for error.
 */
int stasis_app_control_set_channel_var(struct stasis_app_control *control, const char *variable, const char *value);

/*!
 * \brief Place the channel associated with the control on hold.
 * \param control Control for \c res_stasis.
 */
void stasis_app_control_hold(struct stasis_app_control *control);

/*!
 * \brief Remove the channel associated with the control from hold.
 * \param control Control for \c res_stasis.
 */
void stasis_app_control_unhold(struct stasis_app_control *control);

/*!
 * \brief Play music on hold to a channel (does not affect hold status)
 * \param control Control for \c res_stasis.
 * \param moh_class class of music on hold to play (NULL allowed)
 */
void stasis_app_control_moh_start(struct stasis_app_control *control, const char *moh_class);

/*!
 * \brief Stop playing music on hold to a channel (does not affect hold status)
 * \param control Control for \c res_stasis.
 */
void stasis_app_control_moh_stop(struct stasis_app_control *control);

/*!
 * \brief Returns the most recent snapshot for the associated channel.
 *
 * The returned pointer is AO2 managed, so ao2_cleanup() when you're done.
 *
 * \param control Control for \c res_stasis.
 * \return Most recent snapshot. ao2_cleanup() when done.
 * \return \c NULL if channel isn't in cache.
 */
struct ast_channel_snapshot *stasis_app_control_get_snapshot(
	const struct stasis_app_control *control);

/*!
 * \brief Publish a message to the \a control's channel's topic.
 *
 * \param control Control to publish to
 * \param message Message to publish
 */
void stasis_app_control_publish(
	struct stasis_app_control *control, struct stasis_message *message);

/*!
 * \brief Queue a control frame without payload.
 *
 * \param control Control to publish to.
 * \param frame_type type of control frame.
 *
 * \return zero on success
 * \return non-zero on failure
 */
int stasis_app_control_queue_control(struct stasis_app_control *control,
	enum ast_control_frame_type frame_type);

/*!
 * \brief Create a bridge of the specified type.
 *
 * \param type The type of bridge to be created
 *
 * \return New bridge.
 * \return \c NULL on error.
 */
struct ast_bridge *stasis_app_bridge_create(const char *type);

/*!
 * \brief Returns the bridge with the given id.
 * \param bridge_id Uniqueid of the bridge.
 * \return NULL bridge not created by a Stasis application, or bridge does not exist.
 * \return Pointer to bridge.
 */
struct ast_bridge *stasis_app_bridge_find_by_id(
	const char *bridge_id);

/*!
 * \brief Add a channel to the bridge.
 *
 * \param control Control whose channel should be added to the bridge
 * \param bridge Pointer to the bridge
 */
void stasis_app_control_add_channel_to_bridge(
	struct stasis_app_control *control, struct ast_bridge *bridge);

/*!
 * \brief Remove a channel from the bridge.
 *
 * \param control Control whose channel should be removed from the bridge
 * \param bridge Pointer to the bridge
 */
void stasis_app_control_remove_channel_from_bridge(
	struct stasis_app_control *control, struct ast_bridge *bridge);

/*!
 * \brief Destroy the bridge.
 *
 * \param bridge_id Uniqueid of bridge to be destroyed
 *
 * \retval non-zero on failure
 * \retval zero on success
 */
void stasis_app_bridge_destroy(const char *bridge_id);

/*!
 * \brief Increment the res_stasis reference count.
 *
 * This ensures graceful shutdown happens in the proper order.
 */
void stasis_app_ref(void);

/*!
 * \brief Decrement the res_stasis reference count.
 *
 * This ensures graceful shutdown happens in the proper order.
 */
void stasis_app_unref(void);

/*! @} */

#endif /* _ASTERISK_STASIS_APP_H */
