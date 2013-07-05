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
 * \brief /api-docs/events.{format} implementation- WebSocket resource
 *
 * \author David M. Lee, II <dlee@digium.com>
 */

#include "asterisk.h"

ASTERISK_FILE_VERSION(__FILE__, "$Revision$")

#include "asterisk/astobj2.h"
#include "asterisk/stasis_app.h"
#include "resource_events.h"

/*! Number of buckets for the Stasis application hash table. Remember to keep it
 *  a prime number!
 */
#define APPS_NUM_BUCKETS 7

/*! \brief A connection to the event WebSocket */
struct event_session {
	struct ari_websocket_session *ws_session;
	struct ao2_container *websocket_apps;
};

/*!
 * \brief Explicitly shutdown a session.
 *
 * An explicit shutdown is necessary, since stasis-app has a reference to this
 * session. We also need to be sure to null out the \c ws_session field, since
 * the websocket is about to go away.
 *
 * \param session Session info struct.
 */
static void session_shutdown(struct event_session *session)
{
        struct ao2_iterator i;
	char *app;
	SCOPED_AO2LOCK(lock, session);

	i = ao2_iterator_init(session->websocket_apps, 0);
	while ((app = ao2_iterator_next(&i))) {
		stasis_app_unregister(app);
		ao2_cleanup(app);
	}
	ao2_iterator_destroy(&i);
	ao2_cleanup(session->websocket_apps);

	session->websocket_apps = NULL;
	session->ws_session = NULL;
}

static void session_dtor(void *obj)
{
#ifdef AST_DEVMODE /* Avoid unused variable warning */
	struct event_session *session = obj;
#endif

	/* session_shutdown should have been called before */
	ast_assert(session->ws_session == NULL);
	ast_assert(session->websocket_apps == NULL);
}

static void session_cleanup(struct event_session *session)
{
	session_shutdown(session);
	ao2_cleanup(session);
}

static struct event_session *session_create(
	struct ari_websocket_session *ws_session)
{
	RAII_VAR(struct event_session *, session, NULL, ao2_cleanup);

	session = ao2_alloc(sizeof(*session), session_dtor);

	session->ws_session = ws_session;
	session->websocket_apps =
		ast_str_container_alloc(APPS_NUM_BUCKETS);

	if (!session->websocket_apps) {
		return NULL;
	}

	ao2_ref(session, +1);
	return session;
}

/*!
 * \brief Callback handler for Stasis application messages.
 */
static void app_handler(void *data, const char *app_name,
			struct ast_json *message)
{
	struct event_session *session = data;
	int res;

	res = ast_json_object_set(message, "application",
				  ast_json_string_create(app_name));
	if(res != 0) {
		return;
	}

	ao2_lock(session);
	if (session->ws_session) {
		ari_websocket_session_write(session->ws_session, message);
	}
	ao2_unlock(session);
}

/*!
 * \brief Register for all of the apps given.
 * \param session Session info struct.
 * \param app_list Comma seperated list of app names to register.
 */
static int session_register_apps(struct event_session *session,
				 const char *app_list)
{
	RAII_VAR(char *, to_free, NULL, ast_free);
	char *apps, *app_name;
	SCOPED_AO2LOCK(lock, session);

	ast_assert(session->ws_session != NULL);
	ast_assert(session->websocket_apps != NULL);

	if (!app_list) {
		return -1;
	}

	to_free = apps = ast_strdup(app_list);
	if (!apps) {
		ari_websocket_session_write(session->ws_session, ari_oom_json());
		return -1;
	}
	while ((app_name = strsep(&apps, ","))) {
		if (ast_str_container_add(session->websocket_apps, app_name)) {
			ari_websocket_session_write(session->ws_session, ari_oom_json());
			return -1;
		}

		stasis_app_register(app_name, app_handler, session);
	}
	return 0;
}

void ari_websocket_event_websocket(struct ari_websocket_session *ws_session,
	struct ast_variable *headers,
	struct ast_event_websocket_args *args)
{
	RAII_VAR(struct event_session *, session, NULL, session_cleanup);
	struct ast_json *msg;
	int res;

	ast_debug(3, "/events WebSocket connection\n");

	session = session_create(ws_session);
	if (!session) {
		ari_websocket_session_write(ws_session, ari_oom_json());
		return;
	}

	if (!args->app) {
		RAII_VAR(struct ast_json *, msg, NULL, ast_json_unref);

		msg = ast_json_pack("{s: s, s: [s]}",
			"type", "MissingParams",
			"params", "app");
		if (!msg) {
			msg = ast_json_ref(ari_oom_json());
		}

		ari_websocket_session_write(session->ws_session, msg);
		return;
	}

	res = session_register_apps(session, args->app);
	if (res != 0) {
		ari_websocket_session_write(ws_session, ari_oom_json());
		return;
	}

	/* We don't process any input, but we'll consume it waiting for EOF */
	while ((msg = ari_websocket_session_read(ws_session))) {
		ast_json_unref(msg);
	}
}
