/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2013, Digium, Inc.
 *
 * Joshua Colp <jcolp@digium.com>
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
 * \author Joshua Colp <jcolp@digium.com>
 *
 * \brief PSJIP SIP Channel Driver
 *
 * \ingroup channel_drivers
 */

/*** MODULEINFO
	<depend>pjproject</depend>
	<depend>res_pjsip</depend>
	<depend>res_pjsip_session</depend>
	<support_level>core</support_level>
 ***/

#include "asterisk.h"

#include <pjsip.h>
#include <pjsip_ua.h>
#include <pjlib.h>

ASTERISK_FILE_VERSION(__FILE__, "$Revision$")

#include "asterisk/lock.h"
#include "asterisk/channel.h"
#include "asterisk/module.h"
#include "asterisk/pbx.h"
#include "asterisk/rtp_engine.h"
#include "asterisk/acl.h"
#include "asterisk/callerid.h"
#include "asterisk/file.h"
#include "asterisk/cli.h"
#include "asterisk/app.h"
#include "asterisk/musiconhold.h"
#include "asterisk/causes.h"
#include "asterisk/taskprocessor.h"
#include "asterisk/dsp.h"
#include "asterisk/stasis_endpoints.h"
#include "asterisk/stasis_channels.h"
#include "asterisk/indications.h"

#include "asterisk/res_pjsip.h"
#include "asterisk/res_pjsip_session.h"

#include "pjsip/include/chan_pjsip.h"
#include "pjsip/include/dialplan_functions.h"

static const char desc[] = "PJSIP Channel";
static const char channel_type[] = "PJSIP";

static unsigned int chan_idx;

static void chan_pjsip_pvt_dtor(void *obj)
{
	struct chan_pjsip_pvt *pvt = obj;
	int i;

	for (i = 0; i < SIP_MEDIA_SIZE; ++i) {
		ao2_cleanup(pvt->media[i]);
		pvt->media[i] = NULL;
	}
}

/* \brief Asterisk core interaction functions */
static struct ast_channel *chan_pjsip_request(const char *type, struct ast_format_cap *cap, const struct ast_channel *requestor, const char *data, int *cause);
static int chan_pjsip_sendtext(struct ast_channel *ast, const char *text);
static int chan_pjsip_digit_begin(struct ast_channel *ast, char digit);
static int chan_pjsip_digit_end(struct ast_channel *ast, char digit, unsigned int duration);
static int chan_pjsip_call(struct ast_channel *ast, const char *dest, int timeout);
static int chan_pjsip_hangup(struct ast_channel *ast);
static int chan_pjsip_answer(struct ast_channel *ast);
static struct ast_frame *chan_pjsip_read(struct ast_channel *ast);
static int chan_pjsip_write(struct ast_channel *ast, struct ast_frame *f);
static int chan_pjsip_indicate(struct ast_channel *ast, int condition, const void *data, size_t datalen);
static int chan_pjsip_transfer(struct ast_channel *ast, const char *target);
static int chan_pjsip_fixup(struct ast_channel *oldchan, struct ast_channel *newchan);
static int chan_pjsip_devicestate(const char *data);
static int chan_pjsip_queryoption(struct ast_channel *ast, int option, void *data, int *datalen);

/*! \brief PBX interface structure for channel registration */
struct ast_channel_tech chan_pjsip_tech = {
	.type = channel_type,
	.description = "PJSIP Channel Driver",
	.requester = chan_pjsip_request,
	.send_text = chan_pjsip_sendtext,
	.send_digit_begin = chan_pjsip_digit_begin,
	.send_digit_end = chan_pjsip_digit_end,
	.call = chan_pjsip_call,
	.hangup = chan_pjsip_hangup,
	.answer = chan_pjsip_answer,
	.read = chan_pjsip_read,
	.write = chan_pjsip_write,
	.write_video = chan_pjsip_write,
	.exception = chan_pjsip_read,
	.indicate = chan_pjsip_indicate,
	.transfer = chan_pjsip_transfer,
	.fixup = chan_pjsip_fixup,
	.devicestate = chan_pjsip_devicestate,
	.queryoption = chan_pjsip_queryoption,
	.func_channel_read = pjsip_acf_channel_read,
	.properties = AST_CHAN_TP_WANTSJITTER | AST_CHAN_TP_CREATESJITTER
};

/*! \brief SIP session interaction functions */
static void chan_pjsip_session_begin(struct ast_sip_session *session);
static void chan_pjsip_session_end(struct ast_sip_session *session);
static int chan_pjsip_incoming_request(struct ast_sip_session *session, struct pjsip_rx_data *rdata);
static void chan_pjsip_incoming_response(struct ast_sip_session *session, struct pjsip_rx_data *rdata);

/*! \brief SIP session supplement structure */
static struct ast_sip_session_supplement chan_pjsip_supplement = {
	.method = "INVITE",
	.priority = AST_SIP_SESSION_SUPPLEMENT_PRIORITY_CHANNEL,
	.session_begin = chan_pjsip_session_begin,
	.session_end = chan_pjsip_session_end,
	.incoming_request = chan_pjsip_incoming_request,
	.incoming_response = chan_pjsip_incoming_response,
};

static int chan_pjsip_incoming_ack(struct ast_sip_session *session, struct pjsip_rx_data *rdata);

static struct ast_sip_session_supplement chan_pjsip_ack_supplement = {
	.method = "ACK",
	.priority = AST_SIP_SESSION_SUPPLEMENT_PRIORITY_CHANNEL,
	.incoming_request = chan_pjsip_incoming_ack,
};

/*! \brief Function called by RTP engine to get local audio RTP peer */
static enum ast_rtp_glue_result chan_pjsip_get_rtp_peer(struct ast_channel *chan, struct ast_rtp_instance **instance)
{
	struct ast_sip_channel_pvt *channel = ast_channel_tech_pvt(chan);
	struct chan_pjsip_pvt *pvt = channel->pvt;
	struct ast_sip_endpoint *endpoint;

	if (!pvt || !channel->session || !pvt->media[SIP_MEDIA_AUDIO]->rtp) {
		return AST_RTP_GLUE_RESULT_FORBID;
	}

	endpoint = channel->session->endpoint;

	*instance = pvt->media[SIP_MEDIA_AUDIO]->rtp;
	ao2_ref(*instance, +1);

	ast_assert(endpoint != NULL);
	if (endpoint->media.rtp.encryption != AST_SIP_MEDIA_ENCRYPT_NONE) {
		return AST_RTP_GLUE_RESULT_FORBID;
	}

	if (endpoint->media.direct_media.enabled) {
		return AST_RTP_GLUE_RESULT_REMOTE;
	}

	return AST_RTP_GLUE_RESULT_LOCAL;
}

/*! \brief Function called by RTP engine to get local video RTP peer */
static enum ast_rtp_glue_result chan_pjsip_get_vrtp_peer(struct ast_channel *chan, struct ast_rtp_instance **instance)
{
	struct ast_sip_channel_pvt *channel = ast_channel_tech_pvt(chan);
	struct chan_pjsip_pvt *pvt = channel->pvt;
	struct ast_sip_endpoint *endpoint;

	if (!pvt || !channel->session || !pvt->media[SIP_MEDIA_VIDEO]->rtp) {
		return AST_RTP_GLUE_RESULT_FORBID;
	}

	endpoint = channel->session->endpoint;

	*instance = pvt->media[SIP_MEDIA_VIDEO]->rtp;
	ao2_ref(*instance, +1);

	ast_assert(endpoint != NULL);
	if (endpoint->media.rtp.encryption != AST_SIP_MEDIA_ENCRYPT_NONE) {
		return AST_RTP_GLUE_RESULT_FORBID;
	}

	return AST_RTP_GLUE_RESULT_LOCAL;
}

/*! \brief Function called by RTP engine to get peer capabilities */
static void chan_pjsip_get_codec(struct ast_channel *chan, struct ast_format_cap *result)
{
	struct ast_sip_channel_pvt *channel = ast_channel_tech_pvt(chan);

	ast_format_cap_copy(result, channel->session->endpoint->media.codecs);
}

static int send_direct_media_request(void *data)
{
	RAII_VAR(struct ast_sip_session *, session, data, ao2_cleanup);

	return ast_sip_session_refresh(session, NULL, NULL, NULL,
			session->endpoint->media.direct_media.method, 1);
}

/*! \brief Destructor function for \ref transport_info_data */
static void transport_info_destroy(void *obj)
{
	struct transport_info_data *data = obj;
	ast_free(data);
}

/*! \brief Datastore used to store local/remote addresses for the
 * INVITE request that created the PJSIP channel */
static struct ast_datastore_info transport_info = {
	.type = "chan_pjsip_transport_info",
	.destroy = transport_info_destroy,
};

static struct ast_datastore_info direct_media_mitigation_info = { };

