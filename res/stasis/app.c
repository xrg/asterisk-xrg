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
 * \brief Stasis application support.
 *
 * \author David M. Lee, II <dlee@digium.com>
 */

#include "asterisk.h"

ASTERISK_FILE_VERSION(__FILE__, "$Revision$")

#include "app.h"

#include "asterisk/callerid.h"
#include "asterisk/stasis_app.h"
#include "asterisk/stasis_bridges.h"
#include "asterisk/stasis_channels.h"
#include "asterisk/stasis_endpoints.h"
#include "asterisk/stasis_message_router.h"

struct app {
	/*! Aggregation topic for this application. */
	struct stasis_topic *topic;
	/*! Router for handling messages forwarded to \a topic. */
	struct stasis_message_router *router;
	/*! Subscription to watch for bridge merge messages */
	struct stasis_subscription *bridge_merge_sub;
	/*! Container of the channel forwards to this app's topic. */
	struct ao2_container *forwards;
	/*! Callback function for this application. */
	stasis_app_cb handler;
	/*! Opaque data to hand to callback function. */
	void *data;
	/*! Name of the Stasis application */
	char name[];
};

enum forward_type {
	FORWARD_CHANNEL,
	FORWARD_BRIDGE,
	FORWARD_ENDPOINT,
};

/*! Subscription info for a particular channel/bridge. */
struct app_forwards {
	/*! Count of number of times this channel/bridge has been subscribed */
	int interested;

	/*! Forward for the regular topic */
	struct stasis_forward *topic_forward;
	/*! Forward for the caching topic */
	struct stasis_forward *topic_cached_forward;

	/* Type of object being forwarded */
	enum forward_type forward_type;
	/*! Unique id of the object being forwarded */
	char id[];
};

static void forwards_dtor(void *obj)
{
#ifdef AST_DEVMODE
	struct app_forwards *forwards = obj;
#endif /* AST_DEVMODE */

	ast_assert(forwards->topic_forward == NULL);
	ast_assert(forwards->topic_cached_forward == NULL);
}

static void forwards_unsubscribe(struct app_forwards *forwards)
{
	stasis_forward_cancel(forwards->topic_forward);
	forwards->topic_forward = NULL;
	stasis_forward_cancel(forwards->topic_cached_forward);
	forwards->topic_cached_forward = NULL;
}

static struct app_forwards *forwards_create(struct app *app,
	const char *id)
{
	RAII_VAR(struct app_forwards *, forwards, NULL, ao2_cleanup);

	if (!app || ast_strlen_zero(id)) {
		return NULL;
	}

	forwards = ao2_alloc(sizeof(*forwards) + strlen(id) + 1, forwards_dtor);
	if (!forwards) {
		return NULL;
	}

	strcpy(forwards->id, id);

	ao2_ref(forwards, +1);
	return forwards;
}

/*! Forward a channel's topics to an app */
static struct app_forwards *forwards_create_channel(struct app *app,
	struct ast_channel *chan)
{
	RAII_VAR(struct app_forwards *, forwards, NULL, ao2_cleanup);

	if (!app || !chan) {
		return NULL;
	}

	forwards = forwards_create(app, ast_channel_uniqueid(chan));
	if (!forwards) {
		return NULL;
	}

	forwards->forward_type = FORWARD_CHANNEL;
	forwards->topic_forward = stasis_forward_all(ast_channel_topic(chan),
		app->topic);
	if (!forwards->topic_forward) {
		return NULL;
	}

	forwards->topic_cached_forward = stasis_forward_all(
		ast_channel_topic_cached(chan), app->topic);
	if (!forwards->topic_cached_forward) {
		/* Half-subscribed is a bad thing */
		stasis_forward_cancel(forwards->topic_forward);
		forwards->topic_forward = NULL;
		return NULL;
	}

	ao2_ref(forwards, +1);
	return forwards;
}

/*! Forward a bridge's topics to an app */
static struct app_forwards *forwards_create_bridge(struct app *app,
	struct ast_bridge *bridge)
{
	RAII_VAR(struct app_forwards *, forwards, NULL, ao2_cleanup);

	if (!app || !bridge) {
		return NULL;
	}

