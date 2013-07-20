/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2008 - 2009, Digium, Inc.
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

/*! 
 * \file
 * \brief Call Event Logging API 
 *
 * \todo TODO: There some event types that have been defined here, but are not
 *       yet used anywhere in the code. It would be really awesome if someone
 *       went through and had Asterisk generate these events where it is
 *       appropriate to do so. The defined, but unused events are:
 *       CONF_ENTER, CONF_EXIT, CONF_START, CONF_END, 3WAY_START, 3WAY_END,
 *       TRANSFER, and HOOKFLASH.
 */

#ifndef __AST_CEL_H__
#define __AST_CEL_H__

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#include "asterisk/event.h"

/*!
 * \brief CEL event types
 */
enum ast_cel_event_type {
	/*! \brief channel birth */
	AST_CEL_CHANNEL_START = 1,
	/*! \brief channel end */
	AST_CEL_CHANNEL_END = 2,
	/*! \brief hangup terminates connection */
	AST_CEL_HANGUP = 3,
	/*! \brief A ringing phone is answered */
	AST_CEL_ANSWER = 4,
	/*! \brief an app starts */
	AST_CEL_APP_START = 5,
	/*! \brief an app ends */
	AST_CEL_APP_END = 6,
	/*! \brief a bridge is established */
	AST_CEL_BRIDGE_START = 7,
	/*! \brief a bridge is torn down */
	AST_CEL_BRIDGE_END = 8,
	/*! \brief a conference is started */
	AST_CEL_CONF_START = 9,
	/*! \brief a conference is ended */
	AST_CEL_CONF_END = 10,
	/*! \brief a channel is parked */
	AST_CEL_PARK_START = 11,
	/*! \brief channel out of the park */
	AST_CEL_PARK_END = 12,
	/*! \brief a transfer occurs */
	AST_CEL_BLINDTRANSFER = 13,
	/*! \brief a transfer occurs */
	AST_CEL_ATTENDEDTRANSFER = 14,
	/*! \brief a 3-way conference, usually part of a transfer */
	AST_CEL_HOOKFLASH = 16,
	/*! \brief a 3-way conference, usually part of a transfer */
	AST_CEL_3WAY_START = 17,
	/*! \brief a 3-way conference, usually part of a transfer */
	AST_CEL_3WAY_END = 18,
	/*! \brief channel enters a conference */
	AST_CEL_CONF_ENTER = 19,
	/*! \brief channel exits a conference */
	AST_CEL_CONF_EXIT = 20,
	/*! \brief a user-defined event, the event name field should be set  */
	AST_CEL_USER_DEFINED = 21,
	/*! \brief the last channel with the given linkedid is retired  */
	AST_CEL_LINKEDID_END = 22,
	/*! \brief a directed pickup was performed on this channel  */
	AST_CEL_PICKUP = 24,
	/*! \brief this call was forwarded somewhere else  */
	AST_CEL_FORWARD = 25,
	/*! \brief a bridge turned into a conference and will be treated as such until it is torn down */
	AST_CEL_BRIDGE_TO_CONF = 26,
};

/*! 
 * \brief Check to see if CEL is enabled
 *
 * \since 1.8
 *
 * \retval zero not enabled
 * \retval non-zero enabled
 */
unsigned int ast_cel_check_enabled(void);

/*! 
 * \brief Allocate a CEL record 
 *
 * \since 1.8
 *
 * \note The CEL record must be destroyed with ast_cel_destroy().
 *
 * \retval non-NULL an allocated ast_cel structure
 * \retval NULL error
 */
struct ast_cel *ast_cel_alloc(void);

/*! 
 * \brief Destroy a CEL record.
 *
 * \param cel the record to destroy
 *
 * \since 1.8
 *
 * \return nothing.
 */
void ast_cel_destroy(struct ast_cel *cel);

/*!
 * \brief Get the name of a CEL event type
 *
 * \param type the type to get the name of
 *
 * \since 1.8
 *
 * \return the string representation of the type
 */
const char *ast_cel_get_type_name(enum ast_cel_event_type type);

/*!
 * \brief Get the event type from a string
 *
 * \param name the event type name as a string
 *
 * \since 1.8
 *
 * \return the ast_cel_event_type given by the string
 */
enum ast_cel_event_type ast_cel_str_to_event_type(const char *name);

