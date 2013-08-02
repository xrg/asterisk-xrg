/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 1999 - 2005, Digium, Inc.
 *
 * Mark Spencer <markster@digium.com>
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
 * \brief Call Parking and Pickup API
 * Includes code and algorithms from the Zapata library.
 */

#ifndef _AST_FEATURES_H
#define _AST_FEATURES_H

#include "asterisk/pbx.h"
#include "asterisk/linkedlists.h"
#include "asterisk/bridge.h"

/*! \brief main call feature structure */

enum {
	AST_FEATURE_FLAG_NEEDSDTMF = (1 << 0),
	AST_FEATURE_FLAG_ONPEER =    (1 << 1),
	AST_FEATURE_FLAG_ONSELF =    (1 << 2),
	AST_FEATURE_FLAG_BYCALLEE =  (1 << 3),
	AST_FEATURE_FLAG_BYCALLER =  (1 << 4),
	AST_FEATURE_FLAG_BYBOTH	 =   (3 << 3),
};

/*! \brief Bridge a call, optionally allowing redirection */
int ast_bridge_call(struct ast_channel *chan, struct ast_channel *peer,struct ast_bridge_config *config);

/*!
 * \brief Add an arbitrary channel to a bridge
 * \since 12.0.0
 *
 * The channel that is being added to the bridge can be in any state: unbridged,
 * bridged, answered, unanswered, etc. The channel will be added asynchronously,
 * meaning that when this function returns once the channel has been added to
 * the bridge, not once the channel has been removed from the bridge.
 *
 * In addition, a tone can optionally be played to the channel once the
 * channel is placed into the bridge.
 *
 * \note When this function returns, there is no guarantee that the channel that
 * was passed in is valid any longer. Do not attempt to operate on the channel
 * after this function returns.
 *
 * \param bridge Bridge to which the channel should be added
 * \param chan The channel to add to the bridge
 * \param features Features for this channel in the bridge
 * \param play_tone Indicates if a tone should be played to the channel
 * \param xfersound Sound that should be used to indicate transfer with play_tone
 * \retval 0 Success
 * \retval -1 Failure
 */
int ast_bridge_add_channel(struct ast_bridge *bridge, struct ast_channel *chan,
		struct ast_bridge_features *features, int play_tone, const char *xfersound);



/*!
 * \brief parse L option and read associated channel variables to set warning, warning frequency, and timelimit
 * \note caller must be aware of freeing memory for warning_sound, end_sound, and start_sound
*/
int ast_bridge_timelimit(struct ast_channel *chan, struct ast_bridge_config *config, char *parse, struct timeval *calldurationlimit);

#endif /* _AST_FEATURES_H */