	forwards = forwards_create(app, bridge->uniqueid);
	if (!forwards) {
		return NULL;
	}

	forwards->forward_type = FORWARD_BRIDGE;
	forwards->topic_forward = stasis_forward_all(ast_bridge_topic(bridge),
		app->topic);
	if (!forwards->topic_forward) {
		return NULL;
	}

	forwards->topic_cached_forward = stasis_forward_all(
		ast_bridge_topic_cached(bridge), app->topic);
	if (!forwards->topic_cached_forward) {
		/* Half-subscribed is a bad thing */
		stasis_forward_cancel(forwards->topic_forward);
		forwards->topic_forward = NULL;
		return NULL;
	}

	ao2_ref(forwards, +1);
	return forwards;
}

/*! Forward a endpoint's topics to an app */
static struct app_forwards *forwards_create_endpoint(struct app *app,
	struct ast_endpoint *endpoint)
{
	RAII_VAR(struct app_forwards *, forwards, NULL, ao2_cleanup);

	if (!app || !endpoint) {
		return NULL;
	}

	forwards = forwards_create(app, ast_endpoint_get_id(endpoint));
	if (!forwards) {
		return NULL;
	}

	forwards->forward_type = FORWARD_ENDPOINT;
	forwards->topic_forward = stasis_forward_all(ast_endpoint_topic(endpoint),
		app->topic);
	if (!forwards->topic_forward) {
		return NULL;
	}

	forwards->topic_cached_forward = stasis_forward_all(
		ast_endpoint_topic_cached(endpoint), app->topic);
	if (!forwards->topic_cached_forward) {
		/* Half-subscribed is a bad thing */
		stasis_forward_cancel(forwards->topic_forward);
		forwards->topic_forward = NULL;
		return NULL;
	}

	ao2_ref(forwards, +1);
	return forwards;
}

static int forwards_sort(const void *obj_left, const void *obj_right, int flags)
{
	const struct app_forwards *object_left = obj_left;
	const struct app_forwards *object_right = obj_right;
	const char *right_key = obj_right;
	int cmp;

	switch (flags & (OBJ_POINTER | OBJ_KEY | OBJ_PARTIAL_KEY)) {
	case OBJ_POINTER:
		right_key = object_right->id;
		/* Fall through */
	case OBJ_KEY:
		cmp = strcmp(object_left->id, right_key);
		break;
	case OBJ_PARTIAL_KEY:
		/*
		 * We could also use a partial key struct containing a length
		 * so strlen() does not get called for every comparison instead.
		 */
		cmp = strncmp(object_left->id, right_key, strlen(right_key));
		break;
	default:
		/* Sort can only work on something with a full or partial key. */
		ast_assert(0);
		cmp = 0;
		break;
	}
	return cmp;
}

static void app_dtor(void *obj)
{
	struct app *app = obj;

	ast_verb(1, "Destroying Stasis app %s\n", app->name);

	ast_assert(app->router == NULL);
	ast_assert(app->bridge_merge_sub == NULL);

	ao2_cleanup(app->topic);
	app->topic = NULL;
	ao2_cleanup(app->forwards);
	app->forwards = NULL;
	ao2_cleanup(app->data);
	app->data = NULL;
}

static void sub_default_handler(void *data, struct stasis_subscription *sub,
	struct stasis_message *message)
{
	struct app *app = data;
	RAII_VAR(struct ast_json *, json, NULL, ast_json_unref);

	if (stasis_subscription_final_message(sub, message)) {
		ao2_cleanup(app);
	}

	/* By default, send any message that has a JSON representation */
	json = stasis_message_to_json(message, stasis_app_get_sanitizer());
	if (!json) {
		return;
	}

	app_send(app, json);
}

/*! \brief Typedef for callbacks that get called on channel snapshot updates */
typedef struct ast_json *(*channel_snapshot_monitor)(
	struct ast_channel_snapshot *old_snapshot,
	struct ast_channel_snapshot *new_snapshot,
	const struct timeval *tv);

