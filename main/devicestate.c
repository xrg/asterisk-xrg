/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 1999 - 2008, Digium, Inc.
 *
 * Mark Spencer <markster@digium.com>
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
 * \brief Device state management
 *
 * \author Mark Spencer <markster@digium.com>
 * \author Russell Bryant <russell@digium.com>
 *
 *	\arg \ref AstExtState
 */

/*! \page AstExtState Extension and device states in Asterisk
 *
 * (Note that these descriptions of device states and extension
 * states have not been updated to the way things work
 * in Asterisk 1.6.)
 *
 *	Asterisk has an internal system that reports states
 *	for an extension. By using the dialplan priority -1,
 *	also called a \b hint, a connection can be made from an
 *	extension to one or many devices. The state of the extension
 *	now depends on the combined state of the devices.
 *
 *	The device state is basically based on the current calls.
 *	If the devicestate engine can find a call from or to the
 *	device, it's in use.
 *
 *	Some channel drivers implement a callback function for
 *	a better level of reporting device states. The SIP channel
 *	has a complicated system for this, which is improved
 *	by adding call limits to the configuration.
 *
 *	Functions that want to check the status of an extension
 *	register themself as a \b watcher.
 *	Watchers in this system can subscribe either to all extensions
 *	or just a specific extensions.
 *
 *	For non-device related states, there's an API called
 *	devicestate providers. This is an extendible system for
 *	delivering state information from outside sources or
 *	functions within Asterisk. Currently we have providers
 *	for app_meetme.c - the conference bridge - and call
 *	parking (metermaids).
 *
 *	There are manly three subscribers to extension states
 *	within Asterisk:
 *	- AMI, the manager interface
 *	- app_queue.c - the Queue dialplan application
 *	- SIP subscriptions, a.k.a. "blinking lamps" or
 *	  "buddy lists"
 *
 *	The CLI command "show hints" show last known state
 *
 *	\note None of these handle user states, like an IM presence
 *	system. res_xmpp.c can subscribe and watch such states
 *	in jabber/xmpp based systems.
 *
 *	\section AstDevStateArch Architecture for devicestates
 *
 *	When a channel driver or asterisk app changes state for
 *	a watched object, it alerts the core. The core queues
 *	a change. When the change is processed, there's a query
 *	sent to the channel driver/provider if there's a function
 *	to handle that, otherwise a channel walk is issued to find
 *	a channel that involves the object.
 *
 *	The changes are queued and processed by a separate thread.
 *	This thread calls the watchers subscribing to status
 *	changes for the object. For manager, this results
 *	in events. For SIP, NOTIFY requests.
 *
 *	- Device states
 *		\arg \ref devicestate.c
 *		\arg \ref devicestate.h
 *
 *	\section AstExtStateArch Architecture for extension states
 *
 *	Hints are connected to extension. If an extension changes state
 *	it checks the hint devices. If there is a hint, the callbacks into
 *	device states are checked. The aggregated state is set for the hint
 *	and reported back.
 *
 *	- Extension states
 *		\arg \ref AstENUM ast_extension_states
 *		\arg \ref pbx.c
 *		\arg \ref pbx.h
 *	- Structures
 *		- \ref ast_state_cb struct.  Callbacks for watchers
 *		- Callback ast_state_cb_type
 *		- \ref ast_hint struct.
 *	- Functions
 *		- ast_extension_state_add()
 *		- ast_extension_state_del()
 *		- ast_get_hint()
 *
 */

/*** MODULEINFO
	<support_level>core</support_level>
 ***/

#include "asterisk.h"

ASTERISK_FILE_VERSION(__FILE__, "$Revision$")

#include "asterisk/_private.h"
#include "asterisk/channel.h"
#include "asterisk/utils.h"
#include "asterisk/lock.h"
#include "asterisk/linkedlists.h"
#include "asterisk/devicestate.h"
#include "asterisk/pbx.h"
#include "asterisk/app.h"
#include "asterisk/astobj2.h"
#include "asterisk/stasis.h"
#include "asterisk/devicestate.h"

#define DEVSTATE_TOPIC_BUCKETS 57

