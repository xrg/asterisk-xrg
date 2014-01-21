/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2013, Digium, Inc.
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
 * res/ari/resource_applications.c
 *
 * Stasis application resources
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

#ifndef _ASTERISK_RESOURCE_APPLICATIONS_H
#define _ASTERISK_RESOURCE_APPLICATIONS_H

#include "asterisk/ari.h"

/*! \brief Argument struct for ast_ari_applications_list() */
struct ast_ari_applications_list_args {
};
/*!
 * \brief List all applications.
 *
 * \param headers HTTP headers
 * \param args Swagger parameters
 * \param[out] response HTTP response
 */
void ast_ari_applications_list(struct ast_variable *headers, struct ast_ari_applications_list_args *args, struct ast_ari_response *response);
/*! \brief Argument struct for ast_ari_applications_get() */
struct ast_ari_applications_get_args {
	/*! \brief Application's name */
	const char *application_name;
};
/*!
 * \brief Get details of an application.
 *
 * \param headers HTTP headers
 * \param args Swagger parameters
 * \param[out] response HTTP response
 */
void ast_ari_applications_get(struct ast_variable *headers, struct ast_ari_applications_get_args *args, struct ast_ari_response *response);
/*! \brief Argument struct for ast_ari_applications_subscribe() */
struct ast_ari_applications_subscribe_args {
	/*! \brief Application's name */
	const char *application_name;
	/*! \brief Array of URI for event source (channel:{channelId}, bridge:{bridgeId}, endpoint:{tech}/{resource}, deviceState:{deviceName} */
	const char **event_source;
	/*! \brief Length of event_source array. */
	size_t event_source_count;
	/*! \brief Parsing context for event_source. */
	char *event_source_parse;
};
/*!
 * \brief Body parsing function for /applications/{applicationName}/subscription.
 * \param body The JSON body from which to parse parameters.
 * \param[out] args The args structure to parse into.
 * \retval zero on success
 * \retval non-zero on failure
 */
int ast_ari_applications_subscribe_parse_body(
	struct ast_json *body,
	struct ast_ari_applications_subscribe_args *args);

/*!
 * \brief Subscribe an application to a event source.
 *
 * Returns the state of the application after the subscriptions have changed
 *
 * \param headers HTTP headers
 * \param args Swagger parameters
 * \param[out] response HTTP response
 */
void ast_ari_applications_subscribe(struct ast_variable *headers, struct ast_ari_applications_subscribe_args *args, struct ast_ari_response *response);
/*! \brief Argument struct for ast_ari_applications_unsubscribe() */
struct ast_ari_applications_unsubscribe_args {
	/*! \brief Application's name */
	const char *application_name;
	/*! \brief Array of URI for event source (channel:{channelId}, bridge:{bridgeId}, endpoint:{tech}/{resource}, deviceState:{deviceName} */
	const char **event_source;
	/*! \brief Length of event_source array. */
	size_t event_source_count;
	/*! \brief Parsing context for event_source. */
	char *event_source_parse;
};
/*!
 * \brief Body parsing function for /applications/{applicationName}/subscription.
 * \param body The JSON body from which to parse parameters.
 * \param[out] args The args structure to parse into.
 * \retval zero on success
 * \retval non-zero on failure
 */
int ast_ari_applications_unsubscribe_parse_body(
	struct ast_json *body,
	struct ast_ari_applications_unsubscribe_args *args);

/*!
 * \brief Unsubscribe an application from an event source.
 *
 * Returns the state of the application after the subscriptions have changed
 *
 * \param headers HTTP headers
 * \param args Swagger parameters
 * \param[out] response HTTP response
 */
void ast_ari_applications_unsubscribe(struct ast_variable *headers, struct ast_ari_applications_unsubscribe_args *args, struct ast_ari_response *response);

#endif /* _ASTERISK_RESOURCE_APPLICATIONS_H */
