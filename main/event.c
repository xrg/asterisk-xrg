/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2007 - 2008, Digium, Inc.
 *
 * Russell Bryant <russell@digium.com>
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
 * \brief Internal generic event system
 *
 * \author Russell Bryant <russell@digium.com>
 */

/*** MODULEINFO
	<support_level>core</support_level>
 ***/

#include "asterisk.h"

ASTERISK_FILE_VERSION(__FILE__, "$Revision$")

#include "asterisk/_private.h"

#include "asterisk/event.h"
#include "asterisk/linkedlists.h"
#include "asterisk/dlinkedlists.h"
#include "asterisk/lock.h"
#include "asterisk/utils.h"
#include "asterisk/unaligned.h"
#include "asterisk/utils.h"
#include "asterisk/taskprocessor.h"
#include "asterisk/astobj2.h"
#include "asterisk/cli.h"

static int event_append_ie_raw(struct ast_event **event, enum ast_event_ie_type ie_type,
	const void *data, size_t data_len);
static const void *event_get_ie_raw(const struct ast_event *event, enum ast_event_ie_type ie_type);

/*!
 * \brief An event information element
 *
 * \note The format of this structure is important.  Since these events may
 *       be sent directly over a network, changing this structure will break
 *       compatibility with older versions.  However, at this point, this code
 *       has not made it into a release, so it is still fair game for change.
 */
struct ast_event_ie {
	enum ast_event_ie_type ie_type:16;
	/*! Total length of the IE payload */
	uint16_t ie_payload_len;
	unsigned char ie_payload[0];
} __attribute__((packed));

/*!
 * \brief The payload for a string information element
 */
struct ast_event_ie_str_payload {
	/*! \brief A hash calculated with ast_str_hash(), to speed up comparisons */
	uint32_t hash;
	/*! \brief The actual string, null terminated */
	char str[1];
} __attribute__((packed));

/*!
 * \brief An event
 *
 * An ast_event consists of an event header (this structure), and zero or
 * more information elements defined by ast_event_ie.
 *
 * \note The format of this structure is important.  Since these events may
 *       be sent directly over a network, changing this structure will break
 *       compatibility with older versions.  However, at this point, this code
 *       has not made it into a release, so it is still fair game for change.
 */
struct ast_event {
	/*! Event type */
	enum ast_event_type type:16;
	/*! Total length of the event */
	uint16_t event_len:16;
	/*! The data payload of the event, made up of information elements */
	unsigned char payload[0];
} __attribute__((packed));


struct ast_event_ie_val {
	AST_LIST_ENTRY(ast_event_ie_val) entry;
	enum ast_event_ie_type ie_type;
	enum ast_event_ie_pltype ie_pltype;
	union {
		uint32_t uint;
		struct {
			uint32_t hash;
			const char *str;
		};
		void *raw;
	} payload;
	size_t raw_datalen;
};

struct ie_map {
	enum ast_event_ie_pltype ie_pltype;
	const char *name;
};

/*!
 * \brief IE payload types and names
 */
