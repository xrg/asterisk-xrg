/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2013, Digium, Inc.
 *
 * Jonathan Rose <jrose@digium.com>
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
 * \brief Parking Core
 *
 * \author Jonathan Rose <jrose@digium.com>
 */

#include "asterisk.h"

ASTERISK_FILE_VERSION(__FILE__, "$Revision$")

#include "asterisk/_private.h"
#include "asterisk/astobj2.h"
#include "asterisk/pbx.h"
#include "asterisk/bridge.h"
#include "asterisk/parking.h"
#include "asterisk/channel.h"
#include "asterisk/_private.h"

/*! \brief Message type for parked calls */
STASIS_MESSAGE_TYPE_DEFN(ast_parked_call_type);

/*! \brief Topic for parking lots */
static struct stasis_topic *parking_topic;

/*! \brief The container for the parking provider */
static AO2_GLOBAL_OBJ_STATIC(parking_provider);

static void parking_stasis_cleanup(void)
{
	STASIS_MESSAGE_TYPE_CLEANUP(ast_parked_call_type);
	ao2_cleanup(parking_topic);
	parking_topic = NULL;
}

int ast_parking_stasis_init(void)
{
	if (STASIS_MESSAGE_TYPE_INIT(ast_parked_call_type)) {
		return -1;
	}

	parking_topic = stasis_topic_create("ast_parking");
	if (!parking_topic) {
		return -1;
	}
	ast_register_cleanup(parking_stasis_cleanup);
	return 0;
}

struct stasis_topic *ast_parking_topic(void)
{
	return parking_topic;
}

/*! \brief Destructor for parked_call_payload objects */
static void parked_call_payload_destructor(void *obj)
{
	struct ast_parked_call_payload *park_obj = obj;

	ao2_cleanup(park_obj->parkee);
	ao2_cleanup(park_obj->retriever);
	ast_string_field_free_memory(park_obj);
}

struct ast_parked_call_payload *ast_parked_call_payload_create(enum ast_parked_call_event_type event_type,
		struct ast_channel_snapshot *parkee_snapshot, const char *parker_dial_string,
		struct ast_channel_snapshot *retriever_snapshot, const char *parkinglot,
		unsigned int parkingspace, unsigned long int timeout,
		unsigned long int duration)
{
	RAII_VAR(struct ast_parked_call_payload *, payload, NULL, ao2_cleanup);

	payload = ao2_alloc(sizeof(*payload), parked_call_payload_destructor);
	if (!payload) {
		return NULL;
	}

	if (ast_string_field_init(payload, 32)) {
		return NULL;
	}

	payload->event_type = event_type;

	ao2_ref(parkee_snapshot, +1);
	payload->parkee = parkee_snapshot;

	if (retriever_snapshot) {
		ao2_ref(retriever_snapshot, +1);
		payload->retriever = retriever_snapshot;
	}

	if (parkinglot) {
		ast_string_field_set(payload, parkinglot, parkinglot);
	}

	if (parker_dial_string) {
		ast_string_field_set(payload, parker_dial_string, parker_dial_string);
	}

	payload->parkingspace = parkingspace;
	payload->timeout = timeout;
	payload->duration = duration;

	/* Bump the ref count by one since RAII_VAR is going to eat one when we leave. */
	ao2_ref(payload, +1);
	return payload;
}

struct ast_parking_bridge_feature_fn_table *ast_parking_get_bridge_features(void)
{
	return (struct ast_parking_bridge_feature_fn_table*)ao2_global_obj_ref(parking_provider);
}

/*! \brief A wrapper around the fn_table to ao2-ify it */
struct parking_provider_wrapper {
	struct ast_parking_bridge_feature_fn_table fn_table;
};

int ast_parking_register_bridge_features(struct ast_parking_bridge_feature_fn_table *fn_table)
{
	RAII_VAR(struct parking_provider_wrapper *, wrapper,
		ao2_global_obj_ref(parking_provider), ao2_cleanup);

	if (fn_table->module_version != PARKING_MODULE_VERSION) {
		ast_log(AST_LOG_WARNING, "Parking module provided incorrect parking module "
			"version: %d (expected: %d)\n", fn_table->module_version, PARKING_MODULE_VERSION);
		return -1;
	}

	if (wrapper) {
		ast_log(AST_LOG_WARNING, "Parking provider already registered by %s!\n",
			wrapper->fn_table.module_name);
		return -1;
	}

	wrapper = ao2_alloc(sizeof(*wrapper), NULL);
	if (!wrapper) {
		return -1;
	}
	wrapper->fn_table = *fn_table;

	ao2_global_obj_replace(parking_provider, wrapper);
	return 0;
}

int ast_parking_unregister_bridge_features(const char *module_name)
{
	RAII_VAR(struct parking_provider_wrapper *, wrapper,
		ao2_global_obj_ref(parking_provider), ao2_cleanup);

	if (!wrapper) {
		ast_log(AST_LOG_WARNING, "No parking provider to unregister\n");
		return -1;
	}

	if (strcmp(wrapper->fn_table.module_name, module_name)) {
		ast_log(AST_LOG_WARNING, "%s has not registered the parking provider\n", module_name);
		return -1;
	}

	ao2_global_obj_replace_unref(parking_provider, NULL);
	return 0;
}