/*! \brief Device state strings for printing */
static const char * const devstatestring[][2] = {
	{ /* 0 AST_DEVICE_UNKNOWN */     "Unknown",     "UNKNOWN"     }, /*!< Valid, but unknown state */
	{ /* 1 AST_DEVICE_NOT_INUSE */   "Not in use",  "NOT_INUSE"   }, /*!< Not used */
	{ /* 2 AST_DEVICE IN USE */      "In use",      "INUSE"       }, /*!< In use */
	{ /* 3 AST_DEVICE_BUSY */        "Busy",        "BUSY"        }, /*!< Busy */
	{ /* 4 AST_DEVICE_INVALID */     "Invalid",     "INVALID"     }, /*!< Invalid - not known to Asterisk */
	{ /* 5 AST_DEVICE_UNAVAILABLE */ "Unavailable", "UNAVAILABLE" }, /*!< Unavailable (not registered) */
	{ /* 6 AST_DEVICE_RINGING */     "Ringing",     "RINGING"     }, /*!< Ring, ring, ring */
	{ /* 7 AST_DEVICE_RINGINUSE */   "Ring+Inuse",  "RINGINUSE"   }, /*!< Ring and in use */
	{ /* 8 AST_DEVICE_ONHOLD */      "On Hold",      "ONHOLD"      }, /*!< On Hold */
};

/*!\brief Mapping for channel states to device states */
static const struct chan2dev {
	enum ast_channel_state chan;
	enum ast_device_state dev;
} chan2dev[] = {
	{ AST_STATE_DOWN,            AST_DEVICE_NOT_INUSE },
	{ AST_STATE_RESERVED,        AST_DEVICE_INUSE },
	{ AST_STATE_OFFHOOK,         AST_DEVICE_INUSE },
	{ AST_STATE_DIALING,         AST_DEVICE_INUSE },
	{ AST_STATE_RING,            AST_DEVICE_INUSE },
	{ AST_STATE_RINGING,         AST_DEVICE_RINGING },
	{ AST_STATE_UP,              AST_DEVICE_INUSE },
	{ AST_STATE_BUSY,            AST_DEVICE_BUSY },
	{ AST_STATE_DIALING_OFFHOOK, AST_DEVICE_INUSE },
	{ AST_STATE_PRERING,         AST_DEVICE_RINGING },
	{ -100,                      -100 },
};

/*! \brief  A device state provider (not a channel) */
struct devstate_prov {
	char label[40];
	ast_devstate_prov_cb_type callback;
	AST_RWLIST_ENTRY(devstate_prov) list;
};

/*! \brief A list of providers */
static AST_RWLIST_HEAD_STATIC(devstate_provs, devstate_prov);

struct state_change {
	AST_LIST_ENTRY(state_change) list;
	enum ast_devstate_cache cachable;
	char device[1];
};

/*! \brief The state change queue. State changes are queued
	for processing by a separate thread */
static AST_LIST_HEAD_STATIC(state_changes, state_change);

/*! \brief The device state change notification thread */
static pthread_t change_thread = AST_PTHREADT_NULL;

/*! \brief Flag for the queue */
static ast_cond_t change_pending;

struct stasis_subscription *devstate_message_sub;

static struct stasis_topic *device_state_topic_all;
static struct stasis_cache *device_state_cache;
static struct stasis_caching_topic *device_state_topic_cached;
static struct stasis_topic_pool *device_state_topic_pool;

STASIS_MESSAGE_TYPE_DEFN(ast_device_state_message_type);

/* Forward declarations */
static int getproviderstate(const char *provider, const char *address);

/*! \brief Find devicestate as text message for output */
const char *ast_devstate2str(enum ast_device_state devstate)
{
	return devstatestring[devstate][0];
}

/* Deprecated interface (not prefixed with ast_) */
const char *devstate2str(enum ast_device_state devstate)
{
	return devstatestring[devstate][0];
}