static const struct ie_map ie_maps[AST_EVENT_IE_TOTAL] = {
	[AST_EVENT_IE_UNIQUEID]            = { AST_EVENT_IE_PLTYPE_UINT, "UniqueID" },
	[AST_EVENT_IE_EVENTTYPE]           = { AST_EVENT_IE_PLTYPE_UINT, "EventType" },
	[AST_EVENT_IE_EXISTS]              = { AST_EVENT_IE_PLTYPE_UINT, "Exists" },
	[AST_EVENT_IE_CONTEXT]             = { AST_EVENT_IE_PLTYPE_STR,  "Context" },
	[AST_EVENT_IE_CEL_EVENT_TYPE]      = { AST_EVENT_IE_PLTYPE_UINT, "CELEventType" },
	[AST_EVENT_IE_CEL_EVENT_TIME]      = { AST_EVENT_IE_PLTYPE_UINT, "CELEventTime" },
	[AST_EVENT_IE_CEL_EVENT_TIME_USEC] = { AST_EVENT_IE_PLTYPE_UINT, "CELEventTimeUSec" },
	[AST_EVENT_IE_CEL_USEREVENT_NAME]  = { AST_EVENT_IE_PLTYPE_UINT, "CELUserEventName" },
	[AST_EVENT_IE_CEL_CIDNAME]         = { AST_EVENT_IE_PLTYPE_STR,  "CELCIDName" },
	[AST_EVENT_IE_CEL_CIDNUM]          = { AST_EVENT_IE_PLTYPE_STR,  "CELCIDNum" },
	[AST_EVENT_IE_CEL_EXTEN]           = { AST_EVENT_IE_PLTYPE_STR,  "CELExten" },
	[AST_EVENT_IE_CEL_CONTEXT]         = { AST_EVENT_IE_PLTYPE_STR,  "CELContext" },
	[AST_EVENT_IE_CEL_CHANNAME]        = { AST_EVENT_IE_PLTYPE_STR,  "CELChanName" },
	[AST_EVENT_IE_CEL_APPNAME]         = { AST_EVENT_IE_PLTYPE_STR,  "CELAppName" },
	[AST_EVENT_IE_CEL_APPDATA]         = { AST_EVENT_IE_PLTYPE_STR,  "CELAppData" },
	[AST_EVENT_IE_CEL_AMAFLAGS]        = { AST_EVENT_IE_PLTYPE_STR,  "CELAMAFlags" },
	[AST_EVENT_IE_CEL_ACCTCODE]        = { AST_EVENT_IE_PLTYPE_UINT, "CELAcctCode" },
	[AST_EVENT_IE_CEL_UNIQUEID]        = { AST_EVENT_IE_PLTYPE_STR,  "CELUniqueID" },
	[AST_EVENT_IE_CEL_USERFIELD]       = { AST_EVENT_IE_PLTYPE_STR,  "CELUserField" },
	[AST_EVENT_IE_CEL_CIDANI]          = { AST_EVENT_IE_PLTYPE_STR,  "CELCIDani" },
	[AST_EVENT_IE_CEL_CIDRDNIS]        = { AST_EVENT_IE_PLTYPE_STR,  "CELCIDrdnis" },
	[AST_EVENT_IE_CEL_CIDDNID]         = { AST_EVENT_IE_PLTYPE_STR,  "CELCIDdnid" },
	[AST_EVENT_IE_CEL_PEER]            = { AST_EVENT_IE_PLTYPE_STR,  "CELPeer" },
	[AST_EVENT_IE_CEL_LINKEDID]        = { AST_EVENT_IE_PLTYPE_STR,  "CELLinkedID" },
	[AST_EVENT_IE_CEL_PEERACCT]        = { AST_EVENT_IE_PLTYPE_STR,  "CELPeerAcct" },
	[AST_EVENT_IE_CEL_EXTRA]           = { AST_EVENT_IE_PLTYPE_STR,  "CELExtra" },
	[AST_EVENT_IE_EVENT_VERSION]       = { AST_EVENT_IE_PLTYPE_UINT, "EventVersion" },
	[AST_EVENT_IE_SERVICE]             = { AST_EVENT_IE_PLTYPE_STR,  "Service" },
	[AST_EVENT_IE_MODULE]              = { AST_EVENT_IE_PLTYPE_STR,  "Module" },
	[AST_EVENT_IE_ACCOUNT_ID]          = { AST_EVENT_IE_PLTYPE_STR,  "AccountID" },
	[AST_EVENT_IE_SESSION_ID]          = { AST_EVENT_IE_PLTYPE_STR,  "SessionID" },
	[AST_EVENT_IE_SESSION_TV]          = { AST_EVENT_IE_PLTYPE_STR,  "SessionTV" },
	[AST_EVENT_IE_ACL_NAME]            = { AST_EVENT_IE_PLTYPE_STR,  "ACLName" },
	[AST_EVENT_IE_LOCAL_ADDR]          = { AST_EVENT_IE_PLTYPE_STR,  "LocalAddress" },
	[AST_EVENT_IE_REMOTE_ADDR]         = { AST_EVENT_IE_PLTYPE_STR,  "RemoteAddress" },
	[AST_EVENT_IE_EVENT_TV]            = { AST_EVENT_IE_PLTYPE_STR,  "EventTV" },
	[AST_EVENT_IE_REQUEST_TYPE]        = { AST_EVENT_IE_PLTYPE_STR,  "RequestType" },
	[AST_EVENT_IE_REQUEST_PARAMS]      = { AST_EVENT_IE_PLTYPE_STR,  "RequestParams" },
	[AST_EVENT_IE_AUTH_METHOD]         = { AST_EVENT_IE_PLTYPE_STR,  "AuthMethod" },
	[AST_EVENT_IE_SEVERITY]            = { AST_EVENT_IE_PLTYPE_STR,  "Severity" },
	[AST_EVENT_IE_EXPECTED_ADDR]       = { AST_EVENT_IE_PLTYPE_STR,  "ExpectedAddress" },
	[AST_EVENT_IE_CHALLENGE]           = { AST_EVENT_IE_PLTYPE_STR,  "Challenge" },
	[AST_EVENT_IE_RESPONSE]            = { AST_EVENT_IE_PLTYPE_STR,  "Response" },
	[AST_EVENT_IE_EXPECTED_RESPONSE]   = { AST_EVENT_IE_PLTYPE_STR,  "ExpectedResponse" },
	[AST_EVENT_IE_RECEIVED_CHALLENGE]  = { AST_EVENT_IE_PLTYPE_STR,  "ReceivedChallenge" },
	[AST_EVENT_IE_RECEIVED_HASH]       = { AST_EVENT_IE_PLTYPE_STR,  "ReceivedHash" },
	[AST_EVENT_IE_USING_PASSWORD]      = { AST_EVENT_IE_PLTYPE_UINT, "UsingPassword" },
	[AST_EVENT_IE_ATTEMPTED_TRANSPORT] = { AST_EVENT_IE_PLTYPE_STR,  "AttemptedTransport" },
};

