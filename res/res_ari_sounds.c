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

/*
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * !!!!!                               DO NOT EDIT                        !!!!!
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * This file is generated by a mustache template. Please see the original
 * template in rest-api-templates/res_ari_resource.c.mustache
 */

/*! \file
 *
 * \brief Sound resources
 *
 * \author David M. Lee, II <dlee@digium.com>
 */

/*** MODULEINFO
	<depend type="module">res_ari</depend>
	<depend type="module">res_stasis</depend>
	<support_level>core</support_level>
 ***/

#include "asterisk.h"

ASTERISK_FILE_VERSION(__FILE__, "$Revision$")

#include "asterisk/app.h"
#include "asterisk/module.h"
#include "asterisk/stasis_app.h"
#include "ari/resource_sounds.h"
#if defined(AST_DEVMODE)
#include "ari/ari_model_validators.h"
#endif

#define MAX_VALS 128

/*!
 * \brief Parameter parsing callback for /sounds.
 * \param get_params GET parameters in the HTTP request.
 * \param path_vars Path variables extracted from the request.
 * \param headers HTTP headers.
 * \param[out] response Response to the HTTP request.
 */
static void ast_ari_sounds_list_cb(
	struct ast_variable *get_params, struct ast_variable *path_vars,
	struct ast_variable *headers, struct ast_ari_response *response)
{
	struct ast_ari_sounds_list_args args = {};
	struct ast_variable *i;
#if defined(AST_DEVMODE)
	int is_valid;
	int code;
#endif /* AST_DEVMODE */

	for (i = get_params; i; i = i->next) {
		if (strcmp(i->name, "lang") == 0) {
			args.lang = (i->value);
		} else
		if (strcmp(i->name, "format") == 0) {
			args.format = (i->value);
		} else
		{}
	}
	ast_ari_sounds_list(headers, &args, response);
#if defined(AST_DEVMODE)
	code = response->response_code;

	switch (code) {
	case 0: /* Implementation is still a stub, or the code wasn't set */
		is_valid = response->message == NULL;
		break;
	case 500: /* Internal Server Error */
	case 501: /* Not Implemented */
		is_valid = 1;
		break;
	default:
		if (200 <= code && code <= 299) {
			is_valid = ast_ari_validate_list(response->message,
				ast_ari_validate_sound_fn());
		} else {
			ast_log(LOG_ERROR, "Invalid error response %d for /sounds\n", code);
			is_valid = 0;
		}
	}

	if (!is_valid) {
		ast_log(LOG_ERROR, "Response validation failed for /sounds\n");
		ast_ari_response_error(response, 500,
			"Internal Server Error", "Response validation failed");
	}
#endif /* AST_DEVMODE */

fin: __attribute__((unused))
	return;
}
/*!
 * \brief Parameter parsing callback for /sounds/{soundId}.
 * \param get_params GET parameters in the HTTP request.
 * \param path_vars Path variables extracted from the request.
 * \param headers HTTP headers.
 * \param[out] response Response to the HTTP request.
 */
static void ast_ari_sounds_get_cb(
	struct ast_variable *get_params, struct ast_variable *path_vars,
	struct ast_variable *headers, struct ast_ari_response *response)
{
	struct ast_ari_sounds_get_args args = {};
	struct ast_variable *i;
#if defined(AST_DEVMODE)
	int is_valid;
	int code;
#endif /* AST_DEVMODE */

	for (i = path_vars; i; i = i->next) {
		if (strcmp(i->name, "soundId") == 0) {
			args.sound_id = (i->value);
		} else
		{}
	}
	ast_ari_sounds_get(headers, &args, response);
#if defined(AST_DEVMODE)
	code = response->response_code;

	switch (code) {
	case 0: /* Implementation is still a stub, or the code wasn't set */
		is_valid = response->message == NULL;
		break;
	case 500: /* Internal Server Error */
	case 501: /* Not Implemented */
		is_valid = 1;
		break;
	default:
		if (200 <= code && code <= 299) {
			is_valid = ast_ari_validate_sound(
				response->message);
		} else {
			ast_log(LOG_ERROR, "Invalid error response %d for /sounds/{soundId}\n", code);
			is_valid = 0;
		}
	}

	if (!is_valid) {
		ast_log(LOG_ERROR, "Response validation failed for /sounds/{soundId}\n");
		ast_ari_response_error(response, 500,
			"Internal Server Error", "Response validation failed");
	}
#endif /* AST_DEVMODE */

fin: __attribute__((unused))
	return;
}

/*! \brief REST handler for /api-docs/sounds.{format} */
static struct stasis_rest_handlers sounds_soundId = {
	.path_segment = "soundId",
	.is_wildcard = 1,
	.callbacks = {
		[AST_HTTP_GET] = ast_ari_sounds_get_cb,
	},
	.num_children = 0,
	.children = {  }
};
/*! \brief REST handler for /api-docs/sounds.{format} */
static struct stasis_rest_handlers sounds = {
	.path_segment = "sounds",
	.callbacks = {
		[AST_HTTP_GET] = ast_ari_sounds_list_cb,
	},
	.num_children = 1,
	.children = { &sounds_soundId, }
};

static int load_module(void)
{
	int res = 0;
	stasis_app_ref();
	res |= ast_ari_add_handler(&sounds);
	return res;
}

static int unload_module(void)
{
	ast_ari_remove_handler(&sounds);
	stasis_app_unref();
	return 0;
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_DEFAULT, "RESTful API module - Sound resources",
	.load = load_module,
	.unload = unload_module,
	.nonoptreq = "res_ari,res_stasis",
	);