static struct ast_json *simple_channel_event(
	const char *type,
	struct ast_channel_snapshot *snapshot,
	const struct timeval *tv)
{
	struct ast_json *json_channel = ast_channel_snapshot_to_json(snapshot, stasis_app_get_sanitizer());

	if (!json_channel) {
		return NULL;
	}

	return ast_json_pack("{s: s, s: o, s: o}",
		"type", type,
		"timestamp", ast_json_timeval(*tv, NULL),
		"channel", json_channel);
}

static struct ast_json *channel_created_event(
	struct ast_channel_snapshot *snapshot,
	const struct timeval *tv)
{
	return simple_channel_event("ChannelCreated", snapshot, tv);
}

static struct ast_json *channel_destroyed_event(
	struct ast_channel_snapshot *snapshot,
	const struct timeval *tv)
{
	struct ast_json *json_channel = ast_channel_snapshot_to_json(snapshot, stasis_app_get_sanitizer());

	if (!json_channel) {
		return NULL;
	}

	return ast_json_pack("{s: s, s: o, s: i, s: s, s: o}",
		"type", "ChannelDestroyed",
		"timestamp", ast_json_timeval(*tv, NULL),
		"cause", snapshot->hangupcause,
		"cause_txt", ast_cause2str(snapshot->hangupcause),
		"channel", json_channel);
}

static struct ast_json *channel_state_change_event(
	struct ast_channel_snapshot *snapshot,
	const struct timeval *tv)
{
	return simple_channel_event("ChannelStateChange", snapshot, tv);
}

/*! \brief Handle channel state changes */
static struct ast_json *channel_state(
	struct ast_channel_snapshot *old_snapshot,
	struct ast_channel_snapshot *new_snapshot,
	const struct timeval *tv)
{
	struct ast_channel_snapshot *snapshot = new_snapshot ?
		new_snapshot : old_snapshot;

	if (!old_snapshot) {
		return channel_created_event(snapshot, tv);
	} else if (!new_snapshot) {
		return channel_destroyed_event(snapshot, tv);
	} else if (old_snapshot->state != new_snapshot->state) {
		return channel_state_change_event(snapshot, tv);
	}

	return NULL;
}

static struct ast_json *channel_dialplan(
	struct ast_channel_snapshot *old_snapshot,
	struct ast_channel_snapshot *new_snapshot,
	const struct timeval *tv)
{
	RAII_VAR(struct ast_json *, json, NULL, ast_json_unref);
	struct ast_json *json_channel;

	/* No Newexten event on cache clear or first event */
	if (!old_snapshot || !new_snapshot) {
		return NULL;
	}

	/* Empty application is not valid for a Newexten event */
	if (ast_strlen_zero(new_snapshot->appl)) {
		return NULL;
	}

	if (ast_channel_snapshot_cep_equal(old_snapshot, new_snapshot)) {
		return NULL;
	}

	json_channel = ast_channel_snapshot_to_json(new_snapshot, stasis_app_get_sanitizer());
	if (!json_channel) {
		return NULL;
	}

	return ast_json_pack("{s: s, s: o, s: s, s: s, s: o}",
		"type", "ChannelDialplan",
		"timestamp", ast_json_timeval(*tv, NULL),
		"dialplan_app", new_snapshot->appl,
		"dialplan_app_data", new_snapshot->data,
		"channel", json_channel);
}

static struct ast_json *channel_callerid(
	struct ast_channel_snapshot *old_snapshot,
	struct ast_channel_snapshot *new_snapshot,
	const struct timeval *tv)
{
	RAII_VAR(struct ast_json *, json, NULL, ast_json_unref);
	struct ast_json *json_channel;

	/* No NewCallerid event on cache clear or first event */
	if (!old_snapshot || !new_snapshot) {
		return NULL;
	}

	if (ast_channel_snapshot_caller_id_equal(old_snapshot, new_snapshot)) {
		return NULL;
	}

	json_channel = ast_channel_snapshot_to_json(new_snapshot, stasis_app_get_sanitizer());
	if (!json_channel) {
		return NULL;
	}

	return ast_json_pack("{s: s, s: o, s: i, s: s, s: o}",
		"type", "ChannelCallerId",
		"timestamp", ast_json_timeval(*tv, NULL),
		"caller_presentation", new_snapshot->caller_pres,
		"caller_presentation_txt", ast_describe_caller_presentation(
			new_snapshot->caller_pres),
		"channel", json_channel);
}