static int direct_media_mitigate_glare(struct ast_sip_session *session)
{
	RAII_VAR(struct ast_datastore *, datastore, NULL, ao2_cleanup);

	if (session->endpoint->media.direct_media.glare_mitigation ==
			AST_SIP_DIRECT_MEDIA_GLARE_MITIGATION_NONE) {
		return 0;
	}

	datastore = ast_sip_session_get_datastore(session, "direct_media_glare_mitigation");
	if (!datastore) {
		return 0;
	}

	/* Removing the datastore ensures we won't try to mitigate glare on subsequent reinvites */
	ast_sip_session_remove_datastore(session, "direct_media_glare_mitigation");

	if ((session->endpoint->media.direct_media.glare_mitigation ==
			AST_SIP_DIRECT_MEDIA_GLARE_MITIGATION_OUTGOING &&
			session->inv_session->role == PJSIP_ROLE_UAC) ||
			(session->endpoint->media.direct_media.glare_mitigation ==
			AST_SIP_DIRECT_MEDIA_GLARE_MITIGATION_INCOMING &&
			session->inv_session->role == PJSIP_ROLE_UAS)) {
		return 1;
	}

	return 0;
}

static int check_for_rtp_changes(struct ast_channel *chan, struct ast_rtp_instance *rtp,
		struct ast_sip_session_media *media, int rtcp_fd)
{
	int changed = 0;

	if (rtp) {
		changed = ast_rtp_instance_get_and_cmp_remote_address(rtp, &media->direct_media_addr);
		if (media->rtp) {
			ast_channel_set_fd(chan, rtcp_fd, -1);
			ast_rtp_instance_set_prop(media->rtp, AST_RTP_PROPERTY_RTCP, 0);
		}
	} else if (!ast_sockaddr_isnull(&media->direct_media_addr)){
		ast_sockaddr_setnull(&media->direct_media_addr);
		changed = 1;
		if (media->rtp) {
			ast_rtp_instance_set_prop(media->rtp, AST_RTP_PROPERTY_RTCP, 1);
			ast_channel_set_fd(chan, rtcp_fd, ast_rtp_instance_fd(media->rtp, 1));
		}
	}

	return changed;
}

/*! \brief Function called by RTP engine to change where the remote party should send media */
static int chan_pjsip_set_rtp_peer(struct ast_channel *chan,
		struct ast_rtp_instance *rtp,
		struct ast_rtp_instance *vrtp,
		struct ast_rtp_instance *tpeer,
		const struct ast_format_cap *cap,
		int nat_active)
{
	struct ast_sip_channel_pvt *channel = ast_channel_tech_pvt(chan);
	struct chan_pjsip_pvt *pvt = channel->pvt;
	struct ast_sip_session *session = channel->session;
	int changed = 0;

	/* Don't try to do any direct media shenanigans on early bridges */
	if ((rtp || vrtp || tpeer) && !ast_channel_is_bridged(chan)) {
		return 0;
	}

	if (nat_active && session->endpoint->media.direct_media.disable_on_nat) {
		return 0;
	}

	if (pvt->media[SIP_MEDIA_AUDIO]) {
		changed |= check_for_rtp_changes(chan, rtp, pvt->media[SIP_MEDIA_AUDIO], 1);
	}
	if (pvt->media[SIP_MEDIA_VIDEO]) {
		changed |= check_for_rtp_changes(chan, vrtp, pvt->media[SIP_MEDIA_VIDEO], 3);
	}

	if (direct_media_mitigate_glare(session)) {
		return 0;
	}

	if (cap && !ast_format_cap_is_empty(cap) && !ast_format_cap_identical(session->direct_media_cap, cap)) {
		ast_format_cap_copy(session->direct_media_cap, cap);
		changed = 1;
	}

	if (changed) {
		ao2_ref(session, +1);


		if (ast_sip_push_task(session->serializer, send_direct_media_request, session)) {
			ao2_cleanup(session);
		}
	}

	return 0;
}

/*! \brief Local glue for interacting with the RTP engine core */
static struct ast_rtp_glue chan_pjsip_rtp_glue = {
	.type = "PJSIP",
	.get_rtp_info = chan_pjsip_get_rtp_peer,
	.get_vrtp_info = chan_pjsip_get_vrtp_peer,
	.get_codec = chan_pjsip_get_codec,
	.update_peer = chan_pjsip_set_rtp_peer,
};

/*! \brief Function called to create a new PJSIP Asterisk channel */
static struct ast_channel *chan_pjsip_new(struct ast_sip_session *session, int state, const char *exten, const char *title, const char *linkedid, const char *cid_name)
{
	struct ast_channel *chan;
	struct ast_format fmt;
	RAII_VAR(struct chan_pjsip_pvt *, pvt, NULL, ao2_cleanup);
	struct ast_sip_channel_pvt *channel;

	if (!(pvt = ao2_alloc(sizeof(*pvt), chan_pjsip_pvt_dtor))) {
		return NULL;
	}

	if (!(chan = ast_channel_alloc(1, state, S_OR(session->id.number.str, ""), S_OR(session->id.name.str, ""), "", "", "", linkedid, 0, "PJSIP/%s-%08x", ast_sorcery_object_get_id(session->endpoint),
		ast_atomic_fetchadd_int((int *)&chan_idx, +1)))) {
		return NULL;
	}

	ast_channel_tech_set(chan, &chan_pjsip_tech);

	if (!(channel = ast_sip_channel_pvt_alloc(pvt, session))) {
		ast_hangup(chan);
		return NULL;
	}

	ast_channel_stage_snapshot(chan);

	/* If res_pjsip_session is ever updated to create/destroy ast_sip_session_media
	 * during a call such as if multiple same-type stream support is introduced,
	 * these will need to be recaptured as well */
	pvt->media[SIP_MEDIA_AUDIO] = ao2_find(session->media, "audio", OBJ_KEY);
	pvt->media[SIP_MEDIA_VIDEO] = ao2_find(session->media, "video", OBJ_KEY);
	ast_channel_tech_pvt_set(chan, channel);
	if (pvt->media[SIP_MEDIA_AUDIO] && pvt->media[SIP_MEDIA_AUDIO]->rtp) {
		ast_rtp_instance_set_channel_id(pvt->media[SIP_MEDIA_AUDIO]->rtp, ast_channel_uniqueid(chan));
	}
	if (pvt->media[SIP_MEDIA_VIDEO] && pvt->media[SIP_MEDIA_VIDEO]->rtp) {
		ast_rtp_instance_set_channel_id(pvt->media[SIP_MEDIA_VIDEO]->rtp, ast_channel_uniqueid(chan));
	}

	if (ast_format_cap_is_empty(session->req_caps) || !ast_format_cap_has_joint(session->req_caps, session->endpoint->media.codecs)) {
		ast_format_cap_copy(ast_channel_nativeformats(chan), session->endpoint->media.codecs);
	} else {
		ast_format_cap_copy(ast_channel_nativeformats(chan), session->req_caps);
	}

	ast_codec_choose(&session->endpoint->media.prefs, ast_channel_nativeformats(chan), 1, &fmt);
	ast_format_copy(ast_channel_writeformat(chan), &fmt);
	ast_format_copy(ast_channel_rawwriteformat(chan), &fmt);
	ast_format_copy(ast_channel_readformat(chan), &fmt);
	ast_format_copy(ast_channel_rawreadformat(chan), &fmt);

	if (state == AST_STATE_RING) {
		ast_channel_rings_set(chan, 1);
	}

	ast_channel_adsicpe_set(chan, AST_ADSI_UNAVAILABLE);

	ast_channel_context_set(chan, session->endpoint->context);
	ast_channel_exten_set(chan, S_OR(exten, "s"));
	ast_channel_priority_set(chan, 1);

	ast_channel_callgroup_set(chan, session->endpoint->pickup.callgroup);
	ast_channel_pickupgroup_set(chan, session->endpoint->pickup.pickupgroup);

	ast_channel_named_callgroups_set(chan, session->endpoint->pickup.named_callgroups);
	ast_channel_named_pickupgroups_set(chan, session->endpoint->pickup.named_pickupgroups);

	if (!ast_strlen_zero(session->endpoint->language)) {
		ast_channel_language_set(chan, session->endpoint->language);
	}

	if (!ast_strlen_zero(session->endpoint->zone)) {
		struct ast_tone_zone *zone = ast_get_indication_zone(session->endpoint->zone);
		if (!zone) {
			ast_log(LOG_ERROR, "Unknown country code '%s' for tonezone. Check indications.conf for available country codes.\n", session->endpoint->zone);
		}
		ast_channel_zone_set(chan, zone);
	}

	ast_endpoint_add_channel(session->endpoint->persistent, chan);

	ast_channel_stage_snapshot_done(chan);

	return chan;
}

static int answer(void *data)
{
	pj_status_t status = PJ_SUCCESS;
	pjsip_tx_data *packet;
	struct ast_sip_session *session = data;

	pjsip_dlg_inc_lock(session->inv_session->dlg);
	if (session->inv_session->invite_tsx) {
		status = pjsip_inv_answer(session->inv_session, 200, NULL, NULL, &packet);
	}
	pjsip_dlg_dec_lock(session->inv_session->dlg);

	if (status == PJ_SUCCESS && packet) {
		ast_sip_session_send_response(session, packet);
	}

	ao2_ref(session, -1);

	return (status == PJ_SUCCESS) ? 0 : -1;
}

