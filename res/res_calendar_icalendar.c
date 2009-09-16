/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2008 - 2009, Digium, Inc.
 *
 * Terry Wilson <twilson@digium.com>
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
 * \brief Resource for handling iCalnedar calendars
 */

/*** MODULEINFO
	<depend>neon</depend>
	<depend>ical</depend>
***/
#include "asterisk.h"

ASTERISK_FILE_VERSION(__FILE__, "$Revision$")

#include <libical/ical.h>
#include <neon/ne_session.h>
#include <neon/ne_uri.h>
#include <neon/ne_request.h>
#include <neon/ne_auth.h>

#include "asterisk/module.h"
#include "asterisk/calendar.h"
#include "asterisk/lock.h"
#include "asterisk/config.h"
#include "asterisk/astobj2.h"

static void *ical_load_calendar(void *data);
static void *unref_icalendar(void *obj);

static struct ast_calendar_tech ical_tech = {
	.type = "ical",
	.module = AST_MODULE,
	.description = "iCalendar .ics calendars",
	.load_calendar = ical_load_calendar,
	.unref_calendar = unref_icalendar,
};

struct icalendar_pvt {
	AST_DECLARE_STRING_FIELDS(
		AST_STRING_FIELD(url);
		AST_STRING_FIELD(user);
		AST_STRING_FIELD(secret);
	);
	struct ast_calendar *owner;
	ne_uri uri;
	ne_session *session;
	icalcomponent *data;
	struct ao2_container *events;
};

static int cb_true(void *user_data, void *arg, int flags)
{
	return CMP_MATCH;
}

static void icalendar_destructor(void *obj)
{
	struct icalendar_pvt *pvt = obj;

	ast_debug(1, "Destroying pvt for iCalendar %s\n", pvt->owner->name);
	if (pvt->session) {
		ne_session_destroy(pvt->session);
	}
	if (pvt->data) {
		icalcomponent_free(pvt->data);
	}
	ast_string_field_free_memory(pvt);

	ao2_callback(pvt->events, OBJ_UNLINK | OBJ_NODATA | OBJ_MULTIPLE, cb_true, NULL);

	ao2_ref(pvt->events, -1);
}

static void *unref_icalendar(void *obj)
{
	struct icalendar_pvt *pvt = obj;

	ao2_ref(pvt, -1);
	return NULL;
}

static int fetch_response_reader(void *data, const char *block, size_t len)
{
	struct ast_str **response = data;
	unsigned char *tmp;

	if (!(tmp = ast_malloc(len + 1))) {
		return -1;
	}
	memcpy(tmp, block, len);
	tmp[len] = '\0';
	ast_str_append(response, 0, "%s", tmp);
	ast_free(tmp);

	return 0;
}

static int auth_credentials(void *userdata, const char *realm, int attempts, char *username, char *secret)
{
	struct icalendar_pvt *pvt = userdata;

	if (attempts > 1) {
		ast_log(LOG_WARNING, "Invalid username or password for iCalendar '%s'\n", pvt->owner->name);
		return -1;
	}

	ne_strnzcpy(username, pvt->user, NE_ABUFSIZ);
	ne_strnzcpy(secret, pvt->secret, NE_ABUFSIZ);

	return 0;
}

static icalcomponent *fetch_icalendar(struct icalendar_pvt *pvt)
{
	int ret;
	struct ast_str *response;
	ne_request *req;
	icalcomponent *comp = NULL;

	if (!pvt) {
		ast_log(LOG_ERROR, "There is no private!\n");
	}

	if (!(response = ast_str_create(512))) {
		ast_log(LOG_ERROR, "Could not allocate memory for response.\n");
		return NULL;
	}

	req = ne_request_create(pvt->session, "GET", pvt->uri.path);
	ne_add_response_body_reader(req, ne_accept_2xx, fetch_response_reader, &response);

	ret = ne_request_dispatch(req);
	if (ret != NE_OK || !ast_str_strlen(response)) {
		ast_log(LOG_WARNING, "Unable to retrieve iCalendar '%s' from '%s': %s\n", pvt->owner->name, pvt->url, ne_get_error(pvt->session));
		ast_free(response);
		return NULL;
	}

	if (!ast_strlen_zero(ast_str_buffer(response))) {
		comp = icalparser_parse_string(ast_str_buffer(response));
	}
	ast_free(response);

	return comp;
}