static channel_snapshot_monitor channel_monitors[] = {
	channel_state,
	channel_dialplan,
	channel_callerid
};

static void sub_channel_update_handler(void *data,
	struct stasis_subscription *sub,
	struct stasis_message *message)
{
	struct app *app = data;
	struct stasis_cache_update *update;
	struct ast_channel_snapshot *new_snapshot;
	struct ast_channel_snapshot *old_snapshot;
	const struct timeval *tv;
	int i;

	ast_assert(stasis_message_type(message) == stasis_cache_update_type());

	update = stasis_message_data(message);

	ast_assert(update->type == ast_channel_snapshot_type());

	new_snapshot = stasis_message_data(update->new_snapshot);
	old_snapshot = stasis_message_data(update->old_snapshot);

	/* Pull timestamp from the new snapshot, or from the update message
	 * when there isn't one. */
	tv = update->new_snapshot ?
		stasis_message_timestamp(update->new_snapshot) :
		stasis_message_timestamp(message);

	for (i = 0; i < ARRAY_LEN(channel_monitors); ++i) {
		RAII_VAR(struct ast_json *, msg, NULL, ast_json_unref);

		msg = channel_monitors[i](old_snapshot, new_snapshot, tv);
		if (msg) {
			app_send(app, msg);
		}
	}
}

static struct ast_json *simple_endpoint_event(
	const char *type,
	struct ast_endpoint_snapshot *snapshot,
	const struct timeval *tv)
{
	struct ast_json *json_endpoint = ast_endpoint_snapshot_to_json(snapshot, stasis_app_get_sanitizer());

	if (!json_endpoint) {
		return NULL;
	}

	return ast_json_pack("{s: s, s: o, s: o}",
		"type", type,
		"timestamp", ast_json_timeval(*tv, NULL),
		"endpoint", json_endpoint);
}

static void sub_endpoint_update_handler(void *data,
	struct stasis_subscription *sub,
	struct stasis_message *message)
{
	RAII_VAR(struct ast_json *, json, NULL, ast_json_unref);
	struct app *app = data;
	struct stasis_cache_update *update;
	struct ast_endpoint_snapshot *new_snapshot;
	const struct timeval *tv;

	ast_assert(stasis_message_type(message) == stasis_cache_update_type());

	update = stasis_message_data(message);

	ast_assert(update->type == ast_endpoint_snapshot_type());

	new_snapshot = stasis_message_data(update->new_snapshot);
	tv = update->new_snapshot ?
		stasis_message_timestamp(update->new_snapshot) :
		stasis_message_timestamp(message);

	json = simple_endpoint_event("EndpointStateChange", new_snapshot, tv);

	if (!json) {
		return;
	}

	app_send(app, json);
}

static struct ast_json *simple_bridge_event(
	const char *type,
	struct ast_bridge_snapshot *snapshot,
	const struct timeval *tv)
{
	struct ast_json *json_bridge = ast_bridge_snapshot_to_json(snapshot, stasis_app_get_sanitizer());
	if (!json_bridge) {
		return NULL;
	}

	return ast_json_pack("{s: s, s: o, s: o}",
		"type", type,
		"timestamp", ast_json_timeval(*tv, NULL),
		"bridge", json_bridge);
}

static void sub_bridge_update_handler(void *data,
	struct stasis_subscription *sub,
	struct stasis_message *message)
{
	RAII_VAR(struct ast_json *, json, NULL, ast_json_unref);
	struct app *app = data;
	struct stasis_cache_update *update;
	struct ast_bridge_snapshot *new_snapshot;
	struct ast_bridge_snapshot *old_snapshot;
	const struct timeval *tv;

	ast_assert(stasis_message_type(message) == stasis_cache_update_type());

	update = stasis_message_data(message);

	ast_assert(update->type == ast_bridge_snapshot_type());

	new_snapshot = stasis_message_data(update->new_snapshot);
	old_snapshot = stasis_message_data(update->old_snapshot);
	tv = update->new_snapshot ?
		stasis_message_timestamp(update->new_snapshot) :
		stasis_message_timestamp(message);