/*! \brief Function called by core when we should answer a PJSIP session */
static int chan_pjsip_answer(struct ast_channel *ast)
{
	struct ast_sip_channel_pvt *channel = ast_channel_tech_pvt(ast);

	if (ast_channel_state(ast) == AST_STATE_UP) {
		return 0;
	}

	ast_setstate(ast, AST_STATE_UP);

	ao2_ref(channel->session, +1);
	if (ast_sip_push_task(channel->session->serializer, answer, channel->session)) {
		ast_log(LOG_WARNING, "Unable to push answer task to the threadpool. Cannot answer call\n");
		ao2_cleanup(channel->session);
		return -1;
	}

	return 0;
}

/*! \brief Internal helper function called when CNG tone is detected */
static struct ast_frame *chan_pjsip_cng_tone_detected(struct ast_sip_session *session, struct ast_frame *f)
{
	const char *target_context;
	int exists;

	/* If we only needed this DSP for fax detection purposes we can just drop it now */
	if (session->endpoint->dtmf == AST_SIP_DTMF_INBAND) {
		ast_dsp_set_features(session->dsp, DSP_FEATURE_DIGIT_DETECT);
	} else {
		ast_dsp_free(session->dsp);
		session->dsp = NULL;
	}

	/* If already executing in the fax extension don't do anything */
	if (!strcmp(ast_channel_exten(session->channel), "fax")) {
		return f;
	}

	target_context = S_OR(ast_channel_macrocontext(session->channel), ast_channel_context(session->channel));

	/* We need to unlock the channel here because ast_exists_extension has the
	 * potential to start and stop an autoservice on the channel. Such action
	 * is prone to deadlock if the channel is locked.
	 */
	ast_channel_unlock(session->channel);
	exists = ast_exists_extension(session->channel, target_context, "fax", 1,
		S_COR(ast_channel_caller(session->channel)->id.number.valid,
			ast_channel_caller(session->channel)->id.number.str, NULL));
	ast_channel_lock(session->channel);

	if (exists) {
		ast_verb(2, "Redirecting '%s' to fax extension due to CNG detection\n",
			ast_channel_name(session->channel));
		pbx_builtin_setvar_helper(session->channel, "FAXEXTEN", ast_channel_exten(session->channel));
		if (ast_async_goto(session->channel, target_context, "fax", 1)) {
			ast_log(LOG_ERROR, "Failed to async goto '%s' into fax extension in '%s'\n",
				ast_channel_name(session->channel), target_context);
		}
		ast_frfree(f);
		f = &ast_null_frame;
	} else {
		ast_log(LOG_NOTICE, "FAX CNG detected on '%s' but no fax extension in '%s'\n",
			ast_channel_name(session->channel), target_context);
	}

	return f;
}

/*! \brief Function called by core to read any waiting frames */
static struct ast_frame *chan_pjsip_read(struct ast_channel *ast)
{
	struct ast_sip_channel_pvt *channel = ast_channel_tech_pvt(ast);
	struct chan_pjsip_pvt *pvt = channel->pvt;
	struct ast_frame *f;
	struct ast_sip_session_media *media = NULL;
	int rtcp = 0;
	int fdno = ast_channel_fdno(ast);

	switch (fdno) {
	case 0:
		media = pvt->media[SIP_MEDIA_AUDIO];
		break;
	case 1:
		media = pvt->media[SIP_MEDIA_AUDIO];
		rtcp = 1;
		break;
	case 2:
		media = pvt->media[SIP_MEDIA_VIDEO];
		break;
	case 3:
		media = pvt->media[SIP_MEDIA_VIDEO];
		rtcp = 1;
		break;
	}

	if (!media || !media->rtp) {
		return &ast_null_frame;
	}

	if (!(f = ast_rtp_instance_read(media->rtp, rtcp))) {
		return f;
	}

	if (f->frametype != AST_FRAME_VOICE) {
		return f;
	}

	if (!(ast_format_cap_iscompatible(ast_channel_nativeformats(ast), &f->subclass.format))) {
		ast_debug(1, "Oooh, format changed to %s\n", ast_getformatname(&f->subclass.format));
		ast_format_cap_set(ast_channel_nativeformats(ast), &f->subclass.format);
		ast_set_read_format(ast, ast_channel_readformat(ast));
		ast_set_write_format(ast, ast_channel_writeformat(ast));
	}

	if (channel->session->dsp) {
		f = ast_dsp_process(ast, channel->session->dsp, f);

		if (f && (f->frametype == AST_FRAME_DTMF)) {
			if (f->subclass.integer == 'f') {
				ast_debug(3, "Fax CNG detected on %s\n", ast_channel_name(ast));
				f = chan_pjsip_cng_tone_detected(channel->session, f);
			} else {
				ast_debug(3, "* Detected inband DTMF '%c' on '%s'\n", f->subclass.integer,
					ast_channel_name(ast));
			}
		}
	}

	return f;
}

/*! \brief Function called by core to write frames */
static int chan_pjsip_write(struct ast_channel *ast, struct ast_frame *frame)
{
	struct ast_sip_channel_pvt *channel = ast_channel_tech_pvt(ast);
	struct chan_pjsip_pvt *pvt = channel->pvt;
	struct ast_sip_session_media *media;
	int res = 0;

	switch (frame->frametype) {
	case AST_FRAME_VOICE:
		media = pvt->media[SIP_MEDIA_AUDIO];

		if (!media) {
			return 0;
		}
		if (!(ast_format_cap_iscompatible(ast_channel_nativeformats(ast), &frame->subclass.format))) {
			char buf[256];

			ast_log(LOG_WARNING,
				"Asked to transmit frame type %s, while native formats is %s (read/write = %s/%s)\n",
				ast_getformatname(&frame->subclass.format),
				ast_getformatname_multiple(buf, sizeof(buf), ast_channel_nativeformats(ast)),
				ast_getformatname(ast_channel_readformat(ast)),
				ast_getformatname(ast_channel_writeformat(ast)));
			return 0;
		}
		if (media->rtp) {
			res = ast_rtp_instance_write(media->rtp, frame);
		}
		break;
	case AST_FRAME_VIDEO:
		if ((media = pvt->media[SIP_MEDIA_VIDEO]) && media->rtp) {
			res = ast_rtp_instance_write(media->rtp, frame);
		}
		break;
	case AST_FRAME_MODEM:
		break;
	default:
		ast_log(LOG_WARNING, "Can't send %d type frames with PJSIP\n", frame->frametype);
		break;
	}

	return res;
}

struct fixup_data {
	struct ast_sip_session *session;
	struct ast_channel *chan;
};

static int fixup(void *data)
{
	struct fixup_data *fix_data = data;
	struct ast_sip_channel_pvt *channel = ast_channel_tech_pvt(fix_data->chan);
	struct chan_pjsip_pvt *pvt = channel->pvt;

	channel->session->channel = fix_data->chan;
	if (pvt->media[SIP_MEDIA_AUDIO] && pvt->media[SIP_MEDIA_AUDIO]->rtp) {
		ast_rtp_instance_set_channel_id(pvt->media[SIP_MEDIA_AUDIO]->rtp, ast_channel_uniqueid(fix_data->chan));
	}
	if (pvt->media[SIP_MEDIA_VIDEO] && pvt->media[SIP_MEDIA_VIDEO]->rtp) {
		ast_rtp_instance_set_channel_id(pvt->media[SIP_MEDIA_VIDEO]->rtp, ast_channel_uniqueid(fix_data->chan));
	}

	return 0;
}

/*! \brief Function called by core to change the underlying owner channel */
static int chan_pjsip_fixup(struct ast_channel *oldchan, struct ast_channel *newchan)
{
	struct ast_sip_channel_pvt *channel = ast_channel_tech_pvt(newchan);
	struct fixup_data fix_data;

	fix_data.session = channel->session;
	fix_data.chan = newchan;

	if (channel->session->channel != oldchan) {
		return -1;
	}

	if (ast_sip_push_task_synchronous(channel->session->serializer, fixup, &fix_data)) {
		ast_log(LOG_WARNING, "Unable to perform channel fixup\n");
		return -1;
	}

	return 0;
}

