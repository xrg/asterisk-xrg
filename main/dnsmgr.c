/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2005-2006, Kevin P. Fleming
 *
 * Kevin P. Fleming <kpfleming@digium.com>
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
 * \brief Background DNS update manager
 *
 * \author Kevin P. Fleming <kpfleming@digium.com> 
 *
 * \bug There is a minor race condition.  In the event that an IP address
 * of a dnsmgr managed host changes, there is the potential for the consumer
 * of that address to access the in_addr data at the same time that the dnsmgr
 * thread is in the middle of updating it to the new address.
 */

#include "asterisk.h"

ASTERISK_FILE_VERSION(__FILE__, "$Revision$")

#include "asterisk/_private.h"
#include <regex.h>
#include <signal.h>

#include "asterisk/dnsmgr.h"
#include "asterisk/linkedlists.h"
#include "asterisk/utils.h"
#include "asterisk/config.h"
#include "asterisk/sched.h"
#include "asterisk/cli.h"
#include "asterisk/manager.h"

static struct sched_context *sched;
static int refresh_sched = -1;
static pthread_t refresh_thread = AST_PTHREADT_NULL;

struct ast_dnsmgr_entry {
	/*! where we will store the resulting address */
	struct in_addr *result;
	/*! the last result, used to check if address has changed */
	struct in_addr last;
	/*! Set to 1 if the entry changes */
	int changed:1;
	ast_mutex_t lock;
	AST_RWLIST_ENTRY(ast_dnsmgr_entry) list;
	/*! just 1 here, but we use calloc to allocate the correct size */
	char name[1];
};

static AST_RWLIST_HEAD_STATIC(entry_list, ast_dnsmgr_entry);

AST_MUTEX_DEFINE_STATIC(refresh_lock);

#define REFRESH_DEFAULT 300

static int enabled;
static int refresh_interval;

struct refresh_info {
	struct entry_list *entries;
	int verbose;
	unsigned int regex_present:1;
	regex_t filter;
};

static struct refresh_info master_refresh_info = {
	.entries = &entry_list,
	.verbose = 0,
};

struct ast_dnsmgr_entry *ast_dnsmgr_get(const char *name, struct in_addr *result)
{
	struct ast_dnsmgr_entry *entry;

	if (!result || ast_strlen_zero(name) || !(entry = ast_calloc(1, sizeof(*entry) + strlen(name))))
		return NULL;

	entry->result = result;
	ast_mutex_init(&entry->lock);
	strcpy(entry->name, name);
	memcpy(&entry->last, result, sizeof(entry->last));

	AST_RWLIST_WRLOCK(&entry_list);
	AST_RWLIST_INSERT_HEAD(&entry_list, entry, list);
	AST_RWLIST_UNLOCK(&entry_list);

	return entry;
}

void ast_dnsmgr_release(struct ast_dnsmgr_entry *entry)
{
	if (!entry)
		return;

	AST_RWLIST_WRLOCK(&entry_list);
	AST_RWLIST_REMOVE(&entry_list, entry, list);
	AST_RWLIST_UNLOCK(&entry_list);
	ast_verb(4, "removing dns manager for '%s'\n", entry->name);

	ast_mutex_destroy(&entry->lock);
	ast_free(entry);
}

int ast_dnsmgr_lookup(const char *name, struct in_addr *result, struct ast_dnsmgr_entry **dnsmgr)
{
	struct ast_hostent ahp;
	struct hostent *hp;

	if (ast_strlen_zero(name) || !result || !dnsmgr)
		return -1;

	if (*dnsmgr && !strcasecmp((*dnsmgr)->name, name))
		return 0;

	ast_verb(4, "doing dnsmgr_lookup for '%s'\n", name);

	/* if it's actually an IP address and not a name,
	   there's no need for a managed lookup */
	if (inet_aton(name, result))
		return 0;

	/* do a lookup now but add a manager so it will automagically get updated in the background */
	if ((hp = ast_gethostbyname(name, &ahp)))
		memcpy(result, hp->h_addr, sizeof(result));
	
	/* if dnsmgr is not enable don't bother adding an entry */
	if (!enabled)
		return 0;
	
	ast_verb(3, "adding dns manager for '%s'\n", name);
	*dnsmgr = ast_dnsmgr_get(name, result);
	return !*dnsmgr;
}

/*
 * Refresh a dnsmgr entry
 */