enum ast_device_state ast_state_chan2dev(enum ast_channel_state chanstate)
{
	int i;
	chanstate &= 0xFFFF;
	for (i = 0; chan2dev[i].chan != -100; i++) {
		if (chan2dev[i].chan == chanstate) {
			return chan2dev[i].dev;
		}
	}
	return AST_DEVICE_UNKNOWN;
}

/* Parseable */
const char *ast_devstate_str(enum ast_device_state state)
{
	return devstatestring[state][1];
}

enum ast_device_state ast_devstate_val(const char *val)
{
	if (!strcasecmp(val, "NOT_INUSE"))
		return AST_DEVICE_NOT_INUSE;
	else if (!strcasecmp(val, "INUSE"))
		return AST_DEVICE_INUSE;
	else if (!strcasecmp(val, "BUSY"))
		return AST_DEVICE_BUSY;
	else if (!strcasecmp(val, "INVALID"))
		return AST_DEVICE_INVALID;
	else if (!strcasecmp(val, "UNAVAILABLE"))
		return AST_DEVICE_UNAVAILABLE;
	else if (!strcasecmp(val, "RINGING"))
		return AST_DEVICE_RINGING;
	else if (!strcasecmp(val, "RINGINUSE"))
		return AST_DEVICE_RINGINUSE;
	else if (!strcasecmp(val, "ONHOLD"))
		return AST_DEVICE_ONHOLD;

	return AST_DEVICE_UNKNOWN;
}

/*! \brief Find out if device is active in a call or not
	\note find channels with the device's name in it
	This function is only used for channels that does not implement
	devicestate natively
*/
enum ast_device_state ast_parse_device_state(const char *device)
{
	struct ast_channel *chan;
	char match[AST_CHANNEL_NAME];
	enum ast_device_state res;

	snprintf(match, sizeof(match), "%s-", device);

	if (!(chan = ast_channel_get_by_name_prefix(match, strlen(match)))) {
		return AST_DEVICE_UNKNOWN;
	}

	res = (ast_channel_state(chan) == AST_STATE_RINGING) ? AST_DEVICE_RINGING : AST_DEVICE_INUSE;

	chan = ast_channel_unref(chan);

	return res;
}

static enum ast_device_state devstate_cached(const char *device)
{
	RAII_VAR(struct stasis_message *, cached_msg, NULL, ao2_cleanup);
	struct ast_device_state_message *device_state;

	cached_msg = stasis_cache_get(ast_device_state_cache(), ast_device_state_message_type(), device);
	if (!cached_msg) {
		return AST_DEVICE_UNKNOWN;
	}
	device_state = stasis_message_data(cached_msg);

	return device_state->state;
}

/*! \brief Check device state through channel specific function or generic function */
static enum ast_device_state _ast_device_state(const char *device, int check_cache)
{
	char *number;
	const struct ast_channel_tech *chan_tech;
	enum ast_device_state res;
	/*! \brief Channel driver that provides device state */
	char *tech;

	/* If the last known state is cached, just return that */
	if (check_cache) {
		res = devstate_cached(device);
		if (res != AST_DEVICE_UNKNOWN) {
			return res;
		}
	}

	number = ast_strdupa(device);
	tech = strsep(&number, "/");
	if (!number) {
		/*! \brief Another provider of device state */
		char *provider;

		provider = strsep(&tech, ":");
		if (!tech) {
			return AST_DEVICE_INVALID;
		}
		/* We have a provider */
		number = tech;

		ast_debug(3, "Checking if I can find provider for \"%s\" - number: %s\n", provider, number);
		return getproviderstate(provider, number);
	}

	ast_debug(4, "No provider found, checking channel drivers for %s - %s\n", tech, number);

	chan_tech = ast_get_channel_tech(tech);
	if (!chan_tech) {
		return AST_DEVICE_INVALID;
	}

	/* Does the channel driver support device state notification? */
	if (!chan_tech->devicestate) {
		/* No, try the generic function */
		return ast_parse_device_state(device);
	}

	res = chan_tech->devicestate(number);
	if (res == AST_DEVICE_UNKNOWN) {
		res = ast_parse_device_state(device);
	}

	return res;
}

enum ast_device_state ast_device_state(const char *device)
{
	/* This function is called from elsewhere in the code to find out the
	 * current state of a device.  Check the cache, first. */