/*! \brief Function called to get the device state of an endpoint */
static int chan_pjsip_devicestate(const char *data)
{
	RAII_VAR(struct ast_sip_endpoint *, endpoint, ast_sorcery_retrieve_by_id(ast_sip_get_sorcery(), "endpoint", data), ao2_cleanup);
	enum ast_device_state state = AST_DEVICE_UNKNOWN;
	RAII_VAR(struct ast_endpoint_snapshot *, endpoint_snapshot, NULL, ao2_cleanup);
	RAII_VAR(struct stasis_cache *, cache, NULL, ao2_cleanup);
	struct ast_devstate_aggregate aggregate;
	int num, inuse = 0;

	if (!endpoint) {
		return AST_DEVICE_INVALID;
	}

	endpoint_snapshot = ast_endpoint_latest_snapshot(ast_endpoint_get_tech(endpoint->persistent),
		ast_endpoint_get_resource(endpoint->persistent));

	if (!endpoint_snapshot) {
		return AST_DEVICE_INVALID;
	}

	if (endpoint_snapshot->state == AST_ENDPOINT_OFFLINE) {
		state = AST_DEVICE_UNAVAILABLE;
	} else if (endpoint_snapshot->state == AST_ENDPOINT_ONLINE) {
		state = AST_DEVICE_NOT_INUSE;
	}

	if (!endpoint_snapshot->num_channels || !(cache = ast_channel_cache())) {
		return state;
	}

	ast_devstate_aggregate_init(&aggregate);

	ao2_ref(cache, +1);

	for (num = 0; num < endpoint_snapshot->num_channels; num++) {
		RAII_VAR(struct stasis_message *, msg, NULL, ao2_cleanup);
		struct ast_channel_snapshot *snapshot;

		msg = stasis_cache_get(cache, ast_channel_snapshot_type(),
			endpoint_snapshot->channel_ids[num]);

		if (!msg) {
			continue;
		}

		snapshot = stasis_message_data(msg);

		if (snapshot->state == AST_STATE_DOWN) {
			ast_devstate_aggregate_add(&aggregate, AST_DEVICE_NOT_INUSE);
		} else if (snapshot->state == AST_STATE_RINGING) {
			ast_devstate_aggregate_add(&aggregate, AST_DEVICE_RINGING);
		} else if ((snapshot->state == AST_STATE_UP) || (snapshot->state == AST_STATE_RING) ||
			(snapshot->state == AST_STATE_BUSY)) {
			ast_devstate_aggregate_add(&aggregate, AST_DEVICE_INUSE);
			inuse++;
		}
	}

	if (endpoint->devicestate_busy_at && (inuse == endpoint->devicestate_busy_at)) {
		state = AST_DEVICE_BUSY;
	} else if (ast_devstate_aggregate_result(&aggregate) != AST_DEVICE_INVALID) {
		state = ast_devstate_aggregate_result(&aggregate);
	}

	return state;
}

/*! \brief Function called to query options on a channel */
static int chan_pjsip_queryoption(struct ast_channel *ast, int option, void *data, int *datalen)
{
	struct ast_sip_channel_pvt *channel = ast_channel_tech_pvt(ast);
	struct ast_sip_session *session = channel->session;
	int res = -1;
	enum ast_sip_session_t38state state = T38_STATE_UNAVAILABLE;

	switch (option) {
	case AST_OPTION_T38_STATE:
		if (session->endpoint->media.t38.enabled) {
			switch (session->t38state) {
			case T38_LOCAL_REINVITE:
			case T38_PEER_REINVITE:
				state = T38_STATE_NEGOTIATING;
				break;
			case T38_ENABLED:
				state = T38_STATE_NEGOTIATED;
				break;
			case T38_REJECTED:
				state = T38_STATE_REJECTED;
				break;
			default:
				state = T38_STATE_UNKNOWN;
				break;
			}
		}

		*((enum ast_t38_state *) data) = state;
		res = 0;

		break;
	default:
		break;
	}

	return res;
}

struct indicate_data {
	struct ast_sip_session *session;
	int condition;
	int response_code;
	void *frame_data;
	size_t datalen;
};

static void indicate_data_destroy(void *obj)
{
	struct indicate_data *ind_data = obj;

	ast_free(ind_data->frame_data);
	ao2_ref(ind_data->session, -1);
}

static struct indicate_data *indicate_data_alloc(struct ast_sip_session *session,
		int condition, int response_code, const void *frame_data, size_t datalen)
{
	struct indicate_data *ind_data = ao2_alloc(sizeof(*ind_data), indicate_data_destroy);

	if (!ind_data) {
		return NULL;
	}

	ind_data->frame_data = ast_malloc(datalen);
	if (!ind_data->frame_data) {
		ao2_ref(ind_data, -1);
		return NULL;
	}

	memcpy(ind_data->frame_data, frame_data, datalen);
	ind_data->datalen = datalen;
	ind_data->condition = condition;
	ind_data->response_code = response_code;
	ao2_ref(session, +1);
	ind_data->session = session;

	return ind_data;
}

static int indicate(void *data)
{
	pjsip_tx_data *packet = NULL;
	struct indicate_data *ind_data = data;
	struct ast_sip_session *session = ind_data->session;
	int response_code = ind_data->response_code;

	if (pjsip_inv_answer(session->inv_session, response_code, NULL, NULL, &packet) == PJ_SUCCESS) {
		ast_sip_session_send_response(session, packet);
	}

	ao2_ref(ind_data, -1);

	return 0;
}

/*! \brief Send SIP INFO with video update request */
static int transmit_info_with_vidupdate(void *data)
{
	const char * xml =
		"<?xml version=\"1.0\" encoding=\"utf-8\" ?>\r\n"
		" <media_control>\r\n"
		"  <vc_primitive>\r\n"
		"   <to_encoder>\r\n"
		"    <picture_fast_update/>\r\n"
		"   </to_encoder>\r\n"
		"  </vc_primitive>\r\n"
		" </media_control>\r\n";

	const struct ast_sip_body body = {
		.type = "application",
		.subtype = "media_control+xml",
		.body_text = xml
	};

	RAII_VAR(struct ast_sip_session *, session, data, ao2_cleanup);
	struct pjsip_tx_data *tdata;

	if (ast_sip_create_request("INFO", session->inv_session->dlg, session->endpoint, NULL, &tdata)) {
		ast_log(LOG_ERROR, "Could not create text video update INFO request\n");
		return -1;
	}
	if (ast_sip_add_body(tdata, &body)) {
		ast_log(LOG_ERROR, "Could not add body to text video update INFO request\n");
		return -1;
	}
	ast_sip_session_send_request(session, tdata);

	return 0;
}

/*! \brief Update connected line information */
static int update_connected_line_information(void *data)
{
	RAII_VAR(struct ast_sip_session *, session, data, ao2_cleanup);
	struct ast_party_id connected_id;

	if ((ast_channel_state(session->channel) != AST_STATE_UP) && (session->inv_session->role == PJSIP_UAS_ROLE)) {
		int response_code = 0;

		if (ast_channel_state(session->channel) == AST_STATE_RING) {
			response_code = !session->endpoint->inband_progress ? 180 : 183;
		} else if (ast_channel_state(session->channel) == AST_STATE_RINGING) {
			response_code = 183;
		}

		if (response_code) {
			struct pjsip_tx_data *packet = NULL;

			if (pjsip_inv_answer(session->inv_session, response_code, NULL, NULL, &packet) == PJ_SUCCESS) {
				ast_sip_session_send_response(session, packet);
			}
		}
	} else {
		enum ast_sip_session_refresh_method method = session->endpoint->id.refresh_method;

		if (session->inv_session->invite_tsx && (session->inv_session->options & PJSIP_INV_SUPPORT_UPDATE)) {
			method = AST_SIP_SESSION_REFRESH_METHOD_UPDATE;
		}

		connected_id = ast_channel_connected_effective_id(session->channel);
		if ((session->endpoint->id.send_pai || session->endpoint->id.send_rpid) &&
		    (session->endpoint->id.trust_outbound ||
		     ((connected_id.name.presentation & AST_PRES_RESTRICTION) == AST_PRES_ALLOWED &&
		      (connected_id.number.presentation & AST_PRES_RESTRICTION) == AST_PRES_ALLOWED))) {
			ast_sip_session_refresh(session, NULL, NULL, NULL, method, 1);
		}
	}

	return 0;
}