/*!
 * \brief Create a fake channel from data in a CEL event
 *
 * \note
 * This function creates a fake channel containing the
 * serialized channel data in the given cel event.  It should be
 * released with ast_channel_unref() but could be released with
 * ast_channel_release().
 *
 * \param event the CEL event
 *
 * \since 1.8
 *
 * \return a channel with the data filled in, or NULL on error
 *
 * \todo This function is \b very expensive, especially given that some CEL backends
 *       use it on \b every CEL event.  This function really needs to go away at
 *       some point.
 */
struct ast_channel *ast_cel_fabricate_channel_from_event(const struct ast_event *event);

/*!
 * \brief Helper struct for getting the fields out of a CEL event
 */
struct ast_cel_event_record {
	/*!
	 * \brief struct ABI version
	 * \note This \b must be incremented when the struct changes.
	 */
	#define AST_CEL_EVENT_RECORD_VERSION 2
	/*!
	 * \brief struct ABI version
	 * \note This \b must stay as the first member.
	 */
	uint32_t version;
	enum ast_cel_event_type event_type;
	struct timeval event_time;
	const char *event_name;
	const char *user_defined_name;
	const char *caller_id_name;
	const char *caller_id_num;
	const char *caller_id_ani;
	const char *caller_id_rdnis;
	const char *caller_id_dnid;
	const char *extension;
	const char *context;
	const char *channel_name;
	const char *application_name;
	const char *application_data;
	const char *account_code;
	const char *peer_account;
	const char *unique_id;
	const char *linked_id;
	uint amaflag;
	const char *user_field;
	const char *peer;
	const char *extra;
};

/*!
 * \brief Fill in an ast_cel_event_record from a CEL event
 *
 * \param[in] event the CEL event
 * \param[out] r the ast_cel_event_record to fill in
 *
 * \since 1.8
 *
 * \retval 0 success
 * \retval non-zero failure
 */
int ast_cel_fill_record(const struct ast_event *event, struct ast_cel_event_record *r);

/*!
 * \brief Publish a CEL event
 * \since 12
 *
 * \param chan This is the primary channel associated with this channel event.
 * \param event_type This is the type of call event being reported.
 * \param blob This contains any additional parameters that need to be conveyed for this event.
 */
void ast_cel_publish_event(struct ast_channel *chan,
	enum ast_cel_event_type event_type,
	struct ast_json *blob);

/*!
 * \brief Get the CEL topic
 *
 * \retval The CEL topic
 * \retval NULL if not allocated
 */
struct stasis_topic *ast_cel_topic(void);

/*! \brief A structure to hold CEL global configuration options */
struct ast_cel_general_config {
	AST_DECLARE_STRING_FIELDS(
		AST_STRING_FIELD(date_format); /*!< The desired date format for logging */
	);
	int enable;			/*!< Whether CEL is enabled */
	int64_t events;			/*!< The events to be logged */
	/*! The apps for which to log app start and end events. This is
	 * ast_str_container_alloc()ed and filled with ao2-allocated
	 * char* which are all-lowercase application names. */
	struct ao2_container *apps;
};

/*!
 * \brief Allocate a CEL configuration object
 *
 * \retval NULL on error
 * \retval The new CEL configuration object
 */
void *ast_cel_general_config_alloc(void);

/*!
 * \since 12
 * \brief Obtain the current CEL configuration
 *
 * The configuration is a ref counted object. The caller of this function must
 * decrement the ref count when finished with the configuration.
 *
 * \retval NULL on error
 * \retval The current CEL configuration
 */
struct ast_cel_general_config *ast_cel_get_config(void);

/*!
 * \since 12
 * \brief Set the current CEL configuration
 *
 * \param config The new CEL configuration
 */
void ast_cel_set_config(struct ast_cel_general_config *config);

struct ast_channel_snapshot;
/*!
 * \brief Allocate and populate a CEL event structure
 *
 * \param snapshot An ast_channel_snapshot of the primary channel associated
 *        with this channel event.
 * \param event_type The type of call event being reported.
 * \param userdefevname Custom name for the call event. (optional)
 * \param extra An event-specific opaque JSON blob to be rendered and placed
 *        in the "CEL_EXTRA" information element of the call event. (optional)
 * \param peer_name The peer name to be placed into the event. (optional)
 *
 * \since 12
 *
 * \retval The created ast_event structure
 * \retval NULL on failure
 */
struct ast_event *ast_cel_create_event(struct ast_channel_snapshot *snapshot,
		enum ast_cel_event_type event_type, const char *userdefevname,
		struct ast_json *extra, const char *peer_name);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif /* __AST_CEL_H__ */