static void icalendar_add_event(icalcomponent *comp, struct icaltime_span *span, void *data)
{
	struct icalendar_pvt *pvt = data;
	struct ast_calendar_event *event;
	icaltimezone *utc = icaltimezone_get_utc_timezone();
	icaltimetype start, end, tmp;
	icalcomponent *valarm;
	icalproperty *prop;
	struct icaltriggertype trigger;

	if (!(pvt && pvt->owner)) {
		ast_log(LOG_ERROR, "Require a private structure with an ownenr\n");
		return;
	}

	if (!(event = ast_calendar_event_alloc(pvt->owner))) {
		ast_log(LOG_ERROR, "Could not allocate an event!\n");
		return;
	}

	start = icaltime_from_timet_with_zone(span->start, 0, utc);
	end = icaltime_from_timet_with_zone(span->end, 0, utc);
	event->start = span->start;
	event->end = span->end;

	switch(icalcomponent_get_status(comp)) {
	case ICAL_STATUS_CONFIRMED:
		event->busy_state = AST_CALENDAR_BS_BUSY;
		break;

	case ICAL_STATUS_TENTATIVE:
		event->busy_state = AST_CALENDAR_BS_BUSY_TENTATIVE;
		break;

	default:
		event->busy_state = AST_CALENDAR_BS_FREE;
	}

	if ((prop = icalcomponent_get_first_property(comp, ICAL_SUMMARY_PROPERTY))) {
		ast_string_field_set(event, summary, icalproperty_get_value_as_string(prop));
	}

	if ((prop = icalcomponent_get_first_property(comp, ICAL_DESCRIPTION_PROPERTY))) {
		ast_string_field_set(event, description, icalproperty_get_value_as_string(prop));
	}

	if ((prop = icalcomponent_get_first_property(comp, ICAL_ORGANIZER_PROPERTY))) {
		ast_string_field_set(event, organizer, icalproperty_get_value_as_string(prop));
	}

	if ((prop = icalcomponent_get_first_property(comp, ICAL_LOCATION_PROPERTY))) {
		ast_string_field_set(event, location, icalproperty_get_value_as_string(prop));
	}

	if ((prop = icalcomponent_get_first_property(comp, ICAL_UID_PROPERTY))) {
		ast_string_field_set(event, uid, icalproperty_get_value_as_string(prop));
	} else {
		ast_log(LOG_WARNING, "No UID found, but one is required. Generating, but updates may not be acurate\n");
		if (!ast_strlen_zero(event->summary)) {
			ast_string_field_set(event, uid, event->summary);
		} else {
			char tmp[100];
			snprintf(tmp, sizeof(tmp), "%lu", event->start);
			ast_string_field_set(event, uid, tmp);
		}
	}

	/* Get the attendees */
	for (prop = icalcomponent_get_first_property(comp, ICAL_ATTENDEE_PROPERTY);
			prop; prop = icalcomponent_get_next_property(comp, ICAL_ATTENDEE_PROPERTY)) {
		struct ast_calendar_attendee *attendee;
		const char *data;

		if (!(attendee = ast_calloc(1, sizeof(*attendee)))) {
			event = ast_calendar_unref_event(event);
			return;
		}
		data = icalproperty_get_attendee(prop);
		if (!ast_strlen_zero(data)) {
			attendee->data = ast_strdup(data);;
			AST_LIST_INSERT_TAIL(&event->attendees, attendee, next);
		}
	}


	/* Only set values for alarm based on VALARM.  Can be overriden in main/calendar.c by autoreminder
	 * therefore, go ahead and add events even if their is no VALARM or it is malformed
	 * Currently we are only getting the first VALARM and are handling repitition in main/calendar.c from calendar.conf */
	if (!(valarm = icalcomponent_get_first_component(comp, ICAL_VALARM_COMPONENT))) {
		ao2_link(pvt->events, event);
		event = ast_calendar_unref_event(event);
		return;
	}

	if (!(prop = icalcomponent_get_first_property(valarm, ICAL_TRIGGER_PROPERTY))) {
		ast_log(LOG_WARNING, "VALARM has no TRIGGER, skipping!\n");
		ao2_link(pvt->events, event);
		event = ast_calendar_unref_event(event);
		return;
	}

	trigger = icalproperty_get_trigger(prop);

	if (icaltriggertype_is_null_trigger(trigger)) {
		ast_log(LOG_WARNING, "Bad TRIGGER for VALARM, skipping!\n");
		ao2_link(pvt->events, event);
		event = ast_calendar_unref_event(event);
		return;
	}

	if (!icaltime_is_null_time(trigger.time)) { /* This is an absolute time */
		tmp = icaltime_convert_to_zone(trigger.time, utc);
		event->alarm = icaltime_as_timet_with_zone(tmp, utc);
	} else { /* Offset from either dtstart or dtend */
		/* XXX Technically you can check RELATED to see if the event fires from the END of the event
		 * But, I'm not sure I've ever seen anyone implement it in calendaring software, so I'm ignoring for now */
		tmp = icaltime_add(start, trigger.duration);
		event->alarm = icaltime_as_timet_with_zone(tmp, utc);
	}

	ao2_link(pvt->events, event);
	event = ast_calendar_unref_event(event);

	return;
}

 static void icalendar_update_events(struct icalendar_pvt *pvt)
{
	struct icaltimetype start_time, end_time;
	icalcomponent *iter;

	if (!pvt) {
		ast_log(LOG_ERROR, "iCalendar is NULL\n");
		return;
	}

	if (!pvt->owner) {
		ast_log(LOG_ERROR, "iCalendar is an orphan!\n");
		return;
	}

	if (!pvt->data) {
		ast_log(LOG_ERROR, "The iCalendar has not been parsed!\n");
		return;
	}

	start_time = icaltime_current_time_with_zone(icaltimezone_get_utc_timezone());
	end_time = icaltime_current_time_with_zone(icaltimezone_get_utc_timezone());
	end_time.second += pvt->owner->timeframe * 60;
	icaltime_normalize(end_time);

	for (iter = icalcomponent_get_first_component(pvt->data, ICAL_VEVENT_COMPONENT);
	     iter;
		 iter = icalcomponent_get_next_component(pvt->data, ICAL_VEVENT_COMPONENT))
	{
		icalcomponent_foreach_recurrence(iter, start_time, end_time, icalendar_add_event, pvt);
	}

	ast_calendar_merge_events(pvt->owner, pvt->events);
}

