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
 * res/ari/resource_events.c
 *
 * WebSocket resource
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

#ifndef _ASTERISK_RESOURCE_EVENTS_H
#define _ASTERISK_RESOURCE_EVENTS_H

#include "asterisk/ari.h"

/*! \brief Argument struct for ast_ari_events_event_websocket() */
struct ast_ari_events_event_websocket_args {
	/*! \brief Array of Applications to subscribe to. */
	const char **app;
	/*! \brief Length of app array. */
	size_t app_count;
	/*! \brief Parsing context for app. */
	char *app_parse;
};
/*!
 * \brief WebSocket connection for events.
 *
 * \param session ARI WebSocket.
 * \param headers HTTP headers.
 * \param args Swagger parameters.
 */
void ast_ari_websocket_events_event_websocket(struct ast_ari_websocket_session *session, struct ast_variable *headers, struct ast_ari_events_event_websocket_args *args);
/*! Argument struct for ast_ari_events_user_event() */
struct ast_ari_events_user_event_args {
	/*! Event name */
	const char *event_name;
	/*! The name of the application that will receive this event */
	const char *application;
	/*! Array of URI for event source (channel:{channelId}, bridge:{bridgeId}, endpoint:{tech}/{resource}, deviceState:{deviceName} */
	const char **source;
	/*! Length of source array. */
	size_t source_count;
	/*! Parsing context for source. */
	char *source_parse;
	/*! custom key/value pairs added to the user event */
	struct ast_json *variables;
};
/*!
 * \brief Body parsing function for /events/user/{eventName}.
 * \param body The JSON body from which to parse parameters.
 * \param[out] args The args structure to parse into.
 * \retval zero on success
 * \retval non-zero on failure
 */
int ast_ari_events_user_event_parse_body(
	struct ast_json *body,
	struct ast_ari_events_user_event_args *args);

/*!
 * \brief Generate a user event.
 *
 * \param headers HTTP headers
 * \param args Swagger parameters
 * \param[out] response HTTP response
 */
void ast_ari_events_user_event(struct ast_variable *headers, struct ast_ari_events_user_event_args *args, struct ast_ari_response *response);

#endif /* _ASTERISK_RESOURCE_EVENTS_H */
