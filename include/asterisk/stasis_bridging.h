/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2013 Digium, Inc.
 *
 * Kinsey Moore <kmoore@digium.com>
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

#ifndef _STASIS_BRIDGING_H
#define _STASIS_BRIDGING_H

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#include "asterisk/stringfields.h"
#include "asterisk/utils.h"
#include "asterisk/lock.h"
#include "asterisk/linkedlists.h"
#include "asterisk/channel.h"
#include "asterisk/bridging.h"
#include "asterisk/pbx.h"

/*!
 * \brief Structure that contains a snapshot of information about a bridge
 */
struct ast_bridge_snapshot {
	AST_DECLARE_STRING_FIELDS(
		/*! Immutable bridge UUID. */
		AST_STRING_FIELD(uniqueid);
		/*! Bridge technology that is handling the bridge */
		AST_STRING_FIELD(technology);
		/*! Bridge subclass that is handling the bridge */
		AST_STRING_FIELD(subclass);
	);
	/*! AO2 container of bare channel uniqueid strings participating in the bridge.
	 * Allocated from ast_str_container_alloc() */
	struct ao2_container *channels;
	/*! Bridge flags to tweak behavior */
	struct ast_flags feature_flags;
	/*! Bridge capabilities */
	uint32_t capabilities;
	/*! Number of channels participating in the bridge */
	unsigned int num_channels;
	/*! Number of active channels in the bridge. */
	unsigned int num_active;
};

/*!
 * \since 12
 * \brief Generate a snapshot of the bridge state. This is an ao2 object, so
 * ao2_cleanup() to deallocate.
 *
 * \param bridge The bridge from which to generate a snapshot
 *
 * \retval AO2 refcounted snapshot on success
 * \retval NULL on error
 */
struct ast_bridge_snapshot *ast_bridge_snapshot_create(struct ast_bridge *bridge);

/*!
 * \since 12
 * \brief Message type for \ref ast_bridge_snapshot.
 *
 * \retval Message type for \ref ast_bridge_snapshot.
 */
struct stasis_message_type *ast_bridge_snapshot_type(void);

/*!
 * \since 12
 * \brief A topic which publishes the events for a particular bridge.
 *
 * If the given \a bridge is \c NULL, ast_bridge_topic_all() is returned.
 *
 * \param bridge Bridge for which to get a topic or \c NULL.
 *
 * \retval Topic for bridge's events.
 * \retval ast_bridge_topic_all() if \a bridge is \c NULL.
 */
struct stasis_topic *ast_bridge_topic(struct ast_bridge *bridge);

/*!
 * \since 12
 * \brief A topic which publishes the events for all bridges.
 * \retval Topic for all bridge events.
 */
struct stasis_topic *ast_bridge_topic_all(void);

/*!
 * \since 12
 * \brief A caching topic which caches \ref ast_bridge_snapshot messages from
 * ast_bridge_events_all(void).
 *
 * \retval Caching topic for all bridge events.
 */
struct stasis_caching_topic *ast_bridge_topic_all_cached(void);

/*!
 * \since 12
 * \brief Publish the state of a bridge
 *
 * \param bridge The bridge for which to publish state
 */
void ast_bridge_publish_state(struct ast_bridge *bridge);

/*! \brief Message representing the merge of two bridges */
struct ast_bridge_merge_message {
	struct ast_bridge_snapshot *from;	/*!< Bridge from which channels will be removed during the merge */
	struct ast_bridge_snapshot *to;		/*!< Bridge to which channels will be added during the merge */
};

/*!
 * \since 12
 * \brief Message type for \ref ast_bridge_merge_message.
 *
 * \retval Message type for \ref ast_bridge_merge_message.
 */
struct stasis_message_type *ast_bridge_merge_message_type(void);

/*!
 * \since 12
 * \brief Publish a bridge merge
 *
 * \param to The bridge to which channels are being added
 * \param from The bridge from which channels are being removed
 */
void ast_bridge_publish_merge(struct ast_bridge *to, struct ast_bridge *from);

/*!
 * \since 12
 * \brief Blob of data associated with a bridge.
 *
 * The \c blob is actually a JSON object of structured data. It has a "type" field
 * which contains the type string describing this blob.
 */
struct ast_bridge_blob {
	/*! Bridge blob is associated with (or NULL for global/all bridges) */
	struct ast_bridge_snapshot *bridge;
	/*! Channel blob is associated with (may be NULL for some messages) */
	struct ast_channel_snapshot *channel;
	/*! JSON blob of data */
	struct ast_json *blob;
};

/*!
 * \since 12
 * \brief Message type for \ref channel enter bridge blob messages.
 *
 * \retval Message type for \ref channel enter bridge blob messages.
 */
struct stasis_message_type *ast_channel_entered_bridge_type(void);

