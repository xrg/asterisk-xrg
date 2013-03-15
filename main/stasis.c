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
 * \brief Stasis Message Bus API.
 *
 * \author David M. Lee, II <dlee@digium.com>
 */

/*** MODULEINFO
	<support_level>core</support_level>
 ***/

#include "asterisk.h"

ASTERISK_FILE_VERSION(__FILE__, "$Revision$")

#include "asterisk/astobj2.h"
#include "asterisk/stasis.h"
#include "asterisk/threadpool.h"
#include "asterisk/taskprocessor.h"
#include "asterisk/utils.h"
#include "asterisk/uuid.h"

/*! Initial size of the subscribers list. */
#define INITIAL_SUBSCRIBERS_MAX 4

/*! Threadpool for dispatching notifications to subscribers */
static struct ast_threadpool *pool;

static struct stasis_message_type *__subscription_change_message_type;

/*! \private */
struct stasis_topic {
	char *name;
	/*! Variable length array of the subscribers (raw pointer to avoid cyclic references) */
	struct stasis_subscription **subscribers;
	/*! Allocated length of the subscribers array */
	size_t num_subscribers_max;
	/*! Current size of the subscribers array */
	size_t num_subscribers_current;
};

/* Forward declarations for the tightly-coupled subscription object */
struct stasis_subscription;
static int topic_add_subscription(struct stasis_topic *topic, struct stasis_subscription *sub);

static void topic_dtor(void *obj)
{
	struct stasis_topic *topic = obj;
	ast_free(topic->name);
	topic->name = NULL;
	ast_free(topic->subscribers);
	topic->subscribers = NULL;
}

struct stasis_topic *stasis_topic_create(const char *name)
{
	RAII_VAR(struct stasis_topic *, topic, NULL, ao2_cleanup);

	topic = ao2_alloc(sizeof(*topic), topic_dtor);

	if (!topic) {
		return NULL;
	}

	topic->name = ast_strdup(name);
	if (!topic->name) {
		return NULL;
	}

	topic->num_subscribers_max = INITIAL_SUBSCRIBERS_MAX;
	topic->subscribers = ast_calloc(topic->num_subscribers_max, sizeof(topic->subscribers));
	if (!topic->subscribers) {
		return NULL;
	}

	ao2_ref(topic, +1);
	return topic;
}

const char *stasis_topic_name(const struct stasis_topic *topic)
{
	return topic->name;
}

/*! \private */
struct stasis_subscription {
	/*! Unique ID for this subscription */
	char *uniqueid;
	/*! Topic subscribed to. */
	struct stasis_topic *topic;
	/*! Mailbox for processing incoming messages. */
	struct ast_taskprocessor *mailbox;
	/*! Callback function for incoming message processing. */
	stasis_subscription_cb callback;
	/*! Data pointer to be handed to the callback. */
	void *data;
};

static void subscription_dtor(void *obj)
{
	struct stasis_subscription *sub = obj;
	ast_assert(!stasis_subscription_is_subscribed(sub));
	ast_free(sub->uniqueid);
	sub->uniqueid = NULL;
	ao2_cleanup(sub->topic);
	sub->topic = NULL;
	ast_taskprocessor_unreference(sub->mailbox);
	sub->mailbox = NULL;
}

static void send_subscription_change_message(struct stasis_topic *topic, char *uniqueid, char *description);

static struct stasis_subscription *__stasis_subscribe(struct stasis_topic *topic, stasis_subscription_cb callback, void *data, int needs_mailbox)
{
	RAII_VAR(struct stasis_subscription *, sub, NULL, ao2_cleanup);
	RAII_VAR(struct ast_uuid *, id, NULL, ast_free);
	char uniqueid[AST_UUID_STR_LEN];

	sub = ao2_alloc(sizeof(*sub), subscription_dtor);
	if (!sub) {
		return NULL;
	}

	id = ast_uuid_generate();
	if (!id) {
		ast_log(LOG_ERROR, "UUID generation failed\n");
		return NULL;
	}
	ast_uuid_to_str(id, uniqueid, sizeof(uniqueid));
	if (needs_mailbox) {
		sub->mailbox = ast_threadpool_serializer(uniqueid, pool);
		if (!sub->mailbox) {
			return NULL;
		}
	}

	sub->uniqueid = ast_strdup(uniqueid);
	ao2_ref(topic, +1);
	sub->topic = topic;
	sub->callback = callback;
	sub->data = data;

	if (topic_add_subscription(topic, sub) != 0) {
		return NULL;
	}
	send_subscription_change_message(topic, uniqueid, "Subscribe");

	ao2_ref(sub, +1);
	return sub;
}

struct stasis_subscription *stasis_subscribe(struct stasis_topic *topic, stasis_subscription_cb callback, void *data)
{
	return __stasis_subscribe(topic, callback, data, 1);
}