/*! \brief Function called by core to ask the channel to indicate some sort of condition */
static int chan_pjsip_indicate(struct ast_channel *ast, int condition, const void *data, size_t datalen)
{
	struct ast_sip_channel_pvt *channel = ast_channel_tech_pvt(ast);
	struct chan_pjsip_pvt *pvt = channel->pvt;
	struct ast_sip_session_media *media;
	int response_code = 0;
	int res = 0;

	switch (condition) {
	case AST_CONTROL_RINGING:
		if (ast_channel_state(ast) == AST_STATE_RING) {
			if (channel->session->endpoint->inband_progress) {
				response_code = 183;
				res = -1;
			} else {
				response_code = 180;
			}
		} else {
			res = -1;
		}
		ast_devstate_changed(AST_DEVICE_UNKNOWN, AST_DEVSTATE_CACHABLE, "PJSIP/%s", ast_sorcery_object_get_id(channel->session->endpoint));
		break;
	case AST_CONTROL_BUSY:
		if (ast_channel_state(ast) != AST_STATE_UP) {
			response_code = 486;
		} else {
			res = -1;
		}
		break;
	case AST_CONTROL_CONGESTION:
		if (ast_channel_state(ast) != AST_STATE_UP) {
			response_code = 503;
		} else {
			res = -1;
		}
		break;
	case AST_CONTROL_INCOMPLETE:
		if (ast_channel_state(ast) != AST_STATE_UP) {
			response_code = 484;
		} else {
			res = -1;
		}
		break;
	case AST_CONTROL_PROCEEDING:
		if (ast_channel_state(ast) != AST_STATE_UP) {
			response_code = 100;
		} else {
			res = -1;
		}
		break;
	case AST_CONTROL_PROGRESS:
		if (ast_channel_state(ast) != AST_STATE_UP) {
			response_code = 183;
		} else {
			res = -1;
		}
		break;
	case AST_CONTROL_VIDUPDATE:
		media = pvt->media[SIP_MEDIA_VIDEO];
		if (media && media->rtp) {
			/* FIXME: Only use this for VP8. Additional work would have to be done to
			 * fully support other video codecs */
			struct ast_format_cap *fcap = ast_channel_nativeformats(ast);
			struct ast_format vp8;
			ast_format_set(&vp8, AST_FORMAT_VP8, 0);
			if (ast_format_cap_iscompatible(fcap, &vp8)) {
				/* FIXME Fake RTP write, this will be sent as an RTCP packet. Ideally the
				 * RTP engine would provide a way to externally write/schedule RTCP
				 * packets */
				struct ast_frame fr;
				fr.frametype = AST_FRAME_CONTROL;
				fr.subclass.integer = AST_CONTROL_VIDUPDATE;
				res = ast_rtp_instance_write(media->rtp, &fr);
			} else {
				ao2_ref(channel->session, +1);

				if (ast_sip_push_task(channel->session->serializer, transmit_info_with_vidupdate, channel->session)) {
					ao2_cleanup(channel->session);
				}
			}
		} else {
			res = -1;
		}
		break;
	case AST_CONTROL_CONNECTED_LINE:
		ao2_ref(channel->session, +1);
		if (ast_sip_push_task(channel->session->serializer, update_connected_line_information, channel->session)) {
			ao2_cleanup(channel->session);
		}
		break;
	case AST_CONTROL_UPDATE_RTP_PEER:
		break;
	case AST_CONTROL_PVT_CAUSE_CODE:
		res = -1;
		break;
	case AST_CONTROL_HOLD:
		ast_moh_start(ast, data, NULL);
		break;
	case AST_CONTROL_UNHOLD:
		ast_moh_stop(ast);
		break;
	case AST_CONTROL_SRCUPDATE:
		break;
	case AST_CONTROL_SRCCHANGE:
		break;
	case AST_CONTROL_REDIRECTING:
		if (ast_channel_state(ast) != AST_STATE_UP) {
			response_code = 181;
		} else {
			res = -1;
		}
		break;
	case AST_CONTROL_T38_PARAMETERS:
		res = 0;

		if (channel->session->t38state == T38_PEER_REINVITE) {
			const struct ast_control_t38_parameters *parameters = data;

			if (parameters->request_response == AST_T38_REQUEST_PARMS) {
				res = AST_T38_REQUEST_PARMS;
			}
		}

		break;
	case -1:
		res = -1;
		break;
	default:
		ast_log(LOG_WARNING, "Don't know how to indicate condition %d\n", condition);
		res = -1;
		break;
	}

	if (response_code) {
		struct indicate_data *ind_data = indicate_data_alloc(channel->session, condition, response_code, data, datalen);
		if (!ind_data || ast_sip_push_task(channel->session->serializer, indicate, ind_data)) {
			ast_log(LOG_NOTICE, "Cannot send response code %d to endpoint %s. Could not queue task properly\n",
					response_code, ast_sorcery_object_get_id(channel->session->endpoint));
			ao2_cleanup(ind_data);
			res = -1;
		}
	}

	return res;
}

struct transfer_data {
	struct ast_sip_session *session;
	char *target;
};

static void transfer_data_destroy(void *obj)
{
	struct transfer_data *trnf_data = obj;

	ast_free(trnf_data->target);
	ao2_cleanup(trnf_data->session);
}

static struct transfer_data *transfer_data_alloc(struct ast_sip_session *session, const char *target)
{
	struct transfer_data *trnf_data = ao2_alloc(sizeof(*trnf_data), transfer_data_destroy);

	if (!trnf_data) {
		return NULL;
	}

	if (!(trnf_data->target = ast_strdup(target))) {
		ao2_ref(trnf_data, -1);
		return NULL;
	}

	ao2_ref(session, +1);
	trnf_data->session = session;

	return trnf_data;
}

static void transfer_redirect(struct ast_sip_session *session, const char *target)
{
	pjsip_tx_data *packet;
	enum ast_control_transfer message = AST_TRANSFER_SUCCESS;
	pjsip_contact_hdr *contact;
	pj_str_t tmp;

	if (pjsip_inv_end_session(session->inv_session, 302, NULL, &packet) != PJ_SUCCESS) {
		message = AST_TRANSFER_FAILED;
		ast_queue_control_data(session->channel, AST_CONTROL_TRANSFER, &message, sizeof(message));

		return;
	}

	if (!(contact = pjsip_msg_find_hdr(packet->msg, PJSIP_H_CONTACT, NULL))) {
		contact = pjsip_contact_hdr_create(packet->pool);
	}

	pj_strdup2_with_null(packet->pool, &tmp, target);
	if (!(contact->uri = pjsip_parse_uri(packet->pool, tmp.ptr, tmp.slen, PJSIP_PARSE_URI_AS_NAMEADDR))) {
		message = AST_TRANSFER_FAILED;
		ast_queue_control_data(session->channel, AST_CONTROL_TRANSFER, &message, sizeof(message));
		pjsip_tx_data_dec_ref(packet);

		return;
	}
	pjsip_msg_add_hdr(packet->msg, (pjsip_hdr *) contact);

	ast_sip_session_send_response(session, packet);
	ast_queue_control_data(session->channel, AST_CONTROL_TRANSFER, &message, sizeof(message));
}

static void transfer_refer(struct ast_sip_session *session, const char *target)
{
	pjsip_evsub *sub;
	enum ast_control_transfer message = AST_TRANSFER_SUCCESS;
	pj_str_t tmp;
	pjsip_tx_data *packet;

	if (pjsip_xfer_create_uac(session->inv_session->dlg, NULL, &sub) != PJ_SUCCESS) {
		message = AST_TRANSFER_FAILED;
		ast_queue_control_data(session->channel, AST_CONTROL_TRANSFER, &message, sizeof(message));

		return;
	}

	if (pjsip_xfer_initiate(sub, pj_cstr(&tmp, target), &packet) != PJ_SUCCESS) {
		message = AST_TRANSFER_FAILED;
		ast_queue_control_data(session->channel, AST_CONTROL_TRANSFER, &message, sizeof(message));
		pjsip_evsub_terminate(sub, PJ_FALSE);

		return;
	}

	pjsip_xfer_send_request(sub, packet);
	ast_queue_control_data(session->channel, AST_CONTROL_TRANSFER, &message, sizeof(message));
}

static int transfer(void *data)
{
	struct transfer_data *trnf_data = data;

	if (ast_channel_state(trnf_data->session->channel) == AST_STATE_RING) {
		transfer_redirect(trnf_data->session, trnf_data->target);
	} else {
		transfer_refer(trnf_data->session, trnf_data->target);
	}

	ao2_ref(trnf_data, -1);
	return 0;
}

/*! \brief Function called by core for Asterisk initiated transfer */
static int chan_pjsip_transfer(struct ast_channel *chan, const char *target)
{
	struct ast_sip_channel_pvt *channel = ast_channel_tech_pvt(chan);
	struct transfer_data *trnf_data = transfer_data_alloc(channel->session, target);

	if (!trnf_data) {
		return -1;
	}

	if (ast_sip_push_task(channel->session->serializer, transfer, trnf_data)) {
		ast_log(LOG_WARNING, "Error requesting transfer\n");
		ao2_cleanup(trnf_data);
		return -1;
	}

	return 0;
}

/*! \brief Function called by core to start a DTMF digit */
static int chan_pjsip_digit_begin(struct ast_channel *chan, char digit)
{
	struct ast_sip_channel_pvt *channel = ast_channel_tech_pvt(chan);
	struct chan_pjsip_pvt *pvt = channel->pvt;
	struct ast_sip_session_media *media = pvt->media[SIP_MEDIA_AUDIO];
	int res = 0;

	switch (channel->session->endpoint->dtmf) {
	case AST_SIP_DTMF_RFC_4733:
		if (!media || !media->rtp) {
			return -1;
		}

		ast_rtp_instance_dtmf_begin(media->rtp, digit);
	case AST_SIP_DTMF_NONE:
		break;
	case AST_SIP_DTMF_INBAND:
		res = -1;
		break;
	default:
		break;
	}

	return res;
}

struct info_dtmf_data {
	struct ast_sip_session *session;
	char digit;
	unsigned int duration;
};

static void info_dtmf_data_destroy(void *obj)
{
	struct info_dtmf_data *dtmf_data = obj;
	ao2_ref(dtmf_data->session, -1);
}