static int dnsmgr_refresh(struct ast_dnsmgr_entry *entry, int verbose)
{
	struct ast_hostent ahp;
	struct hostent *hp;
	char iabuf[INET_ADDRSTRLEN];
	char iabuf2[INET_ADDRSTRLEN];
	struct in_addr tmp;
	int changed = 0;
        
	ast_mutex_lock(&entry->lock);
	if (verbose)
		ast_verb(3, "refreshing '%s'\n", entry->name);

	if ((hp = ast_gethostbyname(entry->name, &ahp))) {
		/* check to see if it has changed, do callback if requested (where de callback is defined ????) */
		memcpy(&tmp, hp->h_addr, sizeof(tmp));
		if (tmp.s_addr != entry->last.s_addr) {
			ast_copy_string(iabuf, ast_inet_ntoa(entry->last), sizeof(iabuf));
			ast_copy_string(iabuf2, ast_inet_ntoa(tmp), sizeof(iabuf2));
			ast_log(LOG_NOTICE, "host '%s' changed from %s to %s\n", 
				entry->name, iabuf, iabuf2);
			memcpy(entry->result, hp->h_addr, sizeof(entry->result));
			memcpy(&entry->last, hp->h_addr, sizeof(entry->last));
			changed = entry->changed = 1;
		} 
		
	}
	ast_mutex_unlock(&entry->lock);
	return changed;
}

int ast_dnsmgr_refresh(struct ast_dnsmgr_entry *entry)
{
	return dnsmgr_refresh(entry, 0);
}

/*
 * Check if dnsmgr entry has changed from since last call to this function
 */
int ast_dnsmgr_changed(struct ast_dnsmgr_entry *entry) 
{
	int changed;

	ast_mutex_lock(&entry->lock);

	changed = entry->changed;
	entry->changed = 0;

	ast_mutex_unlock(&entry->lock);
	
	return changed;
}

static void *do_refresh(void *data)
{
	for (;;) {
		pthread_testcancel();
		usleep((ast_sched_wait(sched)*1000));
		pthread_testcancel();
		ast_sched_runq(sched);
	}
	return NULL;
}

static int refresh_list(const void *data)
{
	struct refresh_info *info = (struct refresh_info *)data;
	struct ast_dnsmgr_entry *entry;

	/* if a refresh or reload is already in progress, exit now */
	if (ast_mutex_trylock(&refresh_lock)) {
		if (info->verbose)
			ast_log(LOG_WARNING, "DNS Manager refresh already in progress.\n");
		return -1;
	}

	ast_verb(3, "Refreshing DNS lookups.\n");
	AST_RWLIST_RDLOCK(info->entries);
	AST_RWLIST_TRAVERSE(info->entries, entry, list) {
		if (info->regex_present && regexec(&info->filter, entry->name, 0, NULL, 0))
		    continue;

		dnsmgr_refresh(entry, info->verbose);
	}
	AST_RWLIST_UNLOCK(info->entries);

	ast_mutex_unlock(&refresh_lock);

	/* automatically reschedule based on the interval */
	return refresh_interval * 1000;
}

void dnsmgr_start_refresh(void)
{
	if (refresh_sched > -1) {
		AST_SCHED_DEL(sched, refresh_sched);
		refresh_sched = ast_sched_add_variable(sched, 100, refresh_list, &master_refresh_info, 1);
	}
}

static int do_reload(int loading);

static char *handle_cli_reload(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	switch (cmd) {
	case CLI_INIT:
		e->command = "dnsmgr reload";
		e->usage = 
			"Usage: dnsmgr reload\n"
			"       Reloads the DNS manager configuration.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;	
	}
	if (a->argc > 2)
		return CLI_SHOWUSAGE;

	do_reload(0);
	return CLI_SUCCESS;
}

static char *handle_cli_refresh(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	struct refresh_info info = {
		.entries = &entry_list,
		.verbose = 1,
	};
	switch (cmd) {
	case CLI_INIT:
		e->command = "dnsmgr refresh";
		e->usage = 
			"Usage: dnsmgr refresh [pattern]\n"
			"       Peforms an immediate refresh of the managed DNS entries.\n"
			"       Optional regular expression pattern is used to filter the entries to refresh.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;	
	}
	if (a->argc > 3)
		return CLI_SHOWUSAGE;

	if (a->argc == 3) {
		if ( regcomp(&info.filter, a->argv[2], REG_EXTENDED | REG_NOSUB) )
			return CLI_SHOWUSAGE;
		else
			info.regex_present = 1;
	}

	refresh_list(&info);

	if (info.regex_present)
		regfree(&info.filter);

	return CLI_SUCCESS;
}