struct stasis_subscription *stasis_unsubscribe(struct stasis_subscription *sub)
{
	if (sub) {
		size_t i;
		struct stasis_topic *topic = sub->topic;
		SCOPED_AO2LOCK(lock_topic, topic);

		for (i = 0; i < topic->num_subscribers_current; ++i) {
			if (topic->subscribers[i] == sub) {
				send_subscription_change_message(topic, sub->uniqueid, "Unsubscribe");
				/* swap [i] with last entry; remove last entry */
				topic->subscribers[i] = topic->subscribers[--topic->num_subscribers_current];
				/* Unsubscribing unrefs the subscription */
				ao2_cleanup(sub);
				return NULL;
			}
		}

		ast_log(LOG_ERROR, "Internal error: subscription has invalid topic\n");
	}
	return NULL;
}

int stasis_subscription_is_subscribed(const struct stasis_subscription *sub)
{
	if (sub) {
		size_t i;
		struct stasis_topic *topic = sub->topic;
		SCOPED_AO2LOCK(lock_topic, topic);

		for (i = 0; i < topic->num_subscribers_current; ++i) {
			if (topic->subscribers[i] == sub) {
				return 1;
			}
		}
	}

	return 0;
}

const char *stasis_subscription_uniqueid(const struct stasis_subscription *sub)
{
	return sub->uniqueid;
}

int stasis_subscription_final_message(struct stasis_subscription *sub, struct stasis_message *msg)
{
	struct stasis_subscription_change *change;
	if (stasis_message_type(msg) != stasis_subscription_change()) {
		return 0;
	}

	change = stasis_message_data(msg);
	if (strcmp("Unsubscribe", change->description)) {
		return 0;
	}

	if (strcmp(stasis_subscription_uniqueid(sub), change->uniqueid)) {
		return 0;
	}

	return 1;
}

/*!
 * \brief Add a subscriber to a topic.
 * \param topic Topic
 * \param sub Subscriber
 * \return 0 on success
 * \return Non-zero on error
 */
static int topic_add_subscription(struct stasis_topic *topic, struct stasis_subscription *sub)
{
	struct stasis_subscription **subscribers;
	SCOPED_AO2LOCK(lock, topic);

	/* Increase list size, if needed */
	if (topic->num_subscribers_current + 1 > topic->num_subscribers_max) {
		subscribers = realloc(topic->subscribers, 2 * topic->num_subscribers_max * sizeof(*subscribers));
		if (!subscribers) {
			return -1;
		}
		topic->subscribers = subscribers;
		topic->num_subscribers_max *= 2;
	}

	/* Don't ref sub here or we'll cause a reference cycle. */
	topic->subscribers[topic->num_subscribers_current++] = sub;
	return 0;
}

/*!
 * \private
 * \brief Information needed to dispatch a message to a subscription
 */
struct dispatch {
	/*! Topic message was published to */
	struct stasis_topic *topic;
	/*! The message itself */
	struct stasis_message *message;
	/*! Subscription receiving the message */
	struct stasis_subscription *sub;
};

static void dispatch_dtor(void *data)
{
	struct dispatch *dispatch = data;
	ao2_cleanup(dispatch->topic);
	ao2_cleanup(dispatch->message);
	ao2_cleanup(dispatch->sub);
}

static struct dispatch *dispatch_create(struct stasis_topic *topic, struct stasis_message *message, struct stasis_subscription *sub)
{
	RAII_VAR(struct dispatch *, dispatch, NULL, ao2_cleanup);

	ast_assert(topic != NULL);
	ast_assert(message != NULL);
	ast_assert(sub != NULL);

	dispatch = ao2_alloc(sizeof(*dispatch), dispatch_dtor);
	if (!dispatch) {
		return NULL;
	}

	dispatch->topic = topic;
	ao2_ref(topic, +1);

	dispatch->message = message;
	ao2_ref(message, +1);

	dispatch->sub = sub;
	ao2_ref(sub, +1);

	ao2_ref(dispatch, +1);
	return dispatch;
}

/*!
 * \brief Dispatch a message to a subscriber
 * \param data \ref dispatch object
 * \return 0
 */
static int dispatch_exec(void *data)
{
	RAII_VAR(struct dispatch *, dispatch, data, ao2_cleanup);
	RAII_VAR(struct stasis_topic *, sub_topic, NULL, ao2_cleanup);

	/* Since sub->topic doesn't change, no need to lock sub */
	ast_assert(dispatch->sub->topic != NULL);
	ao2_ref(dispatch->sub->topic, +1);
	sub_topic = dispatch->sub->topic;

	dispatch->sub->callback(dispatch->sub->data,
				dispatch->sub,
				sub_topic,
				dispatch->message);

	return 0;
}