static struct info_dtmf_data *info_dtmf_data_alloc(struct ast_sip_session *session, char digit, unsigned int duration)
{
	struct info_dtmf_data *dtmf_data = ao2_alloc(sizeof(*dtmf_data), info_dtmf_data_destroy);
	if (!dtmf_data) {
		return NULL;
	}
	ao2_ref(session, +1);
	dtmf_data->session = session;
	dtmf_data->digit = digit;
	dtmf_data->duration = duration;
	return dtmf_data;
}

static int transmit_info_dtmf(void *data)
{
	RAII_VAR(struct info_dtmf_data *, dtmf_data, data, ao2_cleanup);

	struct ast_sip_session *session = dtmf_data->session;
	struct pjsip_tx_data *tdata;

	RAII_VAR(struct ast_str *, body_text, NULL, ast_free_ptr);

	struct ast_sip_body body = {
		.type = "application",
		.subtype = "dtmf-relay",
	};

	if (!(body_text = ast_str_create(32))) {
		ast_log(LOG_ERROR, "Could not allocate buffer for INFO DTMF.\n");
		return -1;
	}
	ast_str_set(&body_text, 0, "Signal=%c\r\nDuration=%u\r\n", dtmf_data->digit, dtmf_data->duration);

	body.body_text = ast_str_buffer(body_text);

	if (ast_sip_create_request("INFO", session->inv_session->dlg, session->endpoint, NULL, &tdata)) {
		ast_log(LOG_ERROR, "Could not create DTMF INFO request\n");
		return -1;
	}
	if (ast_sip_add_body(tdata, &body)) {
		ast_log(LOG_ERROR, "Could not add body to DTMF INFO request\n");
		pjsip_tx_data_dec_ref(tdata);
		return -1;
	}
	ast_sip_session_send_request(session, tdata);

	return 0;
}

/*! \brief Function called by core to stop a DTMF digit */
static int chan_pjsip_digit_end(struct ast_channel *ast, char digit, unsigned int duration)
{
	struct ast_sip_channel_pvt *channel = ast_channel_tech_pvt(ast);
	struct chan_pjsip_pvt *pvt = channel->pvt;
	struct ast_sip_session_media *media = pvt->media[SIP_MEDIA_AUDIO];
	int res = 0;

	switch (channel->session->endpoint->dtmf) {
	case AST_SIP_DTMF_INFO:
	{
		struct info_dtmf_data *dtmf_data = info_dtmf_data_alloc(channel->session, digit, duration);

		if (!dtmf_data) {
			return -1;
		}

		if (ast_sip_push_task(channel->session->serializer, transmit_info_dtmf, dtmf_data)) {
			ast_log(LOG_WARNING, "Error sending DTMF via INFO.\n");
			ao2_cleanup(dtmf_data);
			return -1;
		}
		break;
	}
	case AST_SIP_DTMF_RFC_4733:
		if (!media || !media->rtp) {
			return -1;
		}

		ast_rtp_instance_dtmf_end_with_duration(media->rtp, digit, duration);
	case AST_SIP_DTMF_NONE:
		break;
	case AST_SIP_DTMF_INBAND:
		res = -1;
		break;
	}

	return res;
}

static int call(void *data)
{
	struct ast_sip_session *session = data;
	pjsip_tx_data *tdata;

	int res = ast_sip_session_create_invite(session, &tdata);

	if (res) {
		ast_queue_hangup(session->channel);
	} else {
		ast_sip_session_send_request(session, tdata);
	}
	ao2_ref(session, -1);
	return res;
}

/*! \brief Function called by core to actually start calling a remote party */
static int chan_pjsip_call(struct ast_channel *ast, const char *dest, int timeout)
{
	struct ast_sip_channel_pvt *channel = ast_channel_tech_pvt(ast);

	ao2_ref(channel->session, +1);
	if (ast_sip_push_task(channel->session->serializer, call, channel->session)) {
		ast_log(LOG_WARNING, "Error attempting to place outbound call to call '%s'\n", dest);
		ao2_cleanup(channel->session);
		return -1;
	}

	return 0;
}

/*! \brief Internal function which translates from Asterisk cause codes to SIP response codes */
static int hangup_cause2sip(int cause)
{
	switch (cause) {
	case AST_CAUSE_UNALLOCATED:             /* 1 */
	case AST_CAUSE_NO_ROUTE_DESTINATION:    /* 3 IAX2: Can't find extension in context */
	case AST_CAUSE_NO_ROUTE_TRANSIT_NET:    /* 2 */
		return 404;
	case AST_CAUSE_CONGESTION:              /* 34 */
	case AST_CAUSE_SWITCH_CONGESTION:       /* 42 */
		return 503;
	case AST_CAUSE_NO_USER_RESPONSE:        /* 18 */
		return 408;
	case AST_CAUSE_NO_ANSWER:               /* 19 */
	case AST_CAUSE_UNREGISTERED:        /* 20 */
		return 480;
	case AST_CAUSE_CALL_REJECTED:           /* 21 */
		return 403;
	case AST_CAUSE_NUMBER_CHANGED:          /* 22 */
		return 410;
	case AST_CAUSE_NORMAL_UNSPECIFIED:      /* 31 */
		return 480;
	case AST_CAUSE_INVALID_NUMBER_FORMAT:
		return 484;
	case AST_CAUSE_USER_BUSY:
		return 486;
	case AST_CAUSE_FAILURE:
		return 500;
	case AST_CAUSE_FACILITY_REJECTED:       /* 29 */
		return 501;
	case AST_CAUSE_CHAN_NOT_IMPLEMENTED:
		return 503;
	case AST_CAUSE_DESTINATION_OUT_OF_ORDER:
		return 502;
	case AST_CAUSE_BEARERCAPABILITY_NOTAVAIL:       /* Can't find codec to connect to host */
		return 488;
	case AST_CAUSE_INTERWORKING:    /* Unspecified Interworking issues */
		return 500;
	case AST_CAUSE_NOTDEFINED:
	default:
		ast_debug(1, "AST hangup cause %d (no match found in PJSIP)\n", cause);
		return 0;
	}

	/* Never reached */
	return 0;
}

struct hangup_data {
	int cause;
	struct ast_channel *chan;
};

static void hangup_data_destroy(void *obj)
{
	struct hangup_data *h_data = obj;

	h_data->chan = ast_channel_unref(h_data->chan);
}

static struct hangup_data *hangup_data_alloc(int cause, struct ast_channel *chan)
{
	struct hangup_data *h_data = ao2_alloc(sizeof(*h_data), hangup_data_destroy);

	if (!h_data) {
		return NULL;
	}

	h_data->cause = cause;
	h_data->chan = ast_channel_ref(chan);

	return h_data;
}

/*! \brief Clear a channel from a session along with its PVT */
static void clear_session_and_channel(struct ast_sip_session *session, struct ast_channel *ast, struct chan_pjsip_pvt *pvt)
{
	session->channel = NULL;
	if (pvt->media[SIP_MEDIA_AUDIO] && pvt->media[SIP_MEDIA_AUDIO]->rtp) {
		ast_rtp_instance_set_channel_id(pvt->media[SIP_MEDIA_AUDIO]->rtp, "");
	}
	if (pvt->media[SIP_MEDIA_VIDEO] && pvt->media[SIP_MEDIA_VIDEO]->rtp) {
		ast_rtp_instance_set_channel_id(pvt->media[SIP_MEDIA_VIDEO]->rtp, "");
	}
	ast_channel_tech_pvt_set(ast, NULL);
}

static int hangup(void *data)
{
	pj_status_t status;
	pjsip_tx_data *packet = NULL;
	struct hangup_data *h_data = data;
	struct ast_channel *ast = h_data->chan;
	struct ast_sip_channel_pvt *channel = ast_channel_tech_pvt(ast);
	struct chan_pjsip_pvt *pvt = channel->pvt;
	struct ast_sip_session *session = channel->session;
	int cause = h_data->cause;

	if (!session->defer_terminate &&
		((status = pjsip_inv_end_session(session->inv_session, cause ? cause : 603, NULL, &packet)) == PJ_SUCCESS) && packet) {
		if (packet->msg->type == PJSIP_RESPONSE_MSG) {
			ast_sip_session_send_response(session, packet);
		} else {
			ast_sip_session_send_request(session, packet);
		}
	}

	clear_session_and_channel(session, ast, pvt);
	ao2_cleanup(channel);
	ao2_cleanup(h_data);

	return 0;
}

/*! \brief Function called by core to hang up a PJSIP session */
static int chan_pjsip_hangup(struct ast_channel *ast)
{
	struct ast_sip_channel_pvt *channel = ast_channel_tech_pvt(ast);
	struct chan_pjsip_pvt *pvt = channel->pvt;
	int cause = hangup_cause2sip(ast_channel_hangupcause(channel->session->channel));
	struct hangup_data *h_data = hangup_data_alloc(cause, ast);

	if (!h_data) {
		goto failure;
	}

	if (ast_sip_push_task(channel->session->serializer, hangup, h_data)) {
		ast_log(LOG_WARNING, "Unable to push hangup task to the threadpool. Expect bad things\n");
		goto failure;
	}

	return 0;

failure:
	/* Go ahead and do our cleanup of the session and channel even if we're not going
	 * to be able to send our SIP request/response
	 */
	clear_session_and_channel(channel->session, ast, pvt);
	ao2_cleanup(channel);
	ao2_cleanup(h_data);

	return -1;
}

