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
 * \brief Stasis Message API.
 *
 * \author David M. Lee, II <dlee@digium.com>
 */

/*** MODULEINFO
	<support_level>core</support_level>
 ***/

#include "asterisk.h"

ASTERISK_FILE_VERSION(__FILE__, "$Revision$")

#include "asterisk/astobj2.h"
#include "asterisk/hashtab.h"
#include "asterisk/stasis_internal.h"
#include "asterisk/stasis.h"
#include "asterisk/utils.h"

#ifdef LOW_MEMORY
#define NUM_CACHE_BUCKETS 17
#else
#define NUM_CACHE_BUCKETS 563
#endif

/*! \internal */
struct stasis_cache {
	struct ao2_container *entries;
	snapshot_get_id id_fn;
};

/*! \internal */
struct stasis_caching_topic {
	struct stasis_cache *cache;
	struct stasis_topic *topic;
	struct stasis_topic *original_topic;
	struct stasis_subscription *sub;
};

static void stasis_caching_topic_dtor(void *obj) {
	struct stasis_caching_topic *caching_topic = obj;
	ast_assert(!stasis_subscription_is_subscribed(caching_topic->sub));
	ast_assert(stasis_subscription_is_done(caching_topic->sub));
	ao2_cleanup(caching_topic->sub);
	caching_topic->sub = NULL;
	ao2_cleanup(caching_topic->cache);
	caching_topic->cache = NULL;
	ao2_cleanup(caching_topic->topic);
	caching_topic->topic = NULL;
	ao2_cleanup(caching_topic->original_topic);
	caching_topic->original_topic = NULL;
}

struct stasis_topic *stasis_caching_get_topic(struct stasis_caching_topic *caching_topic)
{
	return caching_topic->topic;
}

struct stasis_caching_topic *stasis_caching_unsubscribe(struct stasis_caching_topic *caching_topic)
{
	if (caching_topic) {
		RAII_VAR(struct stasis_caching_topic *, hold_ref, NULL,
			ao2_cleanup);

		/* The subscription may hold the last reference to this caching
		 * topic, but we want to make sure the unsubscribe finishes
		 * before kicking of the caching topic's dtor.
		 */
		ao2_ref(caching_topic, +1);
		hold_ref = caching_topic;

		if (stasis_subscription_is_subscribed(caching_topic->sub)) {
			/* Increment the reference to hold on to it past the
			 * unsubscribe. Will be cleaned up in dtor. */
			ao2_ref(caching_topic->sub, +1);
			stasis_unsubscribe(caching_topic->sub);
		} else {
			ast_log(LOG_ERROR, "stasis_caching_topic unsubscribed multiple times\n");
		}
	}
	return NULL;
}

struct stasis_caching_topic *stasis_caching_unsubscribe_and_join(struct stasis_caching_topic *caching_topic)
{
	if (!caching_topic) {
		return NULL;
	}

	/* Hold a ref past the unsubscribe */
	ao2_ref(caching_topic, +1);
	stasis_caching_unsubscribe(caching_topic);
	stasis_subscription_join(caching_topic->sub);
	ao2_cleanup(caching_topic);
	return NULL;
}

struct cache_entry {
	struct stasis_message_type *type;
	char *id;
	struct stasis_message *snapshot;
};

static void cache_entry_dtor(void *obj)
{
	struct cache_entry *entry = obj;
	ao2_cleanup(entry->type);
	entry->type = NULL;
	ast_free(entry->id);
	entry->id = NULL;
	ao2_cleanup(entry->snapshot);
	entry->snapshot = NULL;
}

static struct cache_entry *cache_entry_create(struct stasis_message_type *type, const char *id, struct stasis_message *snapshot)
{
	RAII_VAR(struct cache_entry *, entry, NULL, ao2_cleanup);

	ast_assert(type != NULL);
	ast_assert(id != NULL);

	entry = ao2_alloc_options(sizeof(*entry), cache_entry_dtor,
		AO2_ALLOC_OPT_LOCK_NOLOCK);
	if (!entry) {
		return NULL;
	}