void stasis_forward_message(struct stasis_topic *topic, struct stasis_topic *publisher_topic, struct stasis_message *message)
{
	struct stasis_subscription **subscribers = NULL;
	size_t num_subscribers, i;

	ast_assert(topic != NULL);
	ast_assert(publisher_topic != NULL);
	ast_assert(message != NULL);

	/* Copy the subscribers, so we don't have to hold the mutex for long */
	{
		SCOPED_AO2LOCK(lock, topic);
		num_subscribers = topic->num_subscribers_current;
		subscribers = ast_malloc(num_subscribers * sizeof(*subscribers));
		if (subscribers) {
			for (i = 0; i < num_subscribers; ++i) {
				ao2_ref(topic->subscribers[i], +1);
				subscribers[i] = topic->subscribers[i];
			}
		}
	}

	if (!subscribers) {
		ast_log(LOG_ERROR, "Dropping message\n");
		return;
	}

	for (i = 0; i < num_subscribers; ++i) {
		struct stasis_subscription *sub = subscribers[i];

		ast_assert(sub != NULL);

		if (sub->mailbox) {
			RAII_VAR(struct dispatch *, dispatch, NULL, ao2_cleanup);

			dispatch = dispatch_create(publisher_topic, message, sub);
			if (!dispatch) {
				ast_log(LOG_DEBUG, "Dropping dispatch\n");
				break;
			}

			if (ast_taskprocessor_push(sub->mailbox, dispatch_exec, dispatch) == 0) {
				dispatch = NULL; /* Ownership transferred to mailbox */
			}
		} else {
			/* No mailbox; dispatch directly */
			sub->callback(sub->data, sub, sub->topic, message);
		}
	}

	for (i = 0; i < num_subscribers; ++i) {
		ao2_cleanup(subscribers[i]);
	}
	ast_free(subscribers);
}

void stasis_publish(struct stasis_topic *topic, struct stasis_message *message)
{
	stasis_forward_message(topic, topic, message);
}

/*! \brief Forwarding subscriber */
static void stasis_forward_cb(void *data, struct stasis_subscription *sub, struct stasis_topic *topic, struct stasis_message *message)
{
	struct stasis_topic *to_topic = data;
	stasis_forward_message(to_topic, topic, message);

	if (stasis_subscription_final_message(sub, message)) {
		ao2_cleanup(to_topic);
	}
}

struct stasis_subscription *stasis_forward_all(struct stasis_topic *from_topic, struct stasis_topic *to_topic)
{
	struct stasis_subscription *sub;
	if (!from_topic || !to_topic) {
		return NULL;
	}
	/* Subscribe without a mailbox, since we're just forwarding messages */
	sub = __stasis_subscribe(from_topic, stasis_forward_cb, to_topic, 0);
	if (sub) {
		/* hold a ref to to_topic for this forwarding subscription */
		ao2_ref(to_topic, +1);
	}
	return sub;
}

static void subscription_change_dtor(void *obj)
{
	struct stasis_subscription_change *change = obj;
	ast_string_field_free_memory(change);
	ao2_cleanup(change->topic);
}

static struct stasis_subscription_change *subscription_change_alloc(struct stasis_topic *topic, char *uniqueid, char *description)
{
	RAII_VAR(struct stasis_subscription_change *, change, NULL, ao2_cleanup);

	change = ao2_alloc(sizeof(struct stasis_subscription_change), subscription_change_dtor);
	if (ast_string_field_init(change, 128)) {
		return NULL;
	}

	ast_string_field_set(change, uniqueid, uniqueid);
	ast_string_field_set(change, description, description);
	ao2_ref(topic, +1);
	change->topic = topic;

	ao2_ref(change, +1);
	return change;
}

struct stasis_message_type *stasis_subscription_change(void)
{
	return __subscription_change_message_type;
}

static void send_subscription_change_message(struct stasis_topic *topic, char *uniqueid, char *description)
{
	RAII_VAR(struct stasis_subscription_change *, change, NULL, ao2_cleanup);
	RAII_VAR(struct stasis_message *, msg, NULL, ao2_cleanup);

	change = subscription_change_alloc(topic, uniqueid, description);

	if (!change) {
		return;
	}

	msg = stasis_message_create(stasis_subscription_change(), change);

	if (!msg) {
		return;
	}

	stasis_publish(topic, msg);
}

/*! \brief Cleanup function */
static void stasis_exit(void)
{
	ao2_cleanup(__subscription_change_message_type);
	__subscription_change_message_type = NULL;
	ast_threadpool_shutdown(pool);
	pool = NULL;
}

int stasis_init(void)
{
	int cache_init;

	/* XXX Should this be configurable? */
	struct ast_threadpool_options opts = {
		.version = AST_THREADPOOL_OPTIONS_VERSION,
		.idle_timeout = 20,
		.auto_increment = 1,
		.initial_size = 0,
		.max_size = 200
	};

	ast_register_atexit(stasis_exit);

	if (pool) {
		ast_log(LOG_ERROR, "Stasis double-initialized\n");
		return -1;
	}

	pool = ast_threadpool_create("stasis-core", NULL, &opts);
	if (!pool) {
		ast_log(LOG_ERROR, "Stasis threadpool allocation failed\n");
		return -1;
	}

	cache_init = stasis_cache_init();
	if (cache_init != 0) {
		return -1;
	}

	__subscription_change_message_type = stasis_message_type_create("stasis_subscription_change");
	if (!__subscription_change_message_type) {
		return -1;
	}

	return 0;
}