/*!
 * \since 12
 * \brief Message type for \ref channel leave bridge blob messages.
 *
 * \retval Message type for \ref channel leave bridge blob messages.
 */
struct stasis_message_type *ast_channel_left_bridge_type(void);

/*!
 * \since 12
 * \brief Creates a \ref ast_bridge_blob message.
 *
 * The \a blob JSON object requires a \c "type" field describing the blob. It
 * should also be treated as immutable and not modified after it is put into the
 * message.
 *
 * \param bridge Channel blob is associated with, or NULL for global/all bridges.
 * \param blob JSON object representing the data.
 * \return \ref ast_bridge_blob message.
 * \return \c NULL on error
 */
struct stasis_message *ast_bridge_blob_create(struct stasis_message_type *type,
	struct ast_bridge *bridge,
	struct ast_channel *chan,
	struct ast_json *blob);

/*!
 * \since 12
 * \brief Publish a bridge channel enter event
 *
 * \param bridge The bridge a channel entered
 * \param chan The channel that entered the bridge
 */
void ast_bridge_publish_enter(struct ast_bridge *bridge, struct ast_channel *chan);

/*!
 * \since 12
 * \brief Publish a bridge channel leave event
 *
 * \param bridge The bridge a channel left
 * \param chan The channel that left the bridge
 */
void ast_bridge_publish_leave(struct ast_bridge *bridge, struct ast_channel *chan);

/*!
 * \brief Build a JSON object from a \ref ast_bridge_snapshot.
 * \return JSON object representing bridge snapshot.
 * \return \c NULL on error
 */
struct ast_json *ast_bridge_snapshot_to_json(const struct ast_bridge_snapshot *snapshot);

/*!
 * \brief Pair showing a bridge snapshot and a specific channel snapshot belonging to the bridge
 */
struct ast_bridge_channel_snapshot_pair {
	struct ast_bridge_snapshot *bridge_snapshot;
	struct ast_channel_snapshot *channel_snapshot;
};

/*!
 * \brief Pair showing a bridge and a specific channel belonging to the bridge
 */
struct ast_bridge_channel_pair {
	struct ast_bridge *bridge;
	struct ast_channel *channel;
};

/*!
 * \since 12
 * \brief Message type for \ref ast_blind_transfer_message.
 *
 * \retval Message type for \ref ast_blind_transfer_message.
 */
struct stasis_message_type *ast_blind_transfer_type(void);

/*!
 * \brief Publish a blind transfer event
 *
 * \param is_external Whether the blind transfer was initiated externally (e.g. via AMI or native protocol)
 * \param result The success or failure of the transfer
 * \param to_transferee The bridge between the transferer and transferee plus the transferer channel
 * \param context The destination context for the blind transfer
 * \param exten The destination extension for the blind transfer
 */
void ast_bridge_publish_blind_transfer(int is_external, enum ast_transfer_result result,
		struct ast_bridge_channel_pair *to_transferee, const char *context, const char *exten);

enum ast_attended_transfer_dest_type {
	/*! The transfer failed, so there is no appropriate final state */
	AST_ATTENDED_TRANSFER_DEST_FAIL,
	/*! The transfer results in a single bridge remaining due to a merge or swap */
	AST_ATTENDED_TRANSFER_DEST_BRIDGE_MERGE,
	/*! The transfer results in a channel or bridge running an application */
	AST_ATTENDED_TRANSFER_DEST_APP,
	/*! The transfer results in both bridges remaining with a local channel linking them */
	AST_ATTENDED_TRANSFER_DEST_LINK,
};

/*!
 * \brief Message representing attended transfer
 */
struct ast_attended_transfer_message {
	/*! Result of the attended transfer */
	enum ast_transfer_result result;
	/*! Indicates if the transfer was initiated externally*/
	int is_external;
	/*! Bridge between transferer <-> transferee and the transferer channel in that bridge. May be NULL */
	struct ast_bridge_channel_snapshot_pair to_transferee;
	/*! Bridge between transferer <-> transfer target and the transferer channel in that bridge. May be NULL */
	struct ast_bridge_channel_snapshot_pair to_transfer_target;
	/*! Indicates the final state of the transfer */
	enum ast_attended_transfer_dest_type dest_type;
	union {
		/*! ID of the surviving bridge. Applicable for AST_ATTENDED_TRANSFER_DEST_BRIDGE_MERGE */
		char bridge[AST_UUID_STR_LEN];
		/*! Destination application of transfer. Applicable for AST_ATTENDED_TRANSFER_DEST_APP */
		char app[AST_MAX_APP];
		/*! Pair of local channels linking the bridges. Applicable for AST_ATTENDED_TRANSFER_DEST_LINK */
		struct ast_channel_snapshot *links[2];
	} dest;
};

/*!
 * \since 12
 * \brief Message type for \ref ast_attended_transfer_message.
 *
 * \retval Message type for \ref ast_attended_transfer_message.
 */
