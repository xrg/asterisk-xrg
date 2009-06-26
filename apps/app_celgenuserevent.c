/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2008, Digium, Inc
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
 * \brief Generate User-Defined CEL event
 *
 * \author Steve Murphy
 *
 * \ingroup applications
 */

#include "asterisk.h"

ASTERISK_FILE_VERSION(__FILE__, "$Revision$")

#include "asterisk/module.h"
#include "asterisk/app.h"
#include "asterisk/channel.h"
#include "asterisk/cel.h"

/*** DOCUMENTATION
	<application name="CELGenUserEvent" language="en_US">
		<synopsis>
			Generates a CEL User Defined Event.
		</synopsis>
		<syntax>
			<parameter name="event-name" required="true">
				<argument name="event-name" required="true">
				</argument>
			</parameter>
		</syntax>
		<description>
			<para>A CEL event will be immediately generated by this channel, with the supplied name for a type.</para>
		</description>
	</application>
 ***/

static char *app = "CELGenUserEvent";

static int celgenuserevent_exec(struct ast_channel *chan, const char *data)
{
	int res = 0;
	char *parse;
	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(event);
		AST_APP_ARG(extra);
	);

	if (ast_strlen_zero(data)) {
		return 0;
	}

	parse = ast_strdupa(data);
	AST_STANDARD_APP_ARGS(args, parse);

	ast_cel_report_event(chan, AST_CEL_USER_DEFINED, args.event, args.extra, NULL);
	return res;
}

static int unload_module(void)
{
	int res;

	res = ast_unregister_application(app);

	ast_module_user_hangup_all();

	return res;
}

static int load_module(void)
{
	int res = ast_register_application_xml(app, celgenuserevent_exec);
	if (res) {
		return AST_MODULE_LOAD_DECLINE;
	} else {
		return AST_MODULE_LOAD_SUCCESS;
	}
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_DEFAULT, "Generate an User-Defined CEL event",
				.load = load_module,
				.unload = unload_module,
	);