struct request_data {
	struct ast_sip_session *session;
	struct ast_format_cap *caps;
	const char *dest;
	int cause;
};

static int request(void *obj)
{
	struct request_data *req_data = obj;
	struct ast_sip_session *session = NULL;
	char *tmp = ast_strdupa(req_data->dest), *endpoint_name = NULL, *request_user = NULL;
	RAII_VAR(struct ast_sip_endpoint *, endpoint, NULL, ao2_cleanup);

	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(endpoint);
		AST_APP_ARG(aor);
	);

	if (ast_strlen_zero(tmp)) {
		ast_log(LOG_ERROR, "Unable to create PJSIP channel with empty destination\n");
		req_data->cause = AST_CAUSE_CHANNEL_UNACCEPTABLE;
		return -1;
	}

	AST_NONSTANDARD_APP_ARGS(args, tmp, '/');

	/* If a request user has been specified extract it from the endpoint name portion */
	if ((endpoint_name = strchr(args.endpoint, '@'))) {
		request_user = args.endpoint;
		*endpoint_name++ = '\0';
	} else {
		endpoint_name = args.endpoint;
	}

	if (ast_strlen_zero(endpoint_name)) {
		ast_log(LOG_ERROR, "Unable to create PJSIP channel with empty endpoint name\n");
		req_data->cause = AST_CAUSE_CHANNEL_UNACCEPTABLE;
	} else if (!(endpoint = ast_sorcery_retrieve_by_id(ast_sip_get_sorcery(), "endpoint", endpoint_name))) {
		ast_log(LOG_ERROR, "Unable to create PJSIP channel - endpoint '%s' was not found\n", endpoint_name);
		req_data->cause = AST_CAUSE_NO_ROUTE_DESTINATION;
		return -1;
	}

	if (!(session = ast_sip_session_create_outgoing(endpoint, args.aor, request_user, req_data->caps))) {
		req_data->cause = AST_CAUSE_NO_ROUTE_DESTINATION;
		return -1;
	}

	req_data->session = session;

	return 0;
}

/*! \brief Function called by core to create a new outgoing PJSIP session */
static struct ast_channel *chan_pjsip_request(const char *type, struct ast_format_cap *cap, const struct ast_channel *requestor, const char *data, int *cause)
{
	struct request_data req_data;
	RAII_VAR(struct ast_sip_session *, session, NULL, ao2_cleanup);

	req_data.caps = cap;
	req_data.dest = data;

	if (ast_sip_push_task_synchronous(NULL, request, &req_data)) {
		*cause = req_data.cause;
		return NULL;
	}

	session = req_data.session;

	if (!(session->channel = chan_pjsip_new(session, AST_STATE_DOWN, NULL, NULL, requestor ? ast_channel_linkedid(requestor) : NULL, NULL))) {
		/* Session needs to be terminated prematurely */
		return NULL;
	}

	return session->channel;
}

struct sendtext_data {
	struct ast_sip_session *session;
	char text[0];
};

static void sendtext_data_destroy(void *obj)
{
	struct sendtext_data *data = obj;
	ao2_ref(data->session, -1);
}

static struct sendtext_data* sendtext_data_create(struct ast_sip_session *session, const char *text)
{
	int size = strlen(text) + 1;
	struct sendtext_data *data = ao2_alloc(sizeof(*data)+size, sendtext_data_destroy);

	if (!data) {
		return NULL;
	}

	data->session = session;
	ao2_ref(data->session, +1);
	ast_copy_string(data->text, text, size);
	return data;
}

static int sendtext(void *obj)
{
	RAII_VAR(struct sendtext_data *, data, obj, ao2_cleanup);
	pjsip_tx_data *tdata;

	const struct ast_sip_body body = {
		.type = "text",
		.subtype = "plain",
		.body_text = data->text
	};

	/* NOT ast_strlen_zero, because a zero-length message is specifically
	 * allowed by RFC 3428 (See section 10, Examples) */
	if (!data->text) {
		return 0;
	}

	ast_debug(3, "Sending in dialog SIP message\n");

	ast_sip_create_request("MESSAGE", data->session->inv_session->dlg, data->session->endpoint, NULL, &tdata);
	ast_sip_add_body(tdata, &body);
	ast_sip_send_request(tdata, data->session->inv_session->dlg, data->session->endpoint);

	return 0;
}

/*! \brief Function called by core to send text on PJSIP session */
static int chan_pjsip_sendtext(struct ast_channel *ast, const char *text)
{
	struct ast_sip_channel_pvt *channel = ast_channel_tech_pvt(ast);
	struct sendtext_data *data = sendtext_data_create(channel->session, text);

	if (!data || ast_sip_push_task(channel->session->serializer, sendtext, data)) {
		ao2_ref(data, -1);
		return -1;
	}
	return 0;
}

/*! \brief Convert SIP hangup causes to Asterisk hangup causes */
static int hangup_sip2cause(int cause)
{
	/* Possible values taken from causes.h */

	switch(cause) {
	case 401:       /* Unauthorized */
		return AST_CAUSE_CALL_REJECTED;
	case 403:       /* Not found */
		return AST_CAUSE_CALL_REJECTED;
	case 404:       /* Not found */
		return AST_CAUSE_UNALLOCATED;
	case 405:       /* Method not allowed */
		return AST_CAUSE_INTERWORKING;
	case 407:       /* Proxy authentication required */
		return AST_CAUSE_CALL_REJECTED;
	case 408:       /* No reaction */
		return AST_CAUSE_NO_USER_RESPONSE;
	case 409:       /* Conflict */
		return AST_CAUSE_NORMAL_TEMPORARY_FAILURE;
	case 410:       /* Gone */
		return AST_CAUSE_NUMBER_CHANGED;
	case 411:       /* Length required */
		return AST_CAUSE_INTERWORKING;
	case 413:       /* Request entity too large */
		return AST_CAUSE_INTERWORKING;
	case 414:       /* Request URI too large */
		return AST_CAUSE_INTERWORKING;
	case 415:       /* Unsupported media type */
		return AST_CAUSE_INTERWORKING;
	case 420:       /* Bad extension */
		return AST_CAUSE_NO_ROUTE_DESTINATION;
	case 480:       /* No answer */
		return AST_CAUSE_NO_ANSWER;
	case 481:       /* No answer */
		return AST_CAUSE_INTERWORKING;
	case 482:       /* Loop detected */
		return AST_CAUSE_INTERWORKING;
	case 483:       /* Too many hops */
		return AST_CAUSE_NO_ANSWER;
	case 484:       /* Address incomplete */
		return AST_CAUSE_INVALID_NUMBER_FORMAT;
	case 485:       /* Ambiguous */
		return AST_CAUSE_UNALLOCATED;
	case 486:       /* Busy everywhere */
		return AST_CAUSE_BUSY;
	case 487:       /* Request terminated */
		return AST_CAUSE_INTERWORKING;
	case 488:       /* No codecs approved */
		return AST_CAUSE_BEARERCAPABILITY_NOTAVAIL;
	case 491:       /* Request pending */
		return AST_CAUSE_INTERWORKING;
	case 493:       /* Undecipherable */
		return AST_CAUSE_INTERWORKING;
	case 500:       /* Server internal failure */
		return AST_CAUSE_FAILURE;
	case 501:       /* Call rejected */
		return AST_CAUSE_FACILITY_REJECTED;
	case 502:
		return AST_CAUSE_DESTINATION_OUT_OF_ORDER;
	case 503:       /* Service unavailable */
		return AST_CAUSE_CONGESTION;
	case 504:       /* Gateway timeout */
		return AST_CAUSE_RECOVERY_ON_TIMER_EXPIRE;
	case 505:       /* SIP version not supported */
		return AST_CAUSE_INTERWORKING;
	case 600:       /* Busy everywhere */
		return AST_CAUSE_USER_BUSY;
	case 603:       /* Decline */
		return AST_CAUSE_CALL_REJECTED;
	case 604:       /* Does not exist anywhere */
		return AST_CAUSE_UNALLOCATED;
	case 606:       /* Not acceptable */
		return AST_CAUSE_BEARERCAPABILITY_NOTAVAIL;
	default:
		if (cause < 500 && cause >= 400) {
			/* 4xx class error that is unknown - someting wrong with our request */
			return AST_CAUSE_INTERWORKING;
		} else if (cause < 600 && cause >= 500) {
			/* 5xx class error - problem in the remote end */
			return AST_CAUSE_CONGESTION;
		} else if (cause < 700 && cause >= 600) {
			/* 6xx - global errors in the 4xx class */
			return AST_CAUSE_INTERWORKING;
		}
		return AST_CAUSE_NORMAL;
	}
	/* Never reached */
	return 0;
}

