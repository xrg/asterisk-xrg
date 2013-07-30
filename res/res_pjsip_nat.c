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

/*** MODULEINFO
	<depend>pjproject</depend>
	<depend>res_pjsip</depend>
	<support_level>core</support_level>
 ***/

#include "asterisk.h"

#include <pjsip.h>
#include <pjsip_ua.h>

#include "asterisk/res_pjsip.h"
#include "asterisk/module.h"
#include "asterisk/acl.h"

static pj_bool_t nat_on_rx_request(pjsip_rx_data *rdata)
{
	RAII_VAR(struct ast_sip_endpoint *, endpoint, ast_pjsip_rdata_get_endpoint(rdata), ao2_cleanup);
	pjsip_contact_hdr *contact;

	if (!endpoint) {
		return PJ_FALSE;
	}

	if (endpoint->nat.rewrite_contact && (contact = pjsip_msg_find_hdr(rdata->msg_info.msg, PJSIP_H_CONTACT, NULL)) &&
		(PJSIP_URI_SCHEME_IS_SIP(contact->uri) || PJSIP_URI_SCHEME_IS_SIPS(contact->uri))) {
		pjsip_sip_uri *uri = pjsip_uri_get_uri(contact->uri);

		pj_cstr(&uri->host, rdata->pkt_info.src_name);
		uri->port = rdata->pkt_info.src_port;
	}

	if (endpoint->nat.force_rport) {
		rdata->msg_info.via->rport_param = 0;
	}

	return PJ_FALSE;
}

/*! \brief Structure which contains information about a transport */
struct request_transport_details {
	/*! \brief Type of transport */
	enum ast_transport type;
	/*! \brief Potential pointer to the transport itself, if UDP */
	pjsip_transport *transport;
	/*! \brief Potential pointer to the transport factory itself, if TCP/TLS */
	pjsip_tpfactory *factory;
	/*! \brief Local address for transport */
	pj_str_t local_address;
	/*! \brief Local port for transport */
	int local_port;
};

/*! \brief Callback function for finding the transport the request is going out on */
static int find_transport_in_use(void *obj, void *arg, int flags)
{
	struct ast_sip_transport *transport = obj;
	struct request_transport_details *details = arg;

	/* If an explicit transport or factory matches then this is what is in use, if we are unavailable
	 * to compare based on that we make sure that the type is the same and the source IP address/port are the same
	 */
	if ((details->transport && details->transport == transport->state->transport) ||
		(details->factory && details->factory == transport->state->factory) ||
		((details->type == transport->type) && (transport->state->factory) &&
			!pj_strcmp(&transport->state->factory->addr_name.host, &details->local_address) &&
			transport->state->factory->addr_name.port == details->local_port)) {
		return CMP_MATCH | CMP_STOP;
	}

	return 0;
}

/*! \brief Helper function which returns the SIP URI of a Contact header */
static pjsip_sip_uri *nat_get_contact_sip_uri(pjsip_tx_data *tdata)
{
	pjsip_contact_hdr *contact = pjsip_msg_find_hdr(tdata->msg, PJSIP_H_CONTACT, NULL);

	if (!contact || (!PJSIP_URI_SCHEME_IS_SIP(contact->uri) && !PJSIP_URI_SCHEME_IS_SIPS(contact->uri))) {
		return NULL;
	}

	return pjsip_uri_get_uri(contact->uri);
}

/*! \brief Structure which contains hook details */
struct nat_hook_details {
	/*! \brief Outgoing message itself */
	pjsip_tx_data *tdata;
	/*! \brief Chosen transport */
	struct ast_sip_transport *transport;
};

/*! \brief Callback function for invoking hooks */
static int nat_invoke_hook(void *obj, void *arg, int flags)
{
	struct ast_sip_nat_hook *hook = obj;
	struct nat_hook_details *details = arg;

	if (hook->outgoing_external_message) {
		hook->outgoing_external_message(details->tdata, details->transport);
	}

	return 0;
}

