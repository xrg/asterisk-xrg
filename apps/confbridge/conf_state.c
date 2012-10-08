/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2012, Terry Wilson
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
 *
 * Please follow coding guidelines
 * http://svn.digium.com/view/asterisk/trunk/doc/CODING-GUIDELINES
 */

/*! \file
 *
 * \brief Confbridge state handling
 *
 * \author\verbatim Terry Wilson <twilson@digium.com> \endverbatim
 *
 * This file contains functions that are used from multiple conf_state
 * files for handling stage change behavior.
 *
 * \ingroup applications
 */

/*** MODULEINFO
	<support_level>core</support_level>
 ***/

#include "asterisk.h"

#include "asterisk/logger.h"
#include "include/conf_state.h"
#include "include/confbridge.h"

void conf_invalid_event_fn(struct conference_bridge_user *cbu)
{
	ast_log(LOG_ERROR, "Invalid event for confbridge user '%s'\n", cbu->u_profile.name);
}

void conf_default_join_waitmarked(struct conference_bridge_user *cbu)
{
	conf_add_user_waiting(cbu->conference_bridge, cbu);
	conf_add_post_join_action(cbu, conf_handle_inactive_waitmarked);
}

void conf_default_leave_waitmarked(struct conference_bridge_user *cbu)
{
	conf_remove_user_waiting(cbu->conference_bridge, cbu);
}

void conf_change_state(struct conference_bridge_user *cbu, struct conference_state *newstate)
{
	ast_debug(1, "Changing conference '%s' state from %s to %s\n", cbu->conference_bridge->name, cbu->conference_bridge->state->name, newstate->name);
	if (cbu->conference_bridge->state->exit) {
		cbu->conference_bridge->state->exit(cbu);
	}
	cbu->conference_bridge->state = newstate;
	if (cbu->conference_bridge->state->entry) {
		cbu->conference_bridge->state->entry(cbu);
	}
}