	entry->id = ast_strdup(id);
	if (!entry->id) {
		return NULL;
	}

	ao2_ref(type, +1);
	entry->type = type;
	if (snapshot != NULL) {
		ao2_ref(snapshot, +1);
		entry->snapshot = snapshot;
	}

	ao2_ref(entry, +1);
	return entry;
}

static int cache_entry_hash(const void *obj, int flags)
{
	const struct cache_entry *entry = obj;
	int hash = 0;

	ast_assert(!(flags & OBJ_KEY));

	hash += ast_hashtab_hash_string(stasis_message_type_name(entry->type));
	hash += ast_hashtab_hash_string(entry->id);
	return hash;
}

static int cache_entry_cmp(void *obj, void *arg, int flags)
{
	const struct cache_entry *left = obj;
	const struct cache_entry *right = arg;

	ast_assert(!(flags & OBJ_KEY));

	if (left->type == right->type && strcmp(left->id, right->id) == 0) {
		return CMP_MATCH | CMP_STOP;
	}

	return 0;
}

static void cache_dtor(void *obj)
{
        struct stasis_cache *cache = obj;

        ao2_cleanup(cache->entries);
        cache->entries = NULL;
}

struct stasis_cache *stasis_cache_create(snapshot_get_id id_fn)
{
        RAII_VAR(struct stasis_cache *, cache, NULL, ao2_cleanup);

        cache = ao2_alloc_options(sizeof(*cache), cache_dtor,
		AO2_ALLOC_OPT_LOCK_NOLOCK);
        if (!cache) {
                return NULL;
        }

        cache->entries = ao2_container_alloc(NUM_CACHE_BUCKETS, cache_entry_hash,
                cache_entry_cmp);
        if (!cache->entries) {
                return NULL;
        }

        cache->id_fn = id_fn;

        ao2_ref(cache, +1);
        return cache;
}

static struct stasis_message *cache_put(struct stasis_cache *cache,
	struct stasis_message_type *type, const char *id,
	struct stasis_message *new_snapshot)
{
	RAII_VAR(struct cache_entry *, new_entry, NULL, ao2_cleanup);
	RAII_VAR(struct cache_entry *, cached_entry, NULL, ao2_cleanup);
	struct stasis_message *old_snapshot = NULL;

	ast_assert(cache->entries != NULL);
	ast_assert(new_snapshot == NULL ||
		type == stasis_message_type(new_snapshot));

	new_entry = cache_entry_create(type, id, new_snapshot);

	if (new_snapshot == NULL) {
		/* Remove entry from cache */
		cached_entry = ao2_find(cache->entries, new_entry, OBJ_POINTER | OBJ_UNLINK);
		if (cached_entry) {
			old_snapshot = cached_entry->snapshot;
			cached_entry->snapshot = NULL;
		}
	} else {
		/* Insert/update cache */
		SCOPED_AO2LOCK(lock, cache->entries);

		cached_entry = ao2_find(cache->entries, new_entry, OBJ_POINTER | OBJ_NOLOCK);
		if (cached_entry) {
			/* Update cache. Because objects are moving, no need to update refcounts. */
			old_snapshot = cached_entry->snapshot;
			cached_entry->snapshot = new_entry->snapshot;
			new_entry->snapshot = NULL;
		} else {
			/* Insert into the cache */
			ao2_link_flags(cache->entries, new_entry, OBJ_NOLOCK);
		}

	}

	return old_snapshot;
}

struct stasis_message *stasis_cache_get(struct stasis_cache *cache, struct stasis_message_type *type, const char *id)
{
	RAII_VAR(struct cache_entry *, search_entry, NULL, ao2_cleanup);
	RAII_VAR(struct cache_entry *, cached_entry, NULL, ao2_cleanup);

	ast_assert(cache->entries != NULL);

	search_entry = cache_entry_create(type, id, NULL);
	if (search_entry == NULL) {
		return NULL;
	}

	cached_entry = ao2_find(cache->entries, search_entry, OBJ_POINTER);
	if (cached_entry == NULL) {
		return NULL;
	}