	return _ast_device_state(device, 1);
}

/*! \brief Add device state provider */
int ast_devstate_prov_add(const char *label, ast_devstate_prov_cb_type callback)
{
	struct devstate_prov *devprov;

	if (!callback || !(devprov = ast_calloc(1, sizeof(*devprov))))
		return -1;

	devprov->callback = callback;
	ast_copy_string(devprov->label, label, sizeof(devprov->label));

	AST_RWLIST_WRLOCK(&devstate_provs);
	AST_RWLIST_INSERT_HEAD(&devstate_provs, devprov, list);
	AST_RWLIST_UNLOCK(&devstate_provs);

	return 0;
}

/*! \brief Remove device state provider */
int ast_devstate_prov_del(const char *label)
{
	struct devstate_prov *devcb;
	int res = -1;

	AST_RWLIST_WRLOCK(&devstate_provs);
	AST_RWLIST_TRAVERSE_SAFE_BEGIN(&devstate_provs, devcb, list) {
		if (!strcasecmp(devcb->label, label)) {
			AST_RWLIST_REMOVE_CURRENT(list);
			ast_free(devcb);
			res = 0;
			break;
		}
	}
	AST_RWLIST_TRAVERSE_SAFE_END;
	AST_RWLIST_UNLOCK(&devstate_provs);

	return res;
}

/*! \brief Get provider device state */
static int getproviderstate(const char *provider, const char *address)
{
	struct devstate_prov *devprov;
	int res = AST_DEVICE_INVALID;

	AST_RWLIST_RDLOCK(&devstate_provs);
	AST_RWLIST_TRAVERSE(&devstate_provs, devprov, list) {
		ast_debug(5, "Checking provider %s with %s\n", devprov->label, provider);

		if (!strcasecmp(devprov->label, provider)) {
			res = devprov->callback(address);
			break;
		}
	}
	AST_RWLIST_UNLOCK(&devstate_provs);

	return res;
}

/*! Called by the state change thread to find out what the state is, and then
 *  to queue up the state change event */
static void do_state_change(const char *device, enum ast_devstate_cache cachable)
{
	enum ast_device_state state;

	state = _ast_device_state(device, 0);

	ast_debug(3, "Changing state for %s - state %d (%s)\n", device, state, ast_devstate2str(state));

	ast_publish_device_state(device, state, cachable);
}

int ast_devstate_changed_literal(enum ast_device_state state, enum ast_devstate_cache cachable, const char *device)
{
	struct state_change *change;

	/*
	 * If we know the state change (how nice of the caller of this function!)
	 * then we can just generate a device state event.
	 *
	 * Otherwise, we do the following:
	 *   - Queue an event up to another thread that the state has changed
	 *   - In the processing thread, it calls the callback provided by the
	 *     device state provider (which may or may not be a channel driver)
	 *     to determine the state.
	 *   - If the device state provider does not know the state, or this is
	 *     for a channel and the channel driver does not implement a device
	 *     state callback, then we will look through the channel list to
	 *     see if we can determine a state based on active calls.
	 *   - Once a state has been determined, a device state event is generated.
	 */

	if (state != AST_DEVICE_UNKNOWN) {
		ast_publish_device_state(device, state, cachable);
	} else if (change_thread == AST_PTHREADT_NULL || !(change = ast_calloc(1, sizeof(*change) + strlen(device)))) {
		/* we could not allocate a change struct, or */
		/* there is no background thread, so process the change now */
		do_state_change(device, cachable);
	} else {
		/* queue the change */
		strcpy(change->device, device);
		change->cachable = cachable;
		AST_LIST_LOCK(&state_changes);
		AST_LIST_INSERT_TAIL(&state_changes, change, list);
		ast_cond_signal(&change_pending);
		AST_LIST_UNLOCK(&state_changes);
	}

	return 0;
}

int ast_device_state_changed_literal(const char *dev)
{
	return ast_devstate_changed_literal(AST_DEVICE_UNKNOWN, AST_DEVSTATE_CACHABLE, dev);
}