const char *ast_event_get_ie_type_name(enum ast_event_ie_type ie_type)
{
	if (ie_type <= 0 || ie_type >= ARRAY_LEN(ie_maps)) {
		ast_log(LOG_ERROR, "Invalid IE type - '%d'\n", ie_type);
		return "";
	}

	return ie_maps[ie_type].name;
}

enum ast_event_ie_pltype ast_event_get_ie_pltype(enum ast_event_ie_type ie_type)
{
	if (ie_type <= 0 || ie_type >= ARRAY_LEN(ie_maps)) {
		ast_log(LOG_ERROR, "Invalid IE type - '%d'\n", ie_type);
		return AST_EVENT_IE_PLTYPE_UNKNOWN;
	}

	return ie_maps[ie_type].ie_pltype;
}

size_t ast_event_get_size(const struct ast_event *event)
{
	size_t res;

	res = ntohs(event->event_len);

	return res;
}

/*! \brief Subscription event check list. */
struct ast_ev_check_list {
	AST_LIST_HEAD_NOLOCK(, ast_event_ie_val) ie_vals;
};

int ast_event_iterator_init(struct ast_event_iterator *iterator, const struct ast_event *event)
{
	int res = 0;

	iterator->event_len = ast_event_get_size(event);
	iterator->event = event;
	if (iterator->event_len >= sizeof(*event) + sizeof(struct ast_event_ie)) {
		iterator->ie = (struct ast_event_ie *) ( ((char *) event) + sizeof(*event) );
	} else {
		iterator->ie = NULL;
		res = -1;
	}

	return res;
}

int ast_event_iterator_next(struct ast_event_iterator *iterator)
{
	iterator->ie = (struct ast_event_ie *) ( ((char *) iterator->ie) + sizeof(*iterator->ie) + ntohs(iterator->ie->ie_payload_len));
	return ((iterator->event_len <= (((char *) iterator->ie) - ((char *) iterator->event))) ? -1 : 0);
}

enum ast_event_ie_type ast_event_iterator_get_ie_type(struct ast_event_iterator *iterator)
{
	return ntohs(iterator->ie->ie_type);
}

uint32_t ast_event_iterator_get_ie_uint(struct ast_event_iterator *iterator)
{
	return ntohl(get_unaligned_uint32(iterator->ie->ie_payload));
}

const char *ast_event_iterator_get_ie_str(struct ast_event_iterator *iterator)
{
	const struct ast_event_ie_str_payload *str_payload;

	str_payload = (struct ast_event_ie_str_payload *) iterator->ie->ie_payload;

	return str_payload ? str_payload->str : NULL;
}

static void *event_iterator_get_ie_raw(struct ast_event_iterator *iterator)
{
	return iterator->ie->ie_payload;
}

enum ast_event_type ast_event_get_type(const struct ast_event *event)
{
	return ntohs(event->type);
}

uint32_t ast_event_get_ie_uint(const struct ast_event *event, enum ast_event_ie_type ie_type)
{
	const uint32_t *ie_val;

	ie_val = event_get_ie_raw(event, ie_type);

	return ie_val ? ntohl(get_unaligned_uint32(ie_val)) : 0;
}

const char *ast_event_get_ie_str(const struct ast_event *event, enum ast_event_ie_type ie_type)
{
	const struct ast_event_ie_str_payload *str_payload;

	str_payload = event_get_ie_raw(event, ie_type);

	return str_payload ? str_payload->str : NULL;
}

static const void *event_get_ie_raw(const struct ast_event *event, enum ast_event_ie_type ie_type)
{
	struct ast_event_iterator iterator;
	int res;

	for (res = ast_event_iterator_init(&iterator, event); !res; res = ast_event_iterator_next(&iterator)) {
		if (ast_event_iterator_get_ie_type(&iterator) == ie_type) {
			return event_iterator_get_ie_raw(&iterator);
		}
	}

	return NULL;
}