static char *handle_cli_status(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	int count = 0;
	struct ast_dnsmgr_entry *entry;
	switch (cmd) {
	case CLI_INIT:
		e->command = "dnsmgr status";
		e->usage = 
			"Usage: dnsmgr status\n"
			"       Displays the DNS manager status.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;	
	}

	if (a->argc > 2)
		return CLI_SHOWUSAGE;

	ast_cli(a->fd, "DNS Manager: %s\n", enabled ? "enabled" : "disabled");
	ast_cli(a->fd, "Refresh Interval: %d seconds\n", refresh_interval);
	AST_RWLIST_RDLOCK(&entry_list);
	AST_RWLIST_TRAVERSE(&entry_list, entry, list)
		count++;
	AST_RWLIST_UNLOCK(&entry_list);
	ast_cli(a->fd, "Number of entries: %d\n", count);

	return CLI_SUCCESS;
}

static struct ast_cli_entry cli_reload = AST_CLI_DEFINE(handle_cli_reload, "Reloads the DNS manager configuration");
static struct ast_cli_entry cli_refresh = AST_CLI_DEFINE(handle_cli_refresh, "Performs an immediate refresh");
static struct ast_cli_entry cli_status = AST_CLI_DEFINE(handle_cli_status, "Display the DNS manager status");

int dnsmgr_init(void)
{
	if (!(sched = sched_context_create())) {
		ast_log(LOG_ERROR, "Unable to create schedule context.\n");
		return -1;
	}
	ast_cli_register(&cli_reload);
	ast_cli_register(&cli_status);
	ast_cli_register(&cli_refresh);
	return do_reload(1);
}

int dnsmgr_reload(void)
{
	return do_reload(0);
}

static int do_reload(int loading)
{
	struct ast_config *config;
	struct ast_flags config_flags = { loading ? 0 : CONFIG_FLAG_FILEUNCHANGED };
	const char *interval_value;
	const char *enabled_value;
	int interval;
	int was_enabled;
	int res = -1;

	if ((config = ast_config_load2("dnsmgr.conf", "dnsmgr", config_flags)) == CONFIG_STATUS_FILEUNCHANGED)
		return 0;

	/* ensure that no refresh cycles run while the reload is in progress */
	ast_mutex_lock(&refresh_lock);

	/* reset defaults in preparation for reading config file */
	refresh_interval = REFRESH_DEFAULT;
	was_enabled = enabled;
	enabled = 0;

	AST_SCHED_DEL(sched, refresh_sched);

	if (config) {
		if ((enabled_value = ast_variable_retrieve(config, "general", "enable"))) {
			enabled = ast_true(enabled_value);
		}
		if ((interval_value = ast_variable_retrieve(config, "general", "refreshinterval"))) {
			if (sscanf(interval_value, "%d", &interval) < 1)
				ast_log(LOG_WARNING, "Unable to convert '%s' to a numeric value.\n", interval_value);
			else if (interval < 0)
				ast_log(LOG_WARNING, "Invalid refresh interval '%d' specified, using default\n", interval);
			else
				refresh_interval = interval;
		}
		ast_config_destroy(config);
	}

	if (enabled && refresh_interval)
		ast_log(LOG_NOTICE, "Managed DNS entries will be refreshed every %d seconds.\n", refresh_interval);

	/* if this reload enabled the manager, create the background thread
	   if it does not exist */
	if (enabled) {
		if (!was_enabled && (refresh_thread == AST_PTHREADT_NULL)) {
			if (ast_pthread_create_background(&refresh_thread, NULL, do_refresh, NULL) < 0) {
				ast_log(LOG_ERROR, "Unable to start refresh thread.\n");
			}
		}
		/* make a background refresh happen right away */
		refresh_sched = ast_sched_add_variable(sched, 100, refresh_list, &master_refresh_info, 1);
		res = 0;
	}
	/* if this reload disabled the manager and there is a background thread,
	   kill it */
	else if (!enabled && was_enabled && (refresh_thread != AST_PTHREADT_NULL)) {
		/* wake up the thread so it will exit */
		pthread_cancel(refresh_thread);
		pthread_kill(refresh_thread, SIGURG);
		pthread_join(refresh_thread, NULL);
		refresh_thread = AST_PTHREADT_NULL;
		ast_cli_unregister(&cli_refresh);
		res = 0;
	}
	else
		res = 0;

	ast_mutex_unlock(&refresh_lock);
	manager_event(EVENT_FLAG_SYSTEM, "Reload", "Module: DNSmgr\r\nStatus: %s\r/nMessage: DNSmgr reload Requested\r\n", enabled ? "Enabled" : "Disabled");

	return res;
}
