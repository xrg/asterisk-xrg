/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2013, Digium, Inc.
 *
 * Mark Michelson <mmichelson@digium.com>
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
	<depend>res_sip</depend>
	<support_level>core</support_level>
 ***/

#include "asterisk.h"

#include <pjsip.h>

#include "asterisk/res_sip.h"
#include "asterisk/module.h"
#include "asterisk/logger.h"
#include "asterisk/sorcery.h"
#include "asterisk/acl.h"

/*** DOCUMENTATION
	<configInfo name="res_sip_acl" language="en_US">
		<synopsis>SIP ACL module</synopsis>
		<description><para>
			<emphasis>ACL</emphasis>
			</para>
			<para>The ACL module used by <literal>res_sip</literal>. This module is
			independent of <literal>endpoints</literal> and operates on all inbound
			SIP communication using res_sip.
			</para><para>
			It should be noted that this module can also reference ACLs from
			<filename>acl.conf</filename>.
			</para><para>
			There are two main ways of creating an access list: <literal>IP-Domain</literal>
			and <literal>Contact Header</literal>. It is possible to create a combined ACL using
			both IP and Contact.
		</para></description>
		<configFile name="res_sip.conf">
			<configObject name="acl">
				<synopsis>Access Control List</synopsis>
				<configOption name="acl">
					<synopsis>Name of IP ACL</synopsis>
					<description><para>
						This matches sections configured in <literal>acl.conf</literal>
					</para></description>
				</configOption>
				<configOption name="contactacl">
					<synopsis>Name of Contact ACL</synopsis>
					<description><para>
						This matches sections configured in <literal>acl.conf</literal>
					</para></description>
				</configOption>
				<configOption name="contactdeny">
					<synopsis>List of Contact Header addresses to Deny</synopsis>
				</configOption>
				<configOption name="contactpermit">
					<synopsis>List of Contact Header addresses to Permit</synopsis>
				</configOption>
				<configOption name="deny">
					<synopsis>List of IP-domains to deny access from</synopsis>
				</configOption>
				<configOption name="permit">
					<synopsis>List of IP-domains to allow access from</synopsis>
				</configOption>
				<configOption name="type">
					<synopsis>Must be of type 'security'.</synopsis>
				</configOption>
			</configObject>
		</configFile>
	</configInfo>
 ***/

static int apply_acl(pjsip_rx_data *rdata, struct ast_acl_list *acl)
{
	struct ast_sockaddr addr;

	if (ast_acl_list_is_empty(acl)) {
		return 0;
	}

	memset(&addr, 0, sizeof(addr));
	ast_sockaddr_parse(&addr, rdata->pkt_info.src_name, PARSE_PORT_FORBID);
	ast_sockaddr_set_port(&addr, rdata->pkt_info.src_port);

	if (ast_apply_acl(acl, &addr, "SIP ACL: ") != AST_SENSE_ALLOW) {
		ast_log(LOG_WARNING, "Incoming SIP message from %s did not pass ACL test\n", ast_sockaddr_stringify(&addr));
		return 1;
	}
	return 0;
}

static int extract_contact_addr(pjsip_contact_hdr *contact, struct ast_sockaddr **addrs)
{
	pjsip_sip_uri *sip_uri;
	char host[256];

	if (!contact) {
		return 0;
	}
	if (!PJSIP_URI_SCHEME_IS_SIP(contact->uri) && !PJSIP_URI_SCHEME_IS_SIPS(contact->uri)) {
		return 0;
	}
	sip_uri = pjsip_uri_get_uri(contact->uri);
	ast_copy_pj_str(host, &sip_uri->host, sizeof(host));
	return ast_sockaddr_resolve(addrs, host, PARSE_PORT_FORBID, AST_AF_UNSPEC);
}

static int apply_contact_acl(pjsip_rx_data *rdata, struct ast_acl_list *contact_acl)
{
	int num_contact_addrs;
	int forbidden = 0;
	struct ast_sockaddr *contact_addrs;
	int i;
	pjsip_contact_hdr *contact = (pjsip_contact_hdr *)&rdata->msg_info.msg->hdr;

	if (ast_acl_list_is_empty(contact_acl)) {
		return 0;
	}

	while ((contact = pjsip_msg_find_hdr(rdata->msg_info.msg, PJSIP_H_CONTACT, contact->next))) {
		num_contact_addrs = extract_contact_addr(contact, &contact_addrs);
		if (num_contact_addrs <= 0) {
			continue;
		}
		for (i = 0; i < num_contact_addrs; ++i) {
			if (ast_apply_acl(contact_acl, &contact_addrs[i], "SIP Contact ACL: ") != AST_SENSE_ALLOW) {
				ast_log(LOG_WARNING, "Incoming SIP message from %s did not pass ACL test\n", ast_sockaddr_stringify(&contact_addrs[i]));
				forbidden = 1;
				break;
			}
		}
		ast_free(contact_addrs);
		if (forbidden) {
			/* No use checking other contacts if we already have failed ACL check */
			break;
		}
	}

	return forbidden;
}

static int check_acls(void *obj, void *arg, int flags)
{
	struct ast_sip_security *security = obj;
	pjsip_rx_data *rdata = arg;

	if (apply_acl(rdata, security->acl) ||
	    apply_contact_acl(rdata, security->contact_acl)) {
		return CMP_MATCH | CMP_STOP;
	}
	return 0;
}

static pj_bool_t acl_on_rx_msg(pjsip_rx_data *rdata)
{
	RAII_VAR(struct ao2_container *, acls, ast_sorcery_retrieve_by_fields(
			 ast_sip_get_sorcery(), SIP_SORCERY_SECURITY_TYPE,
			 AST_RETRIEVE_FLAG_MULTIPLE | AST_RETRIEVE_FLAG_ALL, NULL), ao2_cleanup);
	RAII_VAR(struct ast_sip_security *, matched_acl, NULL, ao2_cleanup);

	if (!acls) {
		ast_log(LOG_ERROR, "Unable to retrieve ACL sorcery data\n");
		return PJ_FALSE;
	}

	if ((matched_acl = ao2_callback(acls, 0, check_acls, rdata))) {
		if (rdata->msg_info.msg->line.req.method.id != PJSIP_ACK_METHOD) {
			pjsip_endpt_respond_stateless(ast_sip_get_pjsip_endpoint(), rdata, 403, NULL, NULL, NULL);
		}
		return PJ_TRUE;
	}

	return PJ_FALSE;
}

static pjsip_module acl_module = {
	.name = { "ACL Module", 14 },
	/* This should run after a logger but before anything else */
	.priority = 1,
	.on_rx_request = acl_on_rx_msg,
};

static int load_module(void)
{
	ast_sip_register_service(&acl_module);
	return AST_MODULE_LOAD_SUCCESS;
}

static int unload_module(void)
{
	ast_sip_unregister_service(&acl_module);
	return 0;
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_LOAD_ORDER, "SIP ACL Resource",
		.load = load_module,
		.unload = unload_module,
		.load_pri = AST_MODPRI_APP_DEPEND,
	       );