struct stasis_message_type *ast_attended_transfer_type(void);

/*!
 * \since 12
 * \brief Publish an attended transfer failure
 *
 * Publish an \ref ast_attended_transfer_message with the dest_type set to
 * \c AST_ATTENDED_TRANSFER_DEST_FAIL.
 *
 * \param is_external Indicates if the transfer was initiated externally
 * \param result The result of the transfer. Will always be a type of failure.
 * \param transferee The bridge between the transferer and transferees as well as the transferer channel from that bridge
 * \param target The bridge between the transferer and transfer targets as well as the transferer channel from that bridge
 */
void ast_bridge_publish_attended_transfer_fail(int is_external, enum ast_transfer_result result,
		struct ast_bridge_channel_pair *transferee, struct ast_bridge_channel_pair *target);

/*!
 * \since 12
 * \brief Publish an attended transfer that results in two bridges becoming one.
 *
 * Publish an \ref ast_attended_transfer_message with the dest_type set to
 * \c AST_ATTENDED_TRANSFER_DEST_BRIDGE_MERGE. This type of attended transfer results from
 * having two bridges involved and either
 *
 * \li Merging the two bridges together
 * \li Moving a channel from one bridge to the other, thus emptying a bridge
 *
 * In either case, two bridges enter, one leaves.
 *
 * \param is_external Indicates if the transfer was initiated externally
 * \param result The result of the transfer.
 * \param transferee The bridge between the transferer and transferees as well as the transferer channel from that bridge
 * \param target The bridge between the transferer and transfer targets as well as the transferer channel from that bridge
 * \param final_bridge The bridge that the parties end up in. Will be a bridge from the transferee or target pair.
 */
void ast_bridge_publish_attended_transfer_bridge_merge(int is_external, enum ast_transfer_result result,
		struct ast_bridge_channel_pair *transferee, struct ast_bridge_channel_pair *target,
		struct ast_bridge *final_bridge);

/*!
 * \since 12
 * \brief Publish an attended transfer that results in an application being run
 *
 * Publish an \ref ast_attended_transfer_message with the dest_type set to
 * \c AST_ATTENDED_TRANSFER_DEST_APP. This occurs when an attended transfer
 * results in either:
 *
 * \li A transferee channel leaving a bridge to run an app
 * \li A bridge of transferees running an app (via a local channel)
 *
 * \param is_external Indicates if the transfer was initiated externally
 * \param result The result of the transfer.
 * \param transferee The bridge between the transferer and transferees as well as the transferer channel from that bridge
 * \param target The bridge between the transferer and transfer targets as well as the transferer channel from that bridge
 * \param dest_app The application that the channel or bridge is running upon transfer completion.
 */
void ast_bridge_publish_attended_transfer_app(int is_external, enum ast_transfer_result result,
		struct ast_bridge_channel_pair *transferee, struct ast_bridge_channel_pair *target,
		const char *dest_app);

/*!
 * \since 12
 * \brief Publish an attended transfer that results in two bridges linked by a local channel
 *
 * Publish an \ref ast_attended_transfer_message with the dest_type set to
 * \c AST_ATTENDED_TRANSFER_DEST_LINK. This occurs when two bridges are involved
 * in an attended transfer, but their properties do not allow for the bridges to
 * merge or to have channels moved off of the bridge. An example of this occurs when
 * attempting to transfer a ConfBridge to another bridge.
 *
 * When this type of transfer occurs, the two bridges continue to exist after the
 * transfer and a local channel is used to link the two bridges together.
 *
 * \param is_external Indicates if the transfer was initiated externally
 * \param result The result of the transfer.
 * \param transferee The bridge between the transferer and transferees as well as the transferer channel from that bridge
 * \param target The bridge between the transferer and transfer targets as well as the transferer channel from that bridge
 * \param locals The local channels linking the bridges together.
 */
void ast_bridge_publish_attended_transfer_link(int is_external, enum ast_transfer_result result,
		struct ast_bridge_channel_pair *transferee, struct ast_bridge_channel_pair *target,
		struct ast_channel *locals[2]);

/*!
 * \brief Returns the most recent snapshot for the bridge.
 *
 * The returned pointer is AO2 managed, so ao2_cleanup() when you're done.
 *
 * \param bridge_id Uniqueid of the bridge for which to get the snapshot.
 * \return Most recent snapshot. ao2_cleanup() when done.
 * \return \c NULL if channel isn't in cache.
 */
struct ast_bridge_snapshot *ast_bridge_snapshot_get_latest(
	const char *bridge_id);

/*!
 * \brief Initialize the stasis bridging topic and message types
 * \retval 0 on success
 * \retval -1 on failure
 */
int ast_stasis_bridging_init(void);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif	/* _STASIS_BRIDGING_H */