	if (!new_snapshot) {
		json = simple_bridge_event("BridgeDestroyed", old_snapshot, tv);
	} else if (!old_snapshot) {
		json = simple_bridge_event("BridgeCreated", new_snapshot, tv);
	}

	if (!json) {
		return;
	}

	app_send(app, json);
}

static void bridge_merge_handler(void *data, struct stasis_subscription *sub,
	struct stasis_message *message)
{
	struct app *app = data;
	struct ast_bridge_merge_message *merge;
	RAII_VAR(struct app_forwards *, forwards, NULL, ao2_cleanup);

	if (stasis_subscription_final_message(sub, message)) {
		ao2_cleanup(app);
	}

	if (stasis_message_type(message) != ast_bridge_merge_message_type()) {
		return;
	}

	merge = stasis_message_data(message);

	/* Find out if we're subscribed to either bridge */
	forwards = ao2_find(app->forwards, merge->from->uniqueid,
		OBJ_SEARCH_KEY);
	if (!forwards) {
		forwards = ao2_find(app->forwards, merge->to->uniqueid,
			OBJ_SEARCH_KEY);
	}

	if (!forwards) {
		return;
	}

	/* Forward the message to the app */
	stasis_publish(app->topic, message);
}

struct app *app_create(const char *name, stasis_app_cb handler, void *data)
{
	RAII_VAR(struct app *, app, NULL, ao2_cleanup);
	size_t size;
	int res = 0;

	ast_assert(name != NULL);
	ast_assert(handler != NULL);

	ast_verb(1, "Creating Stasis app '%s'\n", name);

	size = sizeof(*app) + strlen(name) + 1;
	app = ao2_alloc_options(size, app_dtor, AO2_ALLOC_OPT_LOCK_MUTEX);

	if (!app) {
		return NULL;
	}

	app->forwards = ao2_container_alloc_rbtree(AO2_ALLOC_OPT_LOCK_MUTEX,
		AO2_CONTAINER_ALLOC_OPT_DUPS_OBJ_REJECT,
		forwards_sort, NULL);
	if (!app->forwards) {
		return NULL;
	}

	app->topic = stasis_topic_create(name);
	if (!app->topic) {
		return NULL;
	}

	app->bridge_merge_sub = stasis_subscribe(ast_bridge_topic_all(),
		bridge_merge_handler, app);
	if (!app->bridge_merge_sub) {
		return NULL;
	}
	/* Subscription holds a reference */
	ao2_ref(app, +1);

	app->router = stasis_message_router_create(app->topic);
	if (!app->router) {
		return NULL;
	}

	res |= stasis_message_router_add_cache_update(app->router,
		ast_bridge_snapshot_type(), sub_bridge_update_handler, app);

	res |= stasis_message_router_add_cache_update(app->router,
		ast_channel_snapshot_type(), sub_channel_update_handler, app);

	res |= stasis_message_router_add_cache_update(app->router,
		ast_endpoint_snapshot_type(), sub_endpoint_update_handler, app);

	res |= stasis_message_router_set_default(app->router,
		sub_default_handler, app);

	if (res != 0) {
		return NULL;
	}
	/* Router holds a reference */
	ao2_ref(app, +1);

	strncpy(app->name, name, size - sizeof(*app));
	app->handler = handler;
	ao2_ref(data, +1);
	app->data = data;

	ao2_ref(app, +1);
	return app;
}

/*!
 * \brief Send a message to the given application.
 * \param app App to send the message to.
 * \param message Message to send.
 */
void app_send(struct app *app, struct ast_json *message)
{
	stasis_app_cb handler;
	RAII_VAR(void *, data, NULL, ao2_cleanup);

	/* Copy off mutable state with lock held */
	{
		SCOPED_AO2LOCK(lock, app);
		handler = app->handler;
		if (app->data) {
			ao2_ref(app->data, +1);
			data = app->data;
		}
		/* Name is immutable; no need to copy */
	}

	if (!handler) {
		ast_verb(3,
			"Inactive Stasis app '%s' missed message\n", app->name);
		return;
	}

	handler(data, app->name, message);
}

void app_deactivate(struct app *app)
{
	SCOPED_AO2LOCK(lock, app);
	ast_verb(1, "Deactivating Stasis app '%s'\n", app->name);
	app->handler = NULL;
	ao2_cleanup(app->data);
	app->data = NULL;
}

