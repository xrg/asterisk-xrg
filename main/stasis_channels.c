/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2013, Digium, Inc.
 *
 * Matt Jordan <mjordan@digium.com>
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
 * \brief Stasis Messages and Data Types for Channel Objects
 *
 * \author \verbatim Matt Jordan <mjordan@digium.com> \endverbatim
 *
 */

/*** MODULEINFO
	<support_level>core</support_level>
 ***/

#include "asterisk.h"

ASTERISK_FILE_VERSION(__FILE__, "$Revision$")

#include "asterisk/stasis.h"
#include "asterisk/astobj2.h"
#include "asterisk/stasis_channels.h"

#define NUM_MULTI_CHANNEL_BLOB_BUCKETS 7

/*! \brief Message type for channel snapshot messages */
static struct stasis_message_type *channel_snapshot_type;

/*! \brief Message type for channel blob messages */
static struct stasis_message_type *channel_blob_type;

/*! \brief Message type for channel dial messages */
static struct stasis_message_type *channel_dial_type;

/*! \brief Topic for all channels */
struct stasis_topic *channel_topic_all;

/*! \brief Caching topic for all channels */
struct stasis_caching_topic *channel_topic_all_cached;

struct stasis_message_type *ast_channel_dial_type(void)
{
	return channel_dial_type;
}

struct stasis_message_type *ast_channel_blob_type(void)
{
	return channel_blob_type;
}

struct stasis_message_type *ast_channel_snapshot_type(void)
{
	return channel_snapshot_type;
}

struct stasis_topic *ast_channel_topic_all(void)
{
	return channel_topic_all;
}

struct stasis_caching_topic *ast_channel_topic_all_cached(void)
{
	return channel_topic_all_cached;
}

static const char *channel_snapshot_get_id(struct stasis_message *message)
{
	struct ast_channel_snapshot *snapshot;
	if (ast_channel_snapshot_type() != stasis_message_type(message)) {
		return NULL;
	}
	snapshot = stasis_message_data(message);
	return snapshot->uniqueid;
}

/*! \internal \brief Hash function for \ref ast_channel_snapshot objects */
static int channel_snapshot_hash_cb(const void *obj, const int flags)
{
	const struct ast_channel_snapshot *snapshot = obj;
	const char *name = (flags & OBJ_KEY) ? obj : snapshot->name;
	return ast_str_case_hash(name);
}

/*! \internal \brief Comparison function for \ref ast_channel_snapshot objects */
static int channel_snapshot_cmp_cb(void *obj, void *arg, int flags)
{
	struct ast_channel_snapshot *left = obj;
	struct ast_channel_snapshot *right = arg;
	const char *match = (flags & OBJ_KEY) ? arg : right->name;
	return strcasecmp(left->name, match) ? 0 : (CMP_MATCH | CMP_STOP);
}

static void channel_snapshot_dtor(void *obj)
{
	struct ast_channel_snapshot *snapshot = obj;
	ast_string_field_free_memory(snapshot);
}

struct ast_channel_snapshot *ast_channel_snapshot_create(struct ast_channel *chan)
{
	RAII_VAR(struct ast_channel_snapshot *, snapshot, NULL, ao2_cleanup);

	snapshot = ao2_alloc(sizeof(*snapshot), channel_snapshot_dtor);
	if (!snapshot || ast_string_field_init(snapshot, 1024)) {
		return NULL;
	}

	ast_string_field_set(snapshot, name, ast_channel_name(chan));
	ast_string_field_set(snapshot, accountcode, ast_channel_accountcode(chan));
	ast_string_field_set(snapshot, peeraccount, ast_channel_peeraccount(chan));
	ast_string_field_set(snapshot, userfield, ast_channel_userfield(chan));
	ast_string_field_set(snapshot, uniqueid, ast_channel_uniqueid(chan));
	ast_string_field_set(snapshot, linkedid, ast_channel_linkedid(chan));
	ast_string_field_set(snapshot, parkinglot, ast_channel_parkinglot(chan));
	ast_string_field_set(snapshot, hangupsource, ast_channel_hangupsource(chan));
	if (ast_channel_appl(chan)) {
		ast_string_field_set(snapshot, appl, ast_channel_appl(chan));
	}
	if (ast_channel_data(chan)) {
		ast_string_field_set(snapshot, data, ast_channel_data(chan));
	}
	ast_string_field_set(snapshot, context, ast_channel_context(chan));
	ast_string_field_set(snapshot, exten, ast_channel_exten(chan));