static void chan_pjsip_session_begin(struct ast_sip_session *session)
{
	RAII_VAR(struct ast_datastore *, datastore, NULL, ao2_cleanup);

	if (session->endpoint->media.direct_media.glare_mitigation ==
			AST_SIP_DIRECT_MEDIA_GLARE_MITIGATION_NONE) {
		return;
	}

	datastore = ast_sip_session_alloc_datastore(&direct_media_mitigation_info,
			"direct_media_glare_mitigation");

	if (!datastore) {
		return;
	}

	ast_sip_session_add_datastore(session, datastore);
}

/*! \brief Function called when the session ends */
static void chan_pjsip_session_end(struct ast_sip_session *session)
{
	if (!session->channel) {
		return;
	}

	if (!ast_channel_hangupcause(session->channel) && session->inv_session) {
		int cause = hangup_sip2cause(session->inv_session->cause);

		ast_queue_hangup_with_cause(session->channel, cause);
	} else {
		ast_queue_hangup(session->channel);
	}
}

/*! \brief Function called when a request is received on the session */
static int chan_pjsip_incoming_request(struct ast_sip_session *session, struct pjsip_rx_data *rdata)
{
	RAII_VAR(struct ast_datastore *, datastore, NULL, ao2_cleanup);
	struct transport_info_data *transport_data;
	pjsip_tx_data *packet = NULL;

	if (session->channel) {
		return 0;
	}

	datastore = ast_sip_session_alloc_datastore(&transport_info, "transport_info");
	if (!datastore) {
		return -1;
	}

	transport_data = ast_calloc(1, sizeof(*transport_data));
	if (!transport_data) {
		return -1;
	}
	pj_sockaddr_cp(&transport_data->local_addr, &rdata->tp_info.transport->local_addr);
	pj_sockaddr_cp(&transport_data->remote_addr, &rdata->pkt_info.src_addr);
	datastore->data = transport_data;
	ast_sip_session_add_datastore(session, datastore);

	if (!(session->channel = chan_pjsip_new(session, AST_STATE_RING, session->exten, NULL, NULL, NULL))) {
		if (pjsip_inv_end_session(session->inv_session, 503, NULL, &packet) == PJ_SUCCESS) {
			ast_sip_session_send_response(session, packet);
		}

		ast_log(LOG_ERROR, "Failed to allocate new PJSIP channel on incoming SIP INVITE\n");
		return -1;
	}
	/* channel gets created on incoming request, but we wait to call start
           so other supplements have a chance to run */
	return 0;
}

static int pbx_start_incoming_request(struct ast_sip_session *session, pjsip_rx_data *rdata)
{
	int res;

	res = ast_pbx_start(session->channel);

	switch (res) {
	case AST_PBX_FAILED:
		ast_log(LOG_WARNING, "Failed to start PBX ;(\n");
		ast_channel_hangupcause_set(session->channel, AST_CAUSE_SWITCH_CONGESTION);
		ast_hangup(session->channel);
		break;
	case AST_PBX_CALL_LIMIT:
		ast_log(LOG_WARNING, "Failed to start PBX (call limit reached) \n");
		ast_channel_hangupcause_set(session->channel, AST_CAUSE_SWITCH_CONGESTION);
		ast_hangup(session->channel);
		break;
	case AST_PBX_SUCCESS:
	default:
		break;
	}

	ast_debug(3, "Started PBX on new PJSIP channel %s\n", ast_channel_name(session->channel));

	return (res == AST_PBX_SUCCESS) ? 0 : -1;
}

static struct ast_sip_session_supplement pbx_start_supplement = {
	.method = "INVITE",
	.priority = AST_SIP_SESSION_SUPPLEMENT_PRIORITY_LAST,
	.incoming_request = pbx_start_incoming_request,
};

/*! \brief Function called when a response is received on the session */
static void chan_pjsip_incoming_response(struct ast_sip_session *session, struct pjsip_rx_data *rdata)
{
	struct pjsip_status_line status = rdata->msg_info.msg->line.status;

	if (!session->channel) {
		return;
	}

	switch (status.code) {
	case 180:
		ast_queue_control(session->channel, AST_CONTROL_RINGING);
		if (ast_channel_state(session->channel) != AST_STATE_UP) {
			ast_setstate(session->channel, AST_STATE_RINGING);
		}
		break;
	case 183:
		ast_queue_control(session->channel, AST_CONTROL_PROGRESS);
		break;
	case 200:
		ast_queue_control(session->channel, AST_CONTROL_ANSWER);
		break;
	default:
		break;
	}
}

static int chan_pjsip_incoming_ack(struct ast_sip_session *session, struct pjsip_rx_data *rdata)
{
	if (rdata->msg_info.msg->line.req.method.id == PJSIP_ACK_METHOD) {
		if (session->endpoint->media.direct_media.enabled && session->channel) {
			ast_queue_control(session->channel, AST_CONTROL_SRCCHANGE);
		}
	}
	return 0;
}

static struct ast_custom_function chan_pjsip_dial_contacts_function = {
	.name = "PJSIP_DIAL_CONTACTS",
	.read = pjsip_acf_dial_contacts_read,
};

static struct ast_custom_function media_offer_function = {
	.name = "PJSIP_MEDIA_OFFER",
	.read = pjsip_acf_media_offer_read,
	.write = pjsip_acf_media_offer_write
};

/*!
 * \brief Load the module
 *
 * Module loading including tests for configuration or dependencies.
 * This function can return AST_MODULE_LOAD_FAILURE, AST_MODULE_LOAD_DECLINE,
 * or AST_MODULE_LOAD_SUCCESS. If a dependency or environment variable fails
 * tests return AST_MODULE_LOAD_FAILURE. If the module can not load the
 * configuration file or other non-critical problem return
 * AST_MODULE_LOAD_DECLINE. On success return AST_MODULE_LOAD_SUCCESS.
 */
static int load_module(void)
{
	if (!(chan_pjsip_tech.capabilities = ast_format_cap_alloc(0))) {
		return AST_MODULE_LOAD_DECLINE;
	}

	ast_format_cap_add_all_by_type(chan_pjsip_tech.capabilities, AST_FORMAT_TYPE_AUDIO);

	ast_rtp_glue_register(&chan_pjsip_rtp_glue);

	if (ast_channel_register(&chan_pjsip_tech)) {
		ast_log(LOG_ERROR, "Unable to register channel class %s\n", channel_type);
		goto end;
	}

	if (ast_custom_function_register(&chan_pjsip_dial_contacts_function)) {
		ast_log(LOG_ERROR, "Unable to register PJSIP_DIAL_CONTACTS dialplan function\n");
		goto end;
	}

	if (ast_custom_function_register(&media_offer_function)) {
		ast_log(LOG_WARNING, "Unable to register PJSIP_MEDIA_OFFER dialplan function\n");
		goto end;
	}

	if (ast_sip_session_register_supplement(&chan_pjsip_supplement)) {
		ast_log(LOG_ERROR, "Unable to register PJSIP supplement\n");
		goto end;
	}

	if (ast_sip_session_register_supplement(&pbx_start_supplement)) {
		ast_log(LOG_ERROR, "Unable to register PJSIP pbx start supplement\n");
		ast_sip_session_unregister_supplement(&chan_pjsip_supplement);
		goto end;
	}

	if (ast_sip_session_register_supplement(&chan_pjsip_ack_supplement)) {
		ast_log(LOG_ERROR, "Unable to register PJSIP ACK supplement\n");
		ast_sip_session_unregister_supplement(&pbx_start_supplement);
		ast_sip_session_unregister_supplement(&chan_pjsip_supplement);
		goto end;
	}

	return 0;

end:
	ast_custom_function_unregister(&media_offer_function);
	ast_custom_function_unregister(&chan_pjsip_dial_contacts_function);
	ast_channel_unregister(&chan_pjsip_tech);
	ast_rtp_glue_unregister(&chan_pjsip_rtp_glue);

	return AST_MODULE_LOAD_FAILURE;
}

/*! \brief Reload module */
static int reload(void)
{
	return -1;
}

/*! \brief Unload the PJSIP channel from Asterisk */
static int unload_module(void)
{
	ast_sip_session_unregister_supplement(&chan_pjsip_supplement);
	ast_sip_session_unregister_supplement(&pbx_start_supplement);
	ast_sip_session_unregister_supplement(&chan_pjsip_ack_supplement);

	ast_custom_function_unregister(&media_offer_function);
	ast_custom_function_unregister(&chan_pjsip_dial_contacts_function);

	ast_channel_unregister(&chan_pjsip_tech);
	ast_rtp_glue_unregister(&chan_pjsip_rtp_glue);

	return 0;
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_LOAD_ORDER, "PJSIP Channel Driver",
		.load = load_module,
		.unload = unload_module,
		.reload = reload,
		.load_pri = AST_MODPRI_CHANNEL_DRIVER,
	       );