void app_shutdown(struct app *app)
{
	SCOPED_AO2LOCK(lock, app);

	ast_assert(app_is_finished(app));

	stasis_message_router_unsubscribe(app->router);
	app->router = NULL;
	stasis_unsubscribe(app->bridge_merge_sub);
	app->bridge_merge_sub = NULL;
}

int app_is_active(struct app *app)
{
	SCOPED_AO2LOCK(lock, app);
	return app->handler != NULL;
}

int app_is_finished(struct app *app)
{
	SCOPED_AO2LOCK(lock, app);

	return app->handler == NULL && ao2_container_count(app->forwards) == 0;
}

void app_update(struct app *app, stasis_app_cb handler, void *data)
{
	SCOPED_AO2LOCK(lock, app);

	if (app->handler) {
		RAII_VAR(struct ast_json *, msg, NULL, ast_json_unref);

		ast_verb(1, "Replacing Stasis app '%s'\n", app->name);

		msg = ast_json_pack("{s: s, s: s}",
			"type", "ApplicationReplaced",
			"application", app->name);
		if (msg) {
			app_send(app, msg);
		}
	} else {
		ast_verb(1, "Activating Stasis app '%s'\n", app->name);
	}

	app->handler = handler;
	ao2_cleanup(app->data);
	if (data) {
		ao2_ref(data, +1);
	}
	app->data = data;
}

const char *app_name(const struct app *app)
{
	return app->name;
}

struct ast_json *app_to_json(const struct app *app)
{
	RAII_VAR(struct ast_json *, json, NULL, ast_json_unref);
	struct ast_json *channels;
	struct ast_json *bridges;
	struct ast_json *endpoints;
	struct ao2_iterator i;
	void *obj;

	json = ast_json_pack("{s: s, s: [], s: [], s: []}",
		"name", app->name,
		"channel_ids", "bridge_ids", "endpoint_ids");
	channels = ast_json_object_get(json, "channel_ids");
	bridges = ast_json_object_get(json, "bridge_ids");
	endpoints = ast_json_object_get(json, "endpoint_ids");

	i = ao2_iterator_init(app->forwards, 0);
	while ((obj = ao2_iterator_next(&i))) {
		RAII_VAR(struct app_forwards *, forwards, obj, ao2_cleanup);
		RAII_VAR(struct ast_json *, id, NULL, ast_json_unref);
		int append_res = -1;

		id = ast_json_string_create(forwards->id);

		switch (forwards->forward_type) {
		case FORWARD_CHANNEL:
			append_res = ast_json_array_append(channels,
				ast_json_ref(id));
			break;
		case FORWARD_BRIDGE:
			append_res = ast_json_array_append(bridges,
				ast_json_ref(id));
			break;
		case FORWARD_ENDPOINT:
			append_res = ast_json_array_append(endpoints,
				ast_json_ref(id));
			break;
		}

		if (append_res != 0) {
			ast_log(LOG_ERROR, "Error building response\n");
			ao2_iterator_destroy(&i);
			return NULL;
		}
	}
	ao2_iterator_destroy(&i);

	return ast_json_ref(json);
}

int app_subscribe_channel(struct app *app, struct ast_channel *chan)
{
	int res;

	if (!app || !chan) {
		return -1;
	} else {
		RAII_VAR(struct app_forwards *, forwards, NULL, ao2_cleanup);
		SCOPED_AO2LOCK(lock, app->forwards);

		forwards = ao2_find(app->forwards, ast_channel_uniqueid(chan),
			OBJ_SEARCH_KEY | OBJ_NOLOCK);
		if (!forwards) {
			/* Forwards not found, create one */
			forwards = forwards_create_channel(app, chan);
			if (!forwards) {
				return -1;
			}

			res = ao2_link_flags(app->forwards, forwards,
				OBJ_NOLOCK);
			if (!res) {
				return -1;
			}
		}

		++forwards->interested;
		return 0;
	}
}