	ast_string_field_set(snapshot, caller_name,
		S_COR(ast_channel_caller(chan)->id.name.valid, ast_channel_caller(chan)->id.name.str, ""));
	ast_string_field_set(snapshot, caller_number,
		S_COR(ast_channel_caller(chan)->id.number.valid, ast_channel_caller(chan)->id.number.str, ""));

	ast_string_field_set(snapshot, connected_name,
		S_COR(ast_channel_connected(chan)->id.name.valid, ast_channel_connected(chan)->id.name.str, ""));
	ast_string_field_set(snapshot, connected_number,
		S_COR(ast_channel_connected(chan)->id.number.valid, ast_channel_connected(chan)->id.number.str, ""));

	snapshot->creationtime = ast_channel_creationtime(chan);
	snapshot->state = ast_channel_state(chan);
	snapshot->priority = ast_channel_priority(chan);
	snapshot->amaflags = ast_channel_amaflags(chan);
	snapshot->hangupcause = ast_channel_hangupcause(chan);
	snapshot->flags = *ast_channel_flags(chan);
	snapshot->caller_pres = ast_party_id_presentation(&ast_channel_caller(chan)->id);

	snapshot->manager_vars = ast_channel_get_manager_vars(chan);

	ao2_ref(snapshot, +1);
	return snapshot;
}

static void publish_message_for_channel_topics(struct stasis_message *message, struct ast_channel *chan)
{
	if (chan) {
		stasis_publish(ast_channel_topic(chan), message);
	} else {
		stasis_publish(ast_channel_topic_all(), message);
	}
}

static void channel_blob_dtor(void *obj)
{
	struct ast_channel_blob *event = obj;
	ao2_cleanup(event->snapshot);
	ast_json_unref(event->blob);
}

void ast_channel_publish_dial(struct ast_channel *caller, struct ast_channel *peer, const char *dialstring, const char *dialstatus)
{
	RAII_VAR(struct ast_multi_channel_blob *, payload, NULL, ao2_cleanup);
	RAII_VAR(struct stasis_message *, msg, NULL, ao2_cleanup);
	RAII_VAR(struct ast_json *, blob, NULL, ast_json_unref);
	struct ast_channel_snapshot *caller_snapshot;
	struct ast_channel_snapshot *peer_snapshot;

	ast_assert(peer != NULL);
	blob = ast_json_pack("{s: s, s: s, s: s}",
			     "type", "dial",
			     "dialstatus", S_OR(dialstatus, ""),
			     "dialstring", S_OR(dialstring, ""));
	if (!blob) {
		return;
	}
	payload = ast_multi_channel_blob_create(blob);
	if (!payload) {
		return;
	}

	if (caller) {
		caller_snapshot = ast_channel_snapshot_create(caller);
		if (!caller_snapshot) {
			return;
		}
		ast_multi_channel_blob_add_channel(payload, "caller", caller_snapshot);
	}

	peer_snapshot = ast_channel_snapshot_create(peer);
	if (!peer_snapshot) {
		return;
	}
	ast_multi_channel_blob_add_channel(payload, "peer", peer_snapshot);

	msg = stasis_message_create(ast_channel_dial_type(), payload);
	if (!msg) {
		return;
	}

	publish_message_for_channel_topics(msg, caller);
}

struct stasis_message *ast_channel_blob_create(struct ast_channel *chan,
					       struct ast_json *blob)
{
	RAII_VAR(struct ast_channel_blob *, obj, NULL, ao2_cleanup);
	RAII_VAR(struct stasis_message *, msg, NULL, ao2_cleanup);
	struct ast_json *type;

	ast_assert(blob != NULL);

	type = ast_json_object_get(blob, "type");
	if (type == NULL) {
		ast_log(LOG_ERROR, "Invalid ast_channel_blob; missing type field\n");
		return NULL;
	}

	obj = ao2_alloc(sizeof(*obj), channel_blob_dtor);
	if (!obj) {
		return NULL;
	}