int ast_event_append_ie_str(struct ast_event **event, enum ast_event_ie_type ie_type,
	const char *str)
{
	struct ast_event_ie_str_payload *str_payload;
	size_t payload_len;

	payload_len = sizeof(*str_payload) + strlen(str);
	str_payload = ast_alloca(payload_len);

	strcpy(str_payload->str, str);
	str_payload->hash = ast_str_hash(str);

	return event_append_ie_raw(event, ie_type, str_payload, payload_len);
}

int ast_event_append_ie_uint(struct ast_event **event, enum ast_event_ie_type ie_type,
	uint32_t data)
{
	data = htonl(data);
	return event_append_ie_raw(event, ie_type, &data, sizeof(data));
}

static int event_append_ie_raw(struct ast_event **event, enum ast_event_ie_type ie_type,
	const void *data, size_t data_len)
{
	struct ast_event_ie *ie;
	unsigned int extra_len;
	uint16_t event_len;

	event_len = ntohs((*event)->event_len);
	extra_len = sizeof(*ie) + data_len;

	if (!(*event = ast_realloc(*event, event_len + extra_len))) {
		return -1;
	}

	ie = (struct ast_event_ie *) ( ((char *) *event) + event_len );
	ie->ie_type = htons(ie_type);
	ie->ie_payload_len = htons(data_len);
	memcpy(ie->ie_payload, data, data_len);

	(*event)->event_len = htons(event_len + extra_len);

	return 0;
}

struct ast_event *ast_event_new(enum ast_event_type type, ...)
{
	va_list ap;
	struct ast_event *event;
	enum ast_event_ie_type ie_type;
	struct ast_event_ie_val *ie_val;
	AST_LIST_HEAD_NOLOCK_STATIC(ie_vals, ast_event_ie_val);

	/* Invalid type */
	if (type >= AST_EVENT_TOTAL) {
		ast_log(LOG_WARNING, "Someone tried to create an event of invalid "
			"type '%d'!\n", type);
		return NULL;
	}

	va_start(ap, type);
	for (ie_type = va_arg(ap, enum ast_event_ie_type);
		ie_type != AST_EVENT_IE_END;
		ie_type = va_arg(ap, enum ast_event_ie_type))
	{
		struct ast_event_ie_val *ie_value = ast_alloca(sizeof(*ie_value));
		int insert = 0;

		memset(ie_value, 0, sizeof(*ie_value));
		ie_value->ie_type = ie_type;
		ie_value->ie_pltype = va_arg(ap, enum ast_event_ie_pltype);
		switch (ie_value->ie_pltype) {
		case AST_EVENT_IE_PLTYPE_UINT:
			ie_value->payload.uint = va_arg(ap, uint32_t);
			insert = 1;
			break;
		case AST_EVENT_IE_PLTYPE_STR:
			ie_value->payload.str = va_arg(ap, const char *);
			insert = 1;
			break;
		case AST_EVENT_IE_PLTYPE_RAW:
		{
			void *data = va_arg(ap, void *);
			size_t datalen = va_arg(ap, size_t);
			ie_value->payload.raw = ast_alloca(datalen);
			memcpy(ie_value->payload.raw, data, datalen);
			ie_value->raw_datalen = datalen;
			insert = 1;
			break;
		}
		case AST_EVENT_IE_PLTYPE_UNKNOWN:
			break;
		}

		if (insert) {
			AST_LIST_INSERT_TAIL(&ie_vals, ie_value, entry);
		} else {
			ast_log(LOG_WARNING, "Unsupported PLTYPE(%d)\n", ie_value->ie_pltype);
		}
	}
	va_end(ap);

	if (!(event = ast_calloc(1, sizeof(*event)))) {
		return NULL;
	}

	event->type = htons(type);
	event->event_len = htons(sizeof(*event));

	AST_LIST_TRAVERSE(&ie_vals, ie_val, entry) {
		switch (ie_val->ie_pltype) {
		case AST_EVENT_IE_PLTYPE_STR:
			ast_event_append_ie_str(&event, ie_val->ie_type, ie_val->payload.str);
			break;
		case AST_EVENT_IE_PLTYPE_UINT:
			ast_event_append_ie_uint(&event, ie_val->ie_type, ie_val->payload.uint);
			break;
		case AST_EVENT_IE_PLTYPE_RAW:
			event_append_ie_raw(&event, ie_val->ie_type,
					ie_val->payload.raw, ie_val->raw_datalen);
			break;
		case AST_EVENT_IE_PLTYPE_UNKNOWN:
			break;
		}

		/* realloc inside one of the append functions failed */
		if (!event) {
			return NULL;
		}
	}

	return event;
}

void ast_event_destroy(struct ast_event *event)
{
	ast_free(event);
}