int ast_devstate_changed(enum ast_device_state state, enum ast_devstate_cache cachable, const char *fmt, ...)
{
	char buf[AST_MAX_EXTENSION];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	return ast_devstate_changed_literal(state, cachable, buf);
}

int ast_device_state_changed(const char *fmt, ...)
{
	char buf[AST_MAX_EXTENSION];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	return ast_devstate_changed_literal(AST_DEVICE_UNKNOWN, AST_DEVSTATE_CACHABLE, buf);
}

/*! \brief Go through the dev state change queue and update changes in the dev state thread */
static void *do_devstate_changes(void *data)
{
	struct state_change *next, *current;

	for (;;) {
		/* This basically pops off any state change entries, resets the list back to NULL, unlocks, and processes each state change */
		AST_LIST_LOCK(&state_changes);
		if (AST_LIST_EMPTY(&state_changes))
			ast_cond_wait(&change_pending, &state_changes.lock);
		next = AST_LIST_FIRST(&state_changes);
		AST_LIST_HEAD_INIT_NOLOCK(&state_changes);
		AST_LIST_UNLOCK(&state_changes);

		/* Process each state change */
		while ((current = next)) {
			next = AST_LIST_NEXT(current, list);
			do_state_change(current->device, current->cachable);
			ast_free(current);
		}
	}

	return NULL;
}

#define MAX_SERVERS 64
static int devstate_change_aggregator_cb(void *obj, void *arg, void *data, int flags)
{
	struct stasis_message *msg = obj;
	struct ast_devstate_aggregate *aggregate = arg;
	char *device = data;
	struct ast_device_state_message *device_state = stasis_message_data(msg);

	if (!device_state->eid || strcmp(device, device_state->device)) {
		/* ignore aggregate states and devices that don't match */
		return 0;
	}
	ast_debug(1, "Adding per-server state of '%s' for '%s'\n",
		ast_devstate2str(device_state->state), device);
	ast_devstate_aggregate_add(aggregate, device_state->state);
	return 0;
}

static void device_state_dtor(void *obj)
{
	struct ast_device_state_message *device_state = obj;
	ast_string_field_free_memory(device_state);
	ast_free(device_state->eid);
}

static struct ast_device_state_message *device_state_alloc(const char *device, enum ast_device_state state, enum ast_devstate_cache cachable, const struct ast_eid *eid)
{
	RAII_VAR(struct ast_device_state_message *, new_device_state, ao2_alloc(sizeof(*new_device_state), device_state_dtor), ao2_cleanup);

	if (!new_device_state || ast_string_field_init(new_device_state, 256)) {
		return NULL;
	}

	ast_string_field_set(new_device_state, device, device);
	new_device_state->state = state;
	new_device_state->cachable = cachable;

	if (eid) {
		char eid_str[20];
		struct ast_str *cache_id = ast_str_alloca(256);

		new_device_state->eid = ast_malloc(sizeof(*eid));
		if (!new_device_state->eid) {
			return NULL;
		}

		*new_device_state->eid = *eid;
		ast_eid_to_str(eid_str, sizeof(eid_str), new_device_state->eid);
		ast_str_set(&cache_id, 0, "%s%s", eid_str, device);
		ast_string_field_set(new_device_state, cache_id, ast_str_buffer(cache_id));
	} else {
		/* no EID makes this an aggregate state */
		ast_string_field_set(new_device_state, cache_id, device);
	}

	ao2_ref(new_device_state, +1);
	return new_device_state;
}

static enum ast_device_state get_aggregate_state(char *device)
{
	RAII_VAR(struct ao2_container *, cached, NULL, ao2_cleanup);
	struct ast_devstate_aggregate aggregate;

	ast_devstate_aggregate_init(&aggregate);

	cached = stasis_cache_dump(ast_device_state_cache(), NULL);

	ao2_callback_data(cached, OBJ_NODATA, devstate_change_aggregator_cb, &aggregate, device);

	return ast_devstate_aggregate_result(&aggregate);
}