static pj_status_t nat_on_tx_message(pjsip_tx_data *tdata)
{
	RAII_VAR(struct ao2_container *, transports, NULL, ao2_cleanup);
	RAII_VAR(struct ast_sip_transport *, transport, NULL, ao2_cleanup);
	struct request_transport_details details = { 0, };
	pjsip_via_hdr *via = NULL;
	struct ast_sockaddr addr = { { 0, } };
	pjsip_sip_uri *uri = NULL;
	RAII_VAR(struct ao2_container *, hooks, NULL, ao2_cleanup);

	/* If a transport selector is in use we know the transport or factory, so explicitly find it */
	if (tdata->tp_sel.type == PJSIP_TPSELECTOR_TRANSPORT) {
		details.transport = tdata->tp_sel.u.transport;
	} else if (tdata->tp_sel.type == PJSIP_TPSELECTOR_LISTENER) {
		details.factory = tdata->tp_sel.u.listener;
	} else if (tdata->tp_info.transport->key.type == PJSIP_TRANSPORT_UDP || tdata->tp_info.transport->key.type == PJSIP_TRANSPORT_UDP6) {
		/* Connectionless uses the same transport for all requests */
		details.type = AST_TRANSPORT_UDP;
		details.transport = tdata->tp_info.transport;
	} else {
		if (tdata->tp_info.transport->key.type == PJSIP_TRANSPORT_TCP) {
			details.type = AST_TRANSPORT_TCP;
		} else if (tdata->tp_info.transport->key.type == PJSIP_TRANSPORT_TLS) {
			details.type = AST_TRANSPORT_TLS;
		} else {
			/* Unknown transport type, we can't map and thus can't apply NAT changes */
			return PJ_SUCCESS;
		}

		if ((uri = nat_get_contact_sip_uri(tdata))) {
			details.local_address = uri->host;
			details.local_port = uri->port;
		} else if ((tdata->msg->type == PJSIP_REQUEST_MSG) &&
			(via = pjsip_msg_find_hdr(tdata->msg, PJSIP_H_VIA, NULL))) {
			details.local_address = via->sent_by.host;
			details.local_port = via->sent_by.port;
		} else {
			return PJ_SUCCESS;
		}

		if (!details.local_port) {
			details.local_port = (details.type == AST_TRANSPORT_TLS) ? 5061 : 5060;
		}
	}

	if (!(transports = ast_sorcery_retrieve_by_fields(ast_sip_get_sorcery(), "transport", AST_RETRIEVE_FLAG_MULTIPLE | AST_RETRIEVE_FLAG_ALL, NULL)) ||
		!(transport = ao2_callback(transports, 0, find_transport_in_use, &details)) || !transport->localnet ||
		ast_sockaddr_isnull(&transport->external_address)) {
		return PJ_SUCCESS;
	}

	ast_sockaddr_parse(&addr, tdata->tp_info.dst_name, PARSE_PORT_FORBID);
	ast_sockaddr_set_port(&addr, tdata->tp_info.dst_port);

	/* See if where we are sending this request is local or not, and if not that we can get a Contact URI to modify */
	if (ast_apply_ha(transport->localnet, &addr) != AST_SENSE_ALLOW) {
		return PJ_SUCCESS;
	}

	/* Update the contact header with the external address */
	if (uri || (uri = nat_get_contact_sip_uri(tdata))) {
		pj_strdup2(tdata->pool, &uri->host, ast_sockaddr_stringify_host(&transport->external_address));
		if (transport->external_signaling_port) {
			uri->port = transport->external_signaling_port;
		}
	}

	/* Update the via header if relevant */
	if ((tdata->msg->type == PJSIP_REQUEST_MSG) && (via || (via = pjsip_msg_find_hdr(tdata->msg, PJSIP_H_VIA, NULL)))) {
		pj_strdup2(tdata->pool, &via->sent_by.host, ast_sockaddr_stringify_host(&transport->external_address));
		if (transport->external_signaling_port) {
			via->sent_by.port = transport->external_signaling_port;
		}
	}

	/* Invoke any additional hooks that may be registered */
	if ((hooks = ast_sorcery_retrieve_by_fields(ast_sip_get_sorcery(), "nat_hook", AST_RETRIEVE_FLAG_MULTIPLE | AST_RETRIEVE_FLAG_ALL, NULL))) {
		struct nat_hook_details hook_details = {
			.tdata = tdata,
			.transport = transport,
		};
		ao2_callback(hooks, 0, nat_invoke_hook, &hook_details);
	}

	return PJ_SUCCESS;
}

static pjsip_module nat_module = {
	.name = { "NAT", 3 },
	.id = -1,
	.priority = PJSIP_MOD_PRIORITY_TSX_LAYER - 2,
	.on_rx_request = nat_on_rx_request,
	.on_tx_request = nat_on_tx_message,
	.on_tx_response = nat_on_tx_message,
};

static int load_module(void)
{
	ast_sip_register_service(&nat_module);
	return AST_MODULE_LOAD_SUCCESS;
}

static int unload_module(void)
{
	ast_sip_unregister_service(&nat_module);
	return 0;
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_LOAD_ORDER, "PJSIP NAT Support",
		.load = load_module,
		.unload = unload_module,
		.load_pri = AST_MODPRI_APP_DEPEND,
	       );