	ast_assert(cached_entry->snapshot != NULL);
	ao2_ref(cached_entry->snapshot, +1);
	return cached_entry->snapshot;
}

struct cache_dump_data {
	struct ao2_container *cached;
	struct stasis_message_type *type;
};

static int cache_dump_cb(void *obj, void *arg, int flags)
{
	struct cache_dump_data *cache_dump = arg;
	struct cache_entry *entry = obj;

	if (!cache_dump->type || entry->type == cache_dump->type) {
		ao2_link(cache_dump->cached, entry->snapshot);
	}

	return 0;
}

struct ao2_container *stasis_cache_dump(struct stasis_cache *cache, struct stasis_message_type *type)
{
	struct cache_dump_data cache_dump;

	ast_assert(cache->entries != NULL);

	cache_dump.type = type;
	cache_dump.cached = ao2_container_alloc_options(
		AO2_ALLOC_OPT_LOCK_NOLOCK, 1, NULL, NULL);
	if (!cache_dump.cached) {
		return NULL;
	}

	ao2_callback(cache->entries, OBJ_MULTIPLE | OBJ_NODATA, cache_dump_cb, &cache_dump);
	return cache_dump.cached;
}

STASIS_MESSAGE_TYPE_DEFN(stasis_cache_clear_type);
STASIS_MESSAGE_TYPE_DEFN(stasis_cache_update_type);

struct stasis_message *stasis_cache_clear_create(struct stasis_message *id_message)
{
	RAII_VAR(struct stasis_message *, msg, NULL, ao2_cleanup);

	msg = stasis_message_create(stasis_cache_clear_type(), id_message);
	if (!msg) {
		return NULL;
	}

	ao2_ref(msg, +1);
	return msg;
}

static void stasis_cache_update_dtor(void *obj)
{
	struct stasis_cache_update *update = obj;
	ao2_cleanup(update->topic);
	update->topic = NULL;
	ao2_cleanup(update->old_snapshot);
	update->old_snapshot = NULL;
	ao2_cleanup(update->new_snapshot);
	update->new_snapshot = NULL;
	ao2_cleanup(update->type);
	update->type = NULL;
}

static struct stasis_message *update_create(struct stasis_topic *topic, struct stasis_message *old_snapshot, struct stasis_message *new_snapshot)
{
	RAII_VAR(struct stasis_cache_update *, update, NULL, ao2_cleanup);
	RAII_VAR(struct stasis_message *, msg, NULL, ao2_cleanup);

	ast_assert(topic != NULL);
	ast_assert(old_snapshot != NULL || new_snapshot != NULL);

	update = ao2_alloc_options(sizeof(*update), stasis_cache_update_dtor,
		AO2_ALLOC_OPT_LOCK_NOLOCK);
	if (!update) {
		return NULL;
	}

	ao2_ref(topic, +1);
	update->topic = topic;
	if (old_snapshot) {
		ao2_ref(old_snapshot, +1);
		update->old_snapshot = old_snapshot;
		if (!new_snapshot) {
			ao2_ref(stasis_message_type(old_snapshot), +1);
			update->type = stasis_message_type(old_snapshot);
		}
	}
	if (new_snapshot) {
		ao2_ref(new_snapshot, +1);
		update->new_snapshot = new_snapshot;
		ao2_ref(stasis_message_type(new_snapshot), +1);
		update->type = stasis_message_type(new_snapshot);
	}

	msg = stasis_message_create(stasis_cache_update_type(), update);
	if (!msg) {
		return NULL;
	}

	ao2_ref(msg, +1);
	return msg;
}

static void caching_topic_exec(void *data, struct stasis_subscription *sub,
	struct stasis_topic *topic, struct stasis_message *message)
{
	RAII_VAR(struct stasis_caching_topic *, caching_topic_needs_unref, NULL, ao2_cleanup);
	struct stasis_caching_topic *caching_topic = data;
	const char *id = NULL;