static int aggregate_state_changed(char *device, enum ast_device_state new_aggregate_state)
{
	RAII_VAR(struct stasis_message *, cached_aggregate_msg, NULL, ao2_cleanup);
	struct ast_device_state_message *cached_aggregate_device_state;

	cached_aggregate_msg = stasis_cache_get(ast_device_state_cache(), ast_device_state_message_type(), device);
	if (!cached_aggregate_msg) {
		return 1;
	}

	cached_aggregate_device_state = stasis_message_data(cached_aggregate_msg);
	if (cached_aggregate_device_state->state == new_aggregate_state) {
		return 0;
	}
	return 1;
}

static void devstate_change_collector_cb(void *data, struct stasis_subscription *sub, struct stasis_message *msg)
{
	enum ast_device_state aggregate_state;
	char *device;
	struct ast_device_state_message *device_state;
	RAII_VAR(struct stasis_message *, new_aggregate_msg, NULL, ao2_cleanup);
	RAII_VAR(struct ast_device_state_message *, new_aggregate_state, NULL, ao2_cleanup);

	if (stasis_cache_update_type() == stasis_message_type(msg)) {
		struct stasis_cache_update *update = stasis_message_data(msg);
		if (!update->new_snapshot) {
			return;
		}
		msg = update->new_snapshot;
	}

	if (ast_device_state_message_type() != stasis_message_type(msg)) {
		return;
	}

	device_state = stasis_message_data(msg);

	if (!device_state->eid) {
		/* ignore aggregate messages */
		return;
	}

	device = ast_strdupa(device_state->device);
	ast_debug(1, "Processing device state change for '%s'\n", device);

	if (device_state->cachable == AST_DEVSTATE_NOT_CACHABLE) {
		/* if it's not cachable, there will be no aggregate state to get
		 * and this should be passed through */
		aggregate_state = device_state->state;
	} else {

		aggregate_state = get_aggregate_state(device);
		ast_debug(1, "Aggregate devstate result is '%s' for '%s'\n",
			ast_devstate2str(aggregate_state), device);

		if (!aggregate_state_changed(device, aggregate_state)) {
			/* No change since last reported device state */
			ast_debug(1, "Aggregate state for device '%s' has not changed from '%s'\n",
				device, ast_devstate2str(aggregate_state));
			return;
		}
	}

	ast_debug(1, "Aggregate state for device '%s' has changed to '%s'\n",
		device, ast_devstate2str(aggregate_state));

	ast_publish_device_state_full(device, aggregate_state, device_state->cachable, NULL);
}

/*! \brief Initialize the device state engine in separate thread */
int ast_device_state_engine_init(void)
{
	ast_cond_init(&change_pending, NULL);
	if (ast_pthread_create_background(&change_thread, NULL, do_devstate_changes, NULL) < 0) {
		ast_log(LOG_ERROR, "Unable to start device state change thread.\n");
		return -1;
	}

	return 0;
}

void ast_devstate_aggregate_init(struct ast_devstate_aggregate *agg)
{
	memset(agg, 0, sizeof(*agg));
	agg->state = AST_DEVICE_INVALID;
}

void ast_devstate_aggregate_add(struct ast_devstate_aggregate *agg, enum ast_device_state state)
{
	static enum ast_device_state state_order[] = {
		1, /* AST_DEVICE_UNKNOWN */
		3, /* AST_DEVICE_NOT_INUSE */
		6, /* AST_DEVICE_INUSE */
		7, /* AST_DEVICE_BUSY */
		0, /* AST_DEVICE_INVALID */
		2, /* AST_DEVICE_UNAVAILABLE */
		5, /* AST_DEVICE_RINGING */
		8, /* AST_DEVICE_RINGINUSE */
		4, /* AST_DEVICE_ONHOLD */
	};

	if (state == AST_DEVICE_RINGING) {
		agg->ringing = 1;
	} else if (state == AST_DEVICE_INUSE || state == AST_DEVICE_ONHOLD || state == AST_DEVICE_BUSY) {
		agg->inuse = 1;
	}

	if (agg->ringing && agg->inuse) {
		agg->state = AST_DEVICE_RINGINUSE;
	} else if (state_order[state] > state_order[agg->state]) {
		agg->state = state;
	}
}