static int unsubscribe(struct app *app, const char *kind, const char *id)
{
	RAII_VAR(struct app_forwards *, forwards, NULL, ao2_cleanup);
	SCOPED_AO2LOCK(lock, app->forwards);

	forwards = ao2_find(app->forwards, id, OBJ_SEARCH_KEY | OBJ_NOLOCK);
	if (!forwards) {
		ast_log(LOG_WARNING,
			"App '%s' not subscribed to %s '%s'\n",
			app->name, kind, id);
		return -1;
	}

	if (--forwards->interested == 0) {
		/* No one is interested any more; unsubscribe */
		forwards_unsubscribe(forwards);
		ao2_find(app->forwards, forwards,
			OBJ_POINTER | OBJ_NOLOCK | OBJ_UNLINK |
			OBJ_NODATA);
	}

	return 0;
}

int app_unsubscribe_channel(struct app *app, struct ast_channel *chan)
{
	if (!app || !chan) {
		return -1;
	}

	return app_unsubscribe_channel_id(app, ast_channel_uniqueid(chan));
}

int app_unsubscribe_channel_id(struct app *app, const char *channel_id)
{
	if (!app || !channel_id) {
		return -1;
	}

	return unsubscribe(app, "channel", channel_id);
}

int app_is_subscribed_channel_id(struct app *app, const char *channel_id)
{
	RAII_VAR(struct app_forwards *, forwards, NULL, ao2_cleanup);
	forwards = ao2_find(app->forwards, channel_id, OBJ_SEARCH_KEY);
	return forwards != NULL;
}

int app_subscribe_bridge(struct app *app, struct ast_bridge *bridge)
{
	if (!app || !bridge) {
		return -1;
	} else {
		RAII_VAR(struct app_forwards *, forwards, NULL, ao2_cleanup);
		SCOPED_AO2LOCK(lock, app->forwards);

		forwards = ao2_find(app->forwards, bridge->uniqueid,
			OBJ_SEARCH_KEY | OBJ_NOLOCK);

		if (!forwards) {
			/* Forwards not found, create one */
			forwards = forwards_create_bridge(app, bridge);
			if (!forwards) {
				return -1;
			}
			ao2_link_flags(app->forwards, forwards, OBJ_NOLOCK);
		}

		++forwards->interested;
		return 0;
	}
}

int app_unsubscribe_bridge(struct app *app, struct ast_bridge *bridge)
{
	if (!app || !bridge) {
		return -1;
	}

	return app_unsubscribe_bridge_id(app, bridge->uniqueid);
}

int app_unsubscribe_bridge_id(struct app *app, const char *bridge_id)
{
	if (!app || !bridge_id) {
		return -1;
	}

	return unsubscribe(app, "bridge", bridge_id);
}

int app_is_subscribed_bridge_id(struct app *app, const char *bridge_id)
{
	RAII_VAR(struct app_forwards *, forwards, NULL, ao2_cleanup);
	forwards = ao2_find(app->forwards, bridge_id, OBJ_SEARCH_KEY);
	return forwards != NULL;
}

int app_subscribe_endpoint(struct app *app, struct ast_endpoint *endpoint)
{
	if (!app || !endpoint) {
		return -1;
	} else {
		RAII_VAR(struct app_forwards *, forwards, NULL, ao2_cleanup);
		SCOPED_AO2LOCK(lock, app->forwards);

		forwards = ao2_find(app->forwards, ast_endpoint_get_id(endpoint),
			OBJ_SEARCH_KEY | OBJ_NOLOCK);

		if (!forwards) {
			/* Forwards not found, create one */
			forwards = forwards_create_endpoint(app, endpoint);
			if (!forwards) {
				return -1;
			}
			ao2_link_flags(app->forwards, forwards, OBJ_NOLOCK);
		}

		++forwards->interested;
		return 0;
	}
}

int app_unsubscribe_endpoint_id(struct app *app, const char *endpoint_id)
{
	if (!app || !endpoint_id) {
		return -1;
	}

	return unsubscribe(app, "endpoint", endpoint_id);
}

int app_is_subscribed_endpoint_id(struct app *app, const char *endpoint_id)
{
	RAII_VAR(struct app_forwards *, forwards, NULL, ao2_cleanup);
	forwards = ao2_find(app->forwards, endpoint_id, OBJ_SEARCH_KEY);
	return forwards != NULL;
}