	if (chan) {
		obj->snapshot = ast_channel_snapshot_create(chan);
		if (obj->snapshot == NULL) {
			return NULL;
		}
	}

	obj->blob = ast_json_ref(blob);

	msg = stasis_message_create(ast_channel_blob_type(), obj);
	if (!msg) {
		return NULL;
	}

	ao2_ref(msg, +1);
	return msg;
}

const char *ast_channel_blob_json_type(struct ast_channel_blob *obj)
{
	if (obj == NULL) {
		return NULL;
	}

	return ast_json_string_get(ast_json_object_get(obj->blob, "type"));
}

/*! \brief A channel snapshot wrapper object used in \ref ast_multi_channel_blob objects */
struct channel_role_snapshot {
	struct ast_channel_snapshot *snapshot;	/*!< A channel snapshot */
	char role[0];							/*!< The role assigned to the channel */
};

/*! \brief A multi channel blob data structure for multi_channel_blob stasis messages */
struct ast_multi_channel_blob {
	struct ao2_container *channel_snapshots;	/*!< A container holding the snapshots */
	struct ast_json *blob;						/*< A blob of JSON data */
};

/*! \internal \brief Standard comparison function for \ref channel_role_snapshot objects */
static int channel_role_single_cmp_cb(void *obj, void *arg, int flags)
{
	struct channel_role_snapshot *left = obj;
	struct channel_role_snapshot *right = arg;
	const char *match = (flags & OBJ_KEY) ? arg : right->role;
	return strcasecmp(left->role, match) ? 0 : (CMP_MATCH | CMP_STOP);
}

/*! \internal \brief Multi comparison function for \ref channel_role_snapshot objects */
static int channel_role_multi_cmp_cb(void *obj, void *arg, int flags)
{
	struct channel_role_snapshot *left = obj;
	struct channel_role_snapshot *right = arg;
	const char *match = (flags & OBJ_KEY) ? arg : right->role;
	return strcasecmp(left->role, match) ? 0 : (CMP_MATCH);
}

/*! \internal \brief Hash function for \ref channel_role_snapshot objects */
static int channel_role_hash_cb(const void *obj, const int flags)
{
	const struct channel_role_snapshot *snapshot = obj;
	const char *name = (flags & OBJ_KEY) ? obj : snapshot->role;
	return ast_str_case_hash(name);
}

/*! \internal \brief Destructor for \ref ast_multi_channel_blob objects */
static void multi_channel_blob_dtor(void *obj)
{
	struct ast_multi_channel_blob *multi_blob = obj;

	ao2_cleanup(multi_blob->channel_snapshots);
	ast_json_unref(multi_blob->blob);
}

struct ast_multi_channel_blob *ast_multi_channel_blob_create(struct ast_json *blob)
{
	RAII_VAR(struct ast_multi_channel_blob *, obj,
			ao2_alloc(sizeof(*obj), multi_channel_blob_dtor),
			ao2_cleanup);
	struct ast_json *type;

	ast_assert(blob != NULL);

	if (!obj) {
		return NULL;
	}

	type = ast_json_object_get(blob, "type");
	if (type == NULL) {
		ast_log(LOG_ERROR, "Invalid ast_multi_channel_blob; missing type field\n");
		return NULL;
	}

	obj->channel_snapshots = ao2_container_alloc(NUM_MULTI_CHANNEL_BLOB_BUCKETS,
			channel_role_hash_cb, channel_role_single_cmp_cb);
	if (!obj->channel_snapshots) {
		return NULL;
	}

	obj->blob = ast_json_ref(blob);

	ao2_ref(obj, +1);
	return obj;
}

static void channel_role_snapshot_dtor(void *obj)
{
	struct channel_role_snapshot *role_snapshot = obj;
	ao2_cleanup(role_snapshot->snapshot);
}

void ast_multi_channel_blob_add_channel(struct ast_multi_channel_blob *obj, const char *role, struct ast_channel_snapshot *snapshot)
{
	RAII_VAR(struct channel_role_snapshot *, role_snapshot, NULL, ao2_cleanup);
	int role_len = strlen(role) + 1;

	if (!obj || ast_strlen_zero(role) || !snapshot) {
		return;
	}

	role_snapshot = ao2_alloc(sizeof(*role_snapshot) + role_len, channel_role_snapshot_dtor);
	if (!role_snapshot) {
		return;
	}
	ast_copy_string(role_snapshot->role, role, role_len);
	role_snapshot->snapshot = snapshot;
	ao2_ref(role_snapshot->snapshot, +1);
	ao2_link(obj->channel_snapshots, role_snapshot);
}