enum ast_device_state ast_devstate_aggregate_result(struct ast_devstate_aggregate *agg)
{
	return agg->state;
}

struct stasis_topic *ast_device_state_topic_all(void)
{
	return device_state_topic_all;
}

struct stasis_cache *ast_device_state_cache(void)
{
	return device_state_cache;
}

struct stasis_topic *ast_device_state_topic_cached(void)
{
	return stasis_caching_get_topic(device_state_topic_cached);
}

struct stasis_topic *ast_device_state_topic(const char *device)
{
	return stasis_topic_pool_get_topic(device_state_topic_pool, device);
}

int ast_device_state_clear_cache(const char *device)
{
	RAII_VAR(struct stasis_message *, cached_msg, NULL, ao2_cleanup);
	RAII_VAR(struct stasis_message *, msg, NULL, ao2_cleanup);

	if (!(cached_msg = stasis_cache_get(ast_device_state_cache(),
					    ast_device_state_message_type(), device))) {
		/* nothing to clear */
		return -1;
	}

	msg = stasis_cache_clear_create(cached_msg);
	stasis_publish(ast_device_state_topic(device), msg);
	return 0;
}

int ast_publish_device_state_full(
			const char *device,
			enum ast_device_state state,
			enum ast_devstate_cache cachable,
			struct ast_eid *eid)
{
	RAII_VAR(struct ast_device_state_message *, device_state, NULL, ao2_cleanup);
	RAII_VAR(struct stasis_message *, message, NULL, ao2_cleanup);
	struct stasis_topic *device_specific_topic;

	ast_assert(!ast_strlen_zero(device));

	device_state = device_state_alloc(device, state, cachable, eid);
	if (!device_state) {
		return -1;
	}

	message = stasis_message_create(ast_device_state_message_type(), device_state);

	device_specific_topic = ast_device_state_topic(device);
	if (!device_specific_topic) {
		return -1;
	}

	stasis_publish(device_specific_topic, message);
	return 0;
}

static const char *device_state_get_id(struct stasis_message *message)
{
	struct ast_device_state_message *device_state;
	if (ast_device_state_message_type() != stasis_message_type(message)) {
		return NULL;
	}

	device_state = stasis_message_data(message);
	if (device_state->cachable == AST_DEVSTATE_NOT_CACHABLE) {
		return NULL;
	}

	return device_state->cache_id;
}

static void devstate_cleanup(void)
{
	devstate_message_sub = stasis_unsubscribe_and_join(devstate_message_sub);
	ao2_cleanup(device_state_topic_all);
	device_state_topic_all = NULL;
	ao2_cleanup(device_state_cache);
	device_state_cache = NULL;
	device_state_topic_cached = stasis_caching_unsubscribe_and_join(device_state_topic_cached);
	STASIS_MESSAGE_TYPE_CLEANUP(ast_device_state_message_type);
	ao2_cleanup(device_state_topic_pool);
	device_state_topic_pool = NULL;
}

int devstate_init(void)
{
	ast_register_cleanup(devstate_cleanup);

	if (STASIS_MESSAGE_TYPE_INIT(ast_device_state_message_type) != 0) {
		return -1;
	}
	device_state_topic_all = stasis_topic_create("ast_device_state_topic");
	if (!device_state_topic_all) {
		return -1;
	}
	device_state_cache = stasis_cache_create(device_state_get_id);
	if (!device_state_cache) {
		return -1;
	}
	device_state_topic_cached = stasis_caching_topic_create(device_state_topic_all, device_state_cache);
	if (!device_state_topic_cached) {
		return -1;
	}
	device_state_topic_pool = stasis_topic_pool_create(ast_device_state_topic_all());
	if (!device_state_topic_pool) {
		return -1;
	}

	devstate_message_sub = stasis_subscribe(ast_device_state_topic_cached(), devstate_change_collector_cb, NULL);

	if (!devstate_message_sub) {
		ast_log(LOG_ERROR, "Failed to create subscription for the device state change collector\n");
		return -1;
	}

	return 0;
}