	ast_assert(caching_topic != NULL);
	ast_assert(caching_topic->topic != NULL);
	ast_assert(caching_topic->cache != NULL);
	ast_assert(caching_topic->cache->id_fn != NULL);

	if (stasis_subscription_final_message(sub, message)) {
		caching_topic_needs_unref = caching_topic;
	}

	/* Handle cache clear event */
	if (stasis_cache_clear_type() == stasis_message_type(message)) {
		RAII_VAR(struct stasis_message *, old_snapshot, NULL, ao2_cleanup);
		RAII_VAR(struct stasis_message *, update, NULL, ao2_cleanup);
		struct stasis_message *clear_msg = stasis_message_data(message);
		const char *clear_id = caching_topic->cache->id_fn(clear_msg);
		struct stasis_message_type *clear_type = stasis_message_type(clear_msg);

		ast_assert(clear_type != NULL);

		if (clear_id) {
			old_snapshot = cache_put(caching_topic->cache, clear_type, clear_id, NULL);
			if (old_snapshot) {
				update = update_create(topic, old_snapshot, NULL);
				stasis_publish(caching_topic->topic, update);
				return;
			}

			ast_log(LOG_ERROR,
				"Attempting to remove an item from the %s cache that isn't there: %s %s\n",
				stasis_topic_name(caching_topic->topic), stasis_message_type_name(clear_type), clear_id);
			return;
		}
	}

	id = caching_topic->cache->id_fn(message);
	if (id == NULL) {
		/* Object isn't cached; forward */
		stasis_forward_message(caching_topic->topic, topic, message);
	} else {
		/* Update the cache */
		RAII_VAR(struct stasis_message *, old_snapshot, NULL, ao2_cleanup);
		RAII_VAR(struct stasis_message *, update, NULL, ao2_cleanup);

		old_snapshot = cache_put(caching_topic->cache, stasis_message_type(message), id, message);

		update = update_create(topic, old_snapshot, message);
		if (update == NULL) {
			return;
		}

		stasis_publish(caching_topic->topic, update);
	}
}

struct stasis_caching_topic *stasis_caching_topic_create(struct stasis_topic *original_topic, struct stasis_cache *cache)
{
	RAII_VAR(struct stasis_caching_topic *, caching_topic, NULL, ao2_cleanup);
	struct stasis_subscription *sub;
	RAII_VAR(char *, new_name, NULL, free);
	int ret;

	ret = asprintf(&new_name, "%s-cached", stasis_topic_name(original_topic));
	if (ret < 0) {
		return NULL;
	}

	caching_topic = ao2_alloc_options(sizeof(*caching_topic),
		stasis_caching_topic_dtor, AO2_ALLOC_OPT_LOCK_NOLOCK);
	if (caching_topic == NULL) {
		return NULL;
	}

	caching_topic->topic = stasis_topic_create(new_name);
	if (caching_topic->topic == NULL) {
		return NULL;
	}

	ao2_ref(cache, +1);
	caching_topic->cache = cache;

	sub = internal_stasis_subscribe(original_topic, caching_topic_exec, caching_topic, 0);
	if (sub == NULL) {
		return NULL;
	}

	ao2_ref(original_topic, +1);
	caching_topic->original_topic = original_topic;

	/* This is for the reference contained in the subscription above */
	ao2_ref(caching_topic, +1);
	caching_topic->sub = sub;

	/* The subscription holds the reference, so no additional ref bump. */
	return caching_topic;
}

static void stasis_cache_cleanup(void)
{
	STASIS_MESSAGE_TYPE_CLEANUP(stasis_cache_clear_type);
	STASIS_MESSAGE_TYPE_CLEANUP(stasis_cache_update_type);
}

int stasis_cache_init(void)
{
	ast_register_cleanup(stasis_cache_cleanup);

	if (STASIS_MESSAGE_TYPE_INIT(stasis_cache_clear_type) != 0) {
		return -1;
	}

	if (STASIS_MESSAGE_TYPE_INIT(stasis_cache_update_type) != 0) {
		return -1;
	}

	return 0;
}