struct ast_channel_snapshot *ast_multi_channel_blob_get_channel(struct ast_multi_channel_blob *obj, const char *role)
{
	struct channel_role_snapshot *role_snapshot;

	if (!obj || ast_strlen_zero(role)) {
		return NULL;
	}
	role_snapshot = ao2_find(obj->channel_snapshots, role, OBJ_KEY);
	/* Note that this function does not increase the ref count on snapshot */
	if (!role_snapshot) {
		return NULL;
	}
	ao2_ref(role_snapshot, -1);
	return role_snapshot->snapshot;
}

struct ao2_container *ast_multi_channel_blob_get_channels(struct ast_multi_channel_blob *obj, const char *role)
{
	RAII_VAR(struct ao2_container *, ret_container,
		ao2_container_alloc(NUM_MULTI_CHANNEL_BLOB_BUCKETS, channel_snapshot_hash_cb, channel_snapshot_cmp_cb),
		ao2_cleanup);
	struct ao2_iterator *it_role_snapshots;
	struct channel_role_snapshot *role_snapshot;
	char *arg;

	if (!obj || ast_strlen_zero(role) || !ret_container) {
		return NULL;
	}
	arg = ast_strdupa(role);

	it_role_snapshots = ao2_callback(obj->channel_snapshots, OBJ_MULTIPLE | OBJ_KEY, channel_role_multi_cmp_cb, arg);
	if (!it_role_snapshots) {
		return NULL;
	}

	while ((role_snapshot = ao2_iterator_next(it_role_snapshots))) {
		ao2_link(ret_container, role_snapshot->snapshot);
		ao2_ref(role_snapshot, -1);
	}
	ao2_iterator_destroy(it_role_snapshots);

	ao2_ref(ret_container, +1);
	return ret_container;
}

struct ast_json *ast_multi_channel_blob_get_json(struct ast_multi_channel_blob *obj)
{
	if (!obj) {
		return NULL;
	}
	return obj->blob;
}

const char *ast_multi_channel_blob_get_type(struct ast_multi_channel_blob *obj)
{
	if (!obj) {
		return NULL;
	}

	return ast_json_string_get(ast_json_object_get(obj->blob, "type"));
}

void ast_channel_publish_varset(struct ast_channel *chan, const char *name, const char *value)
{
	RAII_VAR(struct stasis_message *, msg, NULL, ao2_cleanup);
	RAII_VAR(struct ast_json *, blob, NULL, ast_json_unref);

	ast_assert(name != NULL);
	ast_assert(value != NULL);

	blob = ast_json_pack("{s: s, s: s, s: s}",
			     "type", "varset",
			     "variable", name,
			     "value", value);
	if (!blob) {
		ast_log(LOG_ERROR, "Error creating message\n");
		return;
	}

	msg = ast_channel_blob_create(chan, ast_json_ref(blob));

	if (!msg) {
		return;
	}

	publish_message_for_channel_topics(msg, chan);
}

void ast_stasis_channels_shutdown(void)
{
	ao2_cleanup(channel_snapshot_type);
	channel_snapshot_type = NULL;
	ao2_cleanup(channel_blob_type);
	channel_blob_type = NULL;
	ao2_cleanup(channel_dial_type);
	channel_dial_type = NULL;
	ao2_cleanup(channel_topic_all);
	channel_topic_all = NULL;
	channel_topic_all_cached = stasis_caching_unsubscribe(channel_topic_all_cached);
}

void ast_stasis_channels_init(void)
{
	channel_snapshot_type = stasis_message_type_create("ast_channel_snapshot");
	channel_blob_type = stasis_message_type_create("ast_channel_blob");
	channel_dial_type = stasis_message_type_create("ast_channel_dial");
	channel_topic_all = stasis_topic_create("ast_channel_topic_all");
	channel_topic_all_cached = stasis_caching_topic_create(channel_topic_all, channel_snapshot_get_id);
}