static void *ical_load_calendar(void *void_data)
{
	struct icalendar_pvt *pvt;
	struct ast_variable *v;
	struct ast_calendar *cal = void_data;
	ast_mutex_t refreshlock;

	if (!(cal && ast_calendar_config)) {
		ast_log(LOG_ERROR, "You must enable calendar support for res_icalendar to load\n");
		return NULL;
	}
	if (ao2_trylock(cal)) {
		if (cal->unloading) {
			ast_log(LOG_WARNING, "Unloading module, load_calendar cancelled.\n");
		} else {
			ast_log(LOG_WARNING, "Could not lock calendar, aborting!\n");
		}
		return NULL;
	}

	if (!(pvt = ao2_alloc(sizeof(*pvt), icalendar_destructor))) {
		ast_log(LOG_ERROR, "Could not allocate icalendar_pvt structure for calendar: %s\n", cal->name);
		return NULL;
	}

	pvt->owner = cal;

	if (!(pvt->events = ast_calendar_event_container_alloc())) {
		ast_log(LOG_ERROR, "Could not allocate space for fetching events for calendar: %s\n", cal->name);
		pvt = unref_icalendar(pvt);
		ao2_unlock(cal);
		return NULL;
	}

	if (ast_string_field_init(pvt, 32)) {
		ast_log(LOG_ERROR, "Couldn't allocate string field space for calendar: %s\n", cal->name);
		pvt = unref_icalendar(pvt);
		ao2_unlock(cal);
		return NULL;
	}

	for (v = ast_variable_browse(ast_calendar_config, cal->name); v; v = v->next) {
		if (!strcasecmp(v->name, "url")) {
			ast_string_field_set(pvt, url, v->value);
		} else if (!strcasecmp(v->name, "user")) {
			ast_string_field_set(pvt, user, v->value);
		} else if (!strcasecmp(v->name, "secret")) {
			ast_string_field_set(pvt, secret, v->value);
		}
	}

	if (ast_strlen_zero(pvt->url)) {
		ast_log(LOG_WARNING, "No URL was specified for iCalendar '%s' - skipping.\n", cal->name);
		pvt = unref_icalendar(pvt);
		ao2_unlock(cal);
		return NULL;
	}

	if (ne_uri_parse(pvt->url, &pvt->uri) || pvt->uri.host == NULL || pvt->uri.path == NULL) {
		ast_log(LOG_WARNING, "Could not parse url '%s' for iCalendar '%s' - skipping.\n", pvt->url, cal->name);
		pvt = unref_icalendar(pvt);
		ao2_unlock(cal);
		return NULL;
	}

	if (pvt->uri.scheme == NULL) {
		pvt->uri.scheme = "http";
	}

	if (pvt->uri.port == 0) {
		pvt->uri.port = ne_uri_defaultport(pvt->uri.scheme);
	}

	pvt->session = ne_session_create(pvt->uri.scheme, pvt->uri.host, pvt->uri.port);
	ne_set_server_auth(pvt->session, auth_credentials, pvt);
	if (!strncasecmp(pvt->uri.scheme, "https", sizeof(pvt->uri.scheme))) {
		ne_ssl_trust_default_ca(pvt->session);
	}

	cal->tech_pvt = pvt;

	ast_mutex_init(&refreshlock);

	/* Load it the first time */
	if (!(pvt->data = fetch_icalendar(pvt))) {
		ast_log(LOG_WARNING, "Unable to parse iCalendar '%s'\n", cal->name);
	}

	icalendar_update_events(pvt);

	ao2_unlock(cal);

	/* The only writing from another thread will be if unload is true */
	for(;;) {
		struct timeval tv = ast_tvnow();
		struct timespec ts = {0,};

		ts.tv_sec = tv.tv_sec + (60 * pvt->owner->refresh);

		ast_mutex_lock(&refreshlock);
		while (!pvt->owner->unloading) {
			if (ast_cond_timedwait(&pvt->owner->unload, &refreshlock, &ts) == ETIMEDOUT) {
				break;
			}
		}
		ast_mutex_unlock(&refreshlock);

		if (pvt->owner->unloading) {
			ast_debug(10, "Skipping refresh since we got a shutdown signal\n");
			return NULL;
		}

		ast_debug(10, "Refreshing after %d minute timeout\n", pvt->owner->refresh);

		if (!(pvt->data = fetch_icalendar(pvt))) {
			ast_log(LOG_WARNING, "Unable to parse iCalendar '%s'\n", pvt->owner->name);
			continue;
		}

		icalendar_update_events(pvt);
	}

	return NULL;
}

static int load_module(void)
{
	ne_sock_init();
	if (ast_calendar_register(&ical_tech)) {
		ne_sock_exit();
		return AST_MODULE_LOAD_DECLINE;
	}

	return AST_MODULE_LOAD_SUCCESS;
}

static int unload_module(void)
{
	ast_calendar_unregister(&ical_tech);
	ne_sock_exit();
	return 0;
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_DEFAULT, "Asterisk iCalendar .ics file integration",
		.load = load_module,
		.unload = unload_module,
	);
