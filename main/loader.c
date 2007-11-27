/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 1999 - 2006, Digium, Inc.
 *
 * Mark Spencer <markster@digium.com>
 * Kevin P. Fleming <kpfleming@digium.com>
 * Luigi Rizzo <rizzo@icir.org>
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
 * \brief Module Loader
 * \author Mark Spencer <markster@digium.com>
 * \author Kevin P. Fleming <kpfleming@digium.com>
 * \author Luigi Rizzo <rizzo@icir.org>
 * - See ModMngMnt
 */

#include "asterisk.h"

ASTERISK_FILE_VERSION(__FILE__, "$Revision$")

#include "asterisk/_private.h"
#include "asterisk/paths.h"	/* use ast_config_AST_MODULE_DIR */
#include <dirent.h>

#include "asterisk/linkedlists.h"
#include "asterisk/module.h"
#include "asterisk/config.h"
#include "asterisk/channel.h"
#include "asterisk/term.h"
#include "asterisk/manager.h"
#include "asterisk/cdr.h"
#include "asterisk/enum.h"
#include "asterisk/rtp.h"
#include "asterisk/http.h"
#include "asterisk/lock.h"

#ifdef DLFCNCOMPAT
#include "asterisk/dlfcn-compat.h"
#else
#include <dlfcn.h>
#endif

#include "asterisk/md5.h"
#include "asterisk/utils.h"

#ifndef RTLD_NOW
#define RTLD_NOW 0
#endif

#ifndef RTLD_LOCAL
#define RTLD_LOCAL 0
#endif

struct ast_module_user {
	struct ast_channel *chan;
	AST_LIST_ENTRY(ast_module_user) entry;
};

AST_LIST_HEAD(module_user_list, ast_module_user);

static unsigned char expected_key[] =
{ 0x87, 0x76, 0x79, 0x35, 0x23, 0xea, 0x3a, 0xd3,
  0x25, 0x2a, 0xbb, 0x35, 0x87, 0xe4, 0x22, 0x24 };

static char buildopt_sum[33] = AST_BUILDOPT_SUM;

static unsigned int embedding = 1; /* we always start out by registering embedded modules,
				      since they are here before we dlopen() any
				   */
/* Forward declaration */
static int manager_moduleload(struct mansession *s, const struct message *m);
static char mandescr_moduleload[];
static int manager_modulecheck(struct mansession *s, const struct message *m);
static char mandescr_modulecheck[];

struct ast_module {
	const struct ast_module_info *info;
	void *lib;					/* the shared lib, or NULL if embedded */
	int usecount;					/* the number of 'users' currently in this module */
	struct module_user_list users;			/* the list of users in the module */
	struct {
		unsigned int running:1;
		unsigned int declined:1;
	} flags;
	AST_LIST_ENTRY(ast_module) entry;
	char resource[0];
};

static AST_LIST_HEAD_STATIC(module_list, ast_module);

/*
 * module_list is cleared by its constructor possibly after
 * we start accumulating embedded modules, so we need to
 * use another list (without the lock) to accumulate them.
 * Then we update the main list when embedding is done.
 */
static struct module_list embedded_module_list;

struct loadupdate {
	int (*updater)(void);
	AST_LIST_ENTRY(loadupdate) entry;
};

static AST_LIST_HEAD_STATIC(updaters, loadupdate);

AST_MUTEX_DEFINE_STATIC(reloadlock);

/* when dynamic modules are being loaded, ast_module_register() will
   need to know what filename the module was loaded from while it
   is being registered
*/
struct ast_module *resource_being_loaded;

/* XXX: should we check for duplicate resource names here? */

void ast_module_register(const struct ast_module_info *info)
{
	struct ast_module *mod;

	if (embedding) {
		if (!(mod = ast_calloc(1, sizeof(*mod) + strlen(info->name) + 1)))
			return;
		strcpy(mod->resource, info->name);
	} else {
		mod = resource_being_loaded;
	}

	mod->info = info;
	AST_LIST_HEAD_INIT(&mod->users);

	/* during startup, before the loader has been initialized,
	   there are no threads, so there is no need to take the lock
	   on this list to manipulate it. it is also possible that it
	   might be unsafe to use the list lock at that point... so
	   let's avoid it altogether
	*/
	if (embedding) {
		AST_LIST_INSERT_TAIL(&embedded_module_list, mod, entry);
	} else {
		AST_LIST_LOCK(&module_list);
		/* it is paramount that the new entry be placed at the tail of
		   the list, otherwise the code that uses dlopen() to load
		   dynamic modules won't be able to find out if the module it
		   just opened was registered or failed to load
		*/
		AST_LIST_INSERT_TAIL(&module_list, mod, entry);
		AST_LIST_UNLOCK(&module_list);
	}

	/* give the module a copy of its own handle, for later use in registrations and the like */
	*((struct ast_module **) &(info->self)) = mod;
}

void ast_module_unregister(const struct ast_module_info *info)
{
	struct ast_module *mod = NULL;

	/* it is assumed that the users list in the module structure
	   will already be empty, or we cannot have gotten to this
	   point
	*/
	AST_LIST_LOCK(&module_list);
	AST_LIST_TRAVERSE_SAFE_BEGIN(&module_list, mod, entry) {
		if (mod->info == info) {
			AST_LIST_REMOVE_CURRENT(entry);
			break;
		}
	}
	AST_LIST_TRAVERSE_SAFE_END;
	AST_LIST_UNLOCK(&module_list);

	if (mod) {
		AST_LIST_HEAD_DESTROY(&mod->users);
		ast_free(mod);
	}
}

struct ast_module_user *__ast_module_user_add(struct ast_module *mod,
					      struct ast_channel *chan)
{
	struct ast_module_user *u = ast_calloc(1, sizeof(*u));

	if (!u)
		return NULL;

	u->chan = chan;

	AST_LIST_LOCK(&mod->users);
	AST_LIST_INSERT_HEAD(&mod->users, u, entry);
	AST_LIST_UNLOCK(&mod->users);

	ast_atomic_fetchadd_int(&mod->usecount, +1);

	ast_update_use_count();

	return u;
}

void __ast_module_user_remove(struct ast_module *mod, struct ast_module_user *u)
{
	AST_LIST_LOCK(&mod->users);
	AST_LIST_REMOVE(&mod->users, u, entry);
	AST_LIST_UNLOCK(&mod->users);
	ast_atomic_fetchadd_int(&mod->usecount, -1);
	ast_free(u);

	ast_update_use_count();
}

void __ast_module_user_hangup_all(struct ast_module *mod)
{
	struct ast_module_user *u;

	AST_LIST_LOCK(&mod->users);
	while ((u = AST_LIST_REMOVE_HEAD(&mod->users, entry))) {
		ast_softhangup(u->chan, AST_SOFTHANGUP_APPUNLOAD);
		ast_atomic_fetchadd_int(&mod->usecount, -1);
		ast_free(u);
	}
	AST_LIST_UNLOCK(&mod->users);

        ast_update_use_count();
}

/*! \note
 * In addition to modules, the reload command handles some extra keywords
 * which are listed here together with the corresponding handlers.
 * This table is also used by the command completion code.
 */
static struct reload_classes {
	const char *name;
	int (*reload_fn)(void);
} reload_classes[] = {	/* list in alpha order, longest match first for cli completion */
	{ "cdr",	ast_cdr_engine_reload },
	{ "dnsmgr",	dnsmgr_reload },
	{ "extconfig",	read_config_maps },
	{ "enum",	ast_enum_reload },
	{ "manager",	reload_manager },
	{ "rtp",	ast_rtp_reload },
	{ "http",	ast_http_reload },
	{ NULL, 	NULL }
};

static int printdigest(const unsigned char *d)
{
	int x, pos;
	char buf[256]; /* large enough so we don't have to worry */

	for (pos = 0, x = 0; x < 16; x++)
		pos += sprintf(buf + pos, " %02x", *d++);

	ast_debug(1, "Unexpected signature:%s\n", buf);

	return 0;
}

static int key_matches(const unsigned char *key1, const unsigned char *key2)
{
	int x;

	for (x = 0; x < 16; x++) {
		if (key1[x] != key2[x])
			return 0;
	}

	return 1;
}

static int verify_key(const unsigned char *key)
{
	struct MD5Context c;
	unsigned char digest[16];

	MD5Init(&c);
	MD5Update(&c, key, strlen((char *)key));
	MD5Final(digest, &c);

	if (key_matches(expected_key, digest))
		return 0;

	printdigest(digest);

	return -1;
}

static int resource_name_match(const char *name1_in, const char *name2_in)
{
	char *name1 = (char *) name1_in;
	char *name2 = (char *) name2_in;

	/* trim off any .so extensions */
	if (!strcasecmp(name1 + strlen(name1) - 3, ".so")) {
		name1 = ast_strdupa(name1);
		name1[strlen(name1) - 3] = '\0';
	}
	if (!strcasecmp(name2 + strlen(name2) - 3, ".so")) {
		name2 = ast_strdupa(name2);
		name2[strlen(name2) - 3] = '\0';
	}

	return strcasecmp(name1, name2);
}

static struct ast_module *find_resource(const char *resource, int do_lock)
{
	struct ast_module *cur;

	if (do_lock)
		AST_LIST_LOCK(&module_list);

	AST_LIST_TRAVERSE(&module_list, cur, entry) {
		if (!resource_name_match(resource, cur->resource))
			break;
	}

	if (do_lock)
		AST_LIST_UNLOCK(&module_list);

	return cur;
}

#ifdef LOADABLE_MODULES
static void unload_dynamic_module(struct ast_module *mod)
{
	void *lib = mod->lib;

	/* WARNING: the structure pointed to by mod is going to
	   disappear when this operation succeeds, so we can't
	   dereference it */

	if (lib)
		while (!dlclose(lib));
}

static struct ast_module *load_dynamic_module(const char *resource_in, unsigned int global_symbols_only)
{
	char fn[256] = "";
	void *lib = NULL;
	struct ast_module *mod;
	char *resource = (char *) resource_in;
	unsigned int wants_global;

	if (strcasecmp(resource + strlen(resource) - 3, ".so")) {
		resource = alloca(strlen(resource_in) + 3);
	        strcpy(resource, resource_in);
		strcat(resource, ".so");
	}

	snprintf(fn, sizeof(fn), "%s/%s", ast_config_AST_MODULE_DIR, resource);

	/* make a first load of the module in 'quiet' mode... don't try to resolve
	   any symbols, and don't export any symbols. this will allow us to peek into
	   the module's info block (if available) to see what flags it has set */

	if (!(resource_being_loaded = ast_calloc(1, sizeof(*resource_being_loaded) + strlen(resource) + 1)))
		return NULL;

	strcpy(resource_being_loaded->resource, resource);

	if (!(lib = dlopen(fn, RTLD_LAZY | RTLD_LOCAL))) {
		ast_log(LOG_WARNING, "Error loading module '%s': %s\n", resource_in, dlerror());
		ast_free(resource_being_loaded);
		return NULL;
	}

	/* the dlopen() succeeded, let's find out if the module
	   registered itself */
	/* note that this will only work properly as long as
	   ast_module_register() (which is called by the module's
	   constructor) places the new module at the tail of the
	   module_list
	*/
	if (resource_being_loaded != (mod = AST_LIST_LAST(&module_list))) {
		ast_log(LOG_WARNING, "Module '%s' did not register itself during load\n", resource_in);
		/* no, it did not, so close it and return */
		while (!dlclose(lib));
		/* note that the module's destructor will call ast_module_unregister(),
		   which will free the structure we allocated in resource_being_loaded */
		return NULL;
	}

	wants_global = ast_test_flag(mod->info, AST_MODFLAG_GLOBAL_SYMBOLS);

	/* if we are being asked only to load modules that provide global symbols,
	   and this one does not, then close it and return */
	if (global_symbols_only && !wants_global) {
		while (!dlclose(lib));
		return NULL;
	}

	/* if the system supports RTLD_NOLOAD, we can just 'promote' the flags
	   on the already-opened library to what we want... if not, we have to
	   close it and start over
	*/
#if defined(HAVE_RTLD_NOLOAD) && !defined(__Darwin__)
	if (!dlopen(fn, RTLD_NOLOAD | (wants_global ? RTLD_LAZY | RTLD_GLOBAL : RTLD_NOW | RTLD_LOCAL))) {
		ast_log(LOG_WARNING, "Unable to promote flags on module '%s': %s\n", resource_in, dlerror());
		while (!dlclose(lib));
		ast_free(resource_being_loaded);
		return NULL;
	}
#else
	while (!dlclose(lib));
	resource_being_loaded = NULL;

	/* start the load process again */

	if (!(resource_being_loaded = ast_calloc(1, sizeof(*resource_being_loaded) + strlen(resource) + 1)))
		return NULL;

	strcpy(resource_being_loaded->resource, resource);

	if (!(lib = dlopen(fn, wants_global ? RTLD_LAZY | RTLD_GLOBAL : RTLD_NOW | RTLD_LOCAL))) {
		ast_log(LOG_WARNING, "Error loading module '%s': %s\n", resource_in, dlerror());
		ast_free(resource_being_loaded);
		return NULL;
	}

	/* since the module was successfully opened, and it registered itself
	   the previous time we did that, we're going to assume it worked this
	   time too :) */
#endif

	AST_LIST_LAST(&module_list)->lib = lib;
	resource_being_loaded = NULL;

	return AST_LIST_LAST(&module_list);
}
#endif

void ast_module_shutdown(void)
{
	struct ast_module *mod;
	AST_LIST_HEAD_NOLOCK_STATIC(local_module_list, ast_module);

	/* We have to call the unload() callbacks in reverse order that the modules
	 * exist in the module list so it is the reverse order of how they were
	 * loaded. */

	AST_LIST_LOCK(&module_list);
	while ((mod = AST_LIST_REMOVE_HEAD(&module_list, entry)))
		AST_LIST_INSERT_HEAD(&local_module_list, mod, entry);
	AST_LIST_UNLOCK(&module_list);

	while ((mod = AST_LIST_REMOVE_HEAD(&local_module_list, entry))) {
		if (mod->info->unload)
			mod->info->unload();
		/* Since this should only be called when shutting down "gracefully",
		 * all channels should be down before we get to this point, meaning
		 * there will be no module users left. */
		AST_LIST_HEAD_DESTROY(&mod->users);
		free(mod);
	}
}

int ast_unload_resource(const char *resource_name, enum ast_module_unload_mode force)
{
	struct ast_module *mod;
	int res = -1;
	int error = 0;

	AST_LIST_LOCK(&module_list);

	if (!(mod = find_resource(resource_name, 0))) {
		AST_LIST_UNLOCK(&module_list);
		return 0;
	}

	if (!(mod->flags.running || mod->flags.declined))
		error = 1;

	if (!error && (mod->usecount > 0)) {
		if (force)
			ast_log(LOG_WARNING, "Warning:  Forcing removal of module '%s' with use count %d\n",
				resource_name, mod->usecount);
		else {
			ast_log(LOG_WARNING, "Soft unload failed, '%s' has use count %d\n", resource_name,
				mod->usecount);
			error = 1;
		}
	}

	if (!error) {
		__ast_module_user_hangup_all(mod);
		res = mod->info->unload();

		if (res) {
			ast_log(LOG_WARNING, "Firm unload failed for %s\n", resource_name);
			if (force <= AST_FORCE_FIRM)
				error = 1;
			else
				ast_log(LOG_WARNING, "** Dangerous **: Unloading resource anyway, at user request\n");
		}
	}

	if (!error)
		mod->flags.running = mod->flags.declined = 0;

	AST_LIST_UNLOCK(&module_list);

	if (!error && !mod->lib)
		mod->info->restore_globals();

#ifdef LOADABLE_MODULES
	if (!error)
		unload_dynamic_module(mod);
#endif

	if (!error)
		ast_update_use_count();

	return res;
}

char *ast_module_helper(const char *line, const char *word, int pos, int state, int rpos, int needsreload)
{
	struct ast_module *cur;
	int i, which=0, l = strlen(word);
	char *ret = NULL;

	if (pos != rpos)
		return NULL;

	AST_LIST_LOCK(&module_list);
	AST_LIST_TRAVERSE(&module_list, cur, entry) {
		if (!strncasecmp(word, cur->resource, l) &&
		    (cur->info->reload || !needsreload) &&
		    ++which > state) {
			ret = ast_strdup(cur->resource);
			break;
		}
	}
	AST_LIST_UNLOCK(&module_list);

	if (!ret) {
		for (i=0; !ret && reload_classes[i].name; i++) {
			if (!strncasecmp(word, reload_classes[i].name, l) && ++which > state)
				ret = ast_strdup(reload_classes[i].name);
		}
	}

	return ret;
}

int ast_module_reload(const char *name)
{
	struct ast_module *cur;
	int res = 0; /* return value. 0 = not found, others, see below */
	int i;

	if (ast_mutex_trylock(&reloadlock)) {
		ast_verbose("The previous reload command didn't finish yet\n");
		return -1;	/* reload already in progress */
	}
	ast_lastreloadtime = ast_tvnow();

	/* Call "predefined" reload here first */
	for (i = 0; reload_classes[i].name; i++) {
		if (!name || !strcasecmp(name, reload_classes[i].name)) {
			reload_classes[i].reload_fn();	/* XXX should check error ? */
			res = 2;	/* found and reloaded */
		}
	}

	if (name && res) {
		ast_mutex_unlock(&reloadlock);
		return res;
	}

	AST_LIST_LOCK(&module_list);
	AST_LIST_TRAVERSE(&module_list, cur, entry) {
		const struct ast_module_info *info = cur->info;

		if (name && resource_name_match(name, cur->resource))
			continue;

		if (!(cur->flags.running || cur->flags.declined))
			continue;

		if (!info->reload) {	/* cannot be reloaded */
			if (res < 1)	/* store result if possible */
				res = 1;	/* 1 = no reload() method */
			continue;
		}

		res = 2;
		ast_verb(3, "Reloading module '%s' (%s)\n", cur->resource, info->description);
		info->reload();
	}
	AST_LIST_UNLOCK(&module_list);

	ast_mutex_unlock(&reloadlock);

	return res;
}

static unsigned int inspect_module(const struct ast_module *mod)
{
	if (!mod->info->description) {
		ast_log(LOG_WARNING, "Module '%s' does not provide a description.\n", mod->resource);
		return 1;
	}

	if (!mod->info->key) {
		ast_log(LOG_WARNING, "Module '%s' does not provide a license key.\n", mod->resource);
		return 1;
	}

	if (verify_key((unsigned char *) mod->info->key)) {
		ast_log(LOG_WARNING, "Module '%s' did not provide a valid license key.\n", mod->resource);
		return 1;
	}

	if (!ast_strlen_zero(mod->info->buildopt_sum) &&
	    strcmp(buildopt_sum, mod->info->buildopt_sum)) {
		ast_log(LOG_WARNING, "Module '%s' was not compiled with the same compile-time options as this version of Asterisk.\n", mod->resource);
		ast_log(LOG_WARNING, "Module '%s' will not be initialized as it may cause instability.\n", mod->resource);
		return 1;
	}

	return 0;
}

static enum ast_module_load_result load_resource(const char *resource_name, unsigned int global_symbols_only)
{
	struct ast_module *mod;
	enum ast_module_load_result res = AST_MODULE_LOAD_SUCCESS;
	char tmp[256];

	if ((mod = find_resource(resource_name, 0))) {
		if (mod->flags.running) {
			ast_log(LOG_WARNING, "Module '%s' already exists.\n", resource_name);
			return AST_MODULE_LOAD_DECLINE;
		}
		if (global_symbols_only && !ast_test_flag(mod->info, AST_MODFLAG_GLOBAL_SYMBOLS))
			return AST_MODULE_LOAD_SKIP;
	} else {
#ifdef LOADABLE_MODULES
		if (!(mod = load_dynamic_module(resource_name, global_symbols_only))) {
			/* don't generate a warning message during load_modules() */
			if (!global_symbols_only) {
				ast_log(LOG_WARNING, "Module '%s' could not be loaded.\n", resource_name);
				return AST_MODULE_LOAD_DECLINE;
			} else {
				return AST_MODULE_LOAD_SKIP;
			}
		}
#else
		ast_log(LOG_WARNING, "Module '%s' could not be loaded.\n", resource_name);
		return AST_MODULE_LOAD_DECLINE;
#endif
	}

	if (inspect_module(mod)) {
		ast_log(LOG_WARNING, "Module '%s' could not be loaded.\n", resource_name);
#ifdef LOADABLE_MODULES
		unload_dynamic_module(mod);
#endif
		return AST_MODULE_LOAD_DECLINE;
	}

	if (!mod->lib && mod->info->backup_globals()) {
		ast_log(LOG_WARNING, "Module '%s' was unable to backup its global data.\n", resource_name);
		return AST_MODULE_LOAD_DECLINE;
	}

	mod->flags.declined = 0;

	if (mod->info->load)
		res = mod->info->load();

	switch (res) {
	case AST_MODULE_LOAD_SUCCESS:
		if (!ast_fully_booted) {
			ast_verb(1, "%s => (%s)\n", resource_name, term_color(tmp, mod->info->description, COLOR_BROWN, COLOR_BLACK, sizeof(tmp)));
			if (ast_opt_console && !option_verbose)
				ast_verbose( ".");
		} else {
			ast_verb(1, "Loaded %s => (%s)\n", resource_name, mod->info->description);
		}

		mod->flags.running = 1;

		ast_update_use_count();
		break;
	case AST_MODULE_LOAD_DECLINE:
		mod->flags.declined = 1;
		break;
	case AST_MODULE_LOAD_FAILURE:
		break;
	case AST_MODULE_LOAD_SKIP:
		/* modules should never return this value */
		break;
	}

	return res;
}

int ast_load_resource(const char *resource_name)
{
       AST_LIST_LOCK(&module_list);
       load_resource(resource_name, 0);
       AST_LIST_UNLOCK(&module_list);

       return 0;
}

struct load_order_entry {
	char *resource;
	AST_LIST_ENTRY(load_order_entry) entry;
};

AST_LIST_HEAD_NOLOCK(load_order, load_order_entry);

static struct load_order_entry *add_to_load_order(const char *resource, struct load_order *load_order)
{
	struct load_order_entry *order;

	AST_LIST_TRAVERSE(load_order, order, entry) {
		if (!resource_name_match(order->resource, resource))
			return NULL;
	}

	if (!(order = ast_calloc(1, sizeof(*order))))
		return NULL;

	order->resource = ast_strdup(resource);
	AST_LIST_INSERT_TAIL(load_order, order, entry);

	return order;
}

int load_modules(unsigned int preload_only)
{
	struct ast_config *cfg;
	struct ast_module *mod;
	struct load_order_entry *order;
	struct ast_variable *v;
	unsigned int load_count;
	struct load_order load_order;
	int res = 0;
	struct ast_flags config_flags = { 0 };
	int modulecount = 0;
#ifdef LOADABLE_MODULES
	struct dirent *dirent;
	DIR *dir;
#endif

	/* all embedded modules have registered themselves by now */
	embedding = 0;

	ast_verb(1, "Asterisk Dynamic Loader Starting:\n");

	AST_LIST_HEAD_INIT_NOLOCK(&load_order);

	AST_LIST_LOCK(&module_list);

	if (embedded_module_list.first) {
		module_list.first = embedded_module_list.first;
		module_list.last = embedded_module_list.last;
		embedded_module_list.first = NULL;
	}

	if (!(cfg = ast_config_load(AST_MODULE_CONFIG, config_flags))) {
		ast_log(LOG_WARNING, "No '%s' found, no modules will be loaded.\n", AST_MODULE_CONFIG);
		goto done;
	}

	/* first, find all the modules we have been explicitly requested to load */
	for (v = ast_variable_browse(cfg, "modules"); v; v = v->next) {
		if (!strcasecmp(v->name, preload_only ? "preload" : "load"))
			add_to_load_order(v->value, &load_order);
	}

	/* check if 'autoload' is on */
	if (!preload_only && ast_true(ast_variable_retrieve(cfg, "modules", "autoload"))) {
		/* if so, first add all the embedded modules that are not already running to the load order */
		AST_LIST_TRAVERSE(&module_list, mod, entry) {
			/* if it's not embedded, skip it */
			if (mod->lib)
				continue;

			if (mod->flags.running)
				continue;

			order = add_to_load_order(mod->resource, &load_order);
		}

#ifdef LOADABLE_MODULES
		/* if we are allowed to load dynamic modules, scan the directory for
		   for all available modules and add them as well */
		if ((dir  = opendir(ast_config_AST_MODULE_DIR))) {
			while ((dirent = readdir(dir))) {
				int ld = strlen(dirent->d_name);

				/* Must end in .so to load it.  */

				if (ld < 4)
					continue;

				if (strcasecmp(dirent->d_name + ld - 3, ".so"))
					continue;

				/* if there is already a module by this name in the module_list,
				   skip this file */
				if (find_resource(dirent->d_name, 0))
					continue;

				add_to_load_order(dirent->d_name, &load_order);
			}

			closedir(dir);
		} else {
			if (!ast_opt_quiet)
				ast_log(LOG_WARNING, "Unable to open modules directory '%s'.\n",
					ast_config_AST_MODULE_DIR);
		}
#endif
	}

	/* now scan the config for any modules we are prohibited from loading and
	   remove them from the load order */
	for (v = ast_variable_browse(cfg, "modules"); v; v = v->next) {
		if (strcasecmp(v->name, "noload"))
			continue;

		AST_LIST_TRAVERSE_SAFE_BEGIN(&load_order, order, entry) {
			if (!resource_name_match(order->resource, v->value)) {
				AST_LIST_REMOVE_CURRENT(entry);
				ast_free(order->resource);
				ast_free(order);
			}
		}
		AST_LIST_TRAVERSE_SAFE_END;
	}

	/* we are done with the config now, all the information we need is in the
	   load_order list */
	ast_config_destroy(cfg);

	load_count = 0;
	AST_LIST_TRAVERSE(&load_order, order, entry)
		load_count++;

	if (load_count)
		ast_log(LOG_NOTICE, "%d modules will be loaded.\n", load_count);

	/* first, load only modules that provide global symbols */
	AST_LIST_TRAVERSE_SAFE_BEGIN(&load_order, order, entry) {
		switch (load_resource(order->resource, 1)) {
		case AST_MODULE_LOAD_SUCCESS:
			modulecount++;
		case AST_MODULE_LOAD_DECLINE:
			AST_LIST_REMOVE_CURRENT(entry);
			ast_free(order->resource);
			ast_free(order);
			break;
		case AST_MODULE_LOAD_FAILURE:
			res = -1;
			goto done;
		case AST_MODULE_LOAD_SKIP:
			/* try again later */
			break;
		}
	}
	AST_LIST_TRAVERSE_SAFE_END;

	/* now load everything else */
	AST_LIST_TRAVERSE_SAFE_BEGIN(&load_order, order, entry) {
		switch (load_resource(order->resource, 0)) {
		case AST_MODULE_LOAD_SUCCESS:
			modulecount++;
		case AST_MODULE_LOAD_DECLINE:
			AST_LIST_REMOVE_CURRENT(entry);
			ast_free(order->resource);
			ast_free(order);
			break;
		case AST_MODULE_LOAD_FAILURE:
			res = -1;
			goto done;
		case AST_MODULE_LOAD_SKIP:
			/* should not happen */
			break;
		}
	}
	AST_LIST_TRAVERSE_SAFE_END;

done:
	while ((order = AST_LIST_REMOVE_HEAD(&load_order, entry))) {
		ast_free(order->resource);
		ast_free(order);
	}

	AST_LIST_UNLOCK(&module_list);

	ast_manager_register2("ModuleLoad", EVENT_FLAG_SYSTEM, manager_moduleload, "Module management", mandescr_moduleload);
	ast_manager_register2("ModuleCheck", EVENT_FLAG_SYSTEM, manager_modulecheck, "Check if module is loaded", mandescr_modulecheck);

	/* Tell manager clients that are aggressive at logging in that we're done
	   loading modules. If there's a DNS problem in chan_sip, we might not
	   even reach this */
	manager_event(EVENT_FLAG_SYSTEM, "ModuleLoadReport", "ModuleLoadStatus: Done\r\nModuleSelection: %s\r\nModuleCount: %d\r\n", preload_only ? "Preload" : "All", modulecount);
	return res;
}

void ast_update_use_count(void)
{
	/* Notify any module monitors that the use count for a
	   resource has changed */
	struct loadupdate *m;

	AST_LIST_LOCK(&module_list);
	AST_LIST_TRAVERSE(&updaters, m, entry)
		m->updater();
	AST_LIST_UNLOCK(&module_list);
}

int ast_update_module_list(int (*modentry)(const char *module, const char *description, int usecnt, const char *like),
			   const char *like)
{
	struct ast_module *cur;
	int unlock = -1;
	int total_mod_loaded = 0;

	if (AST_LIST_TRYLOCK(&module_list))
		unlock = 0;
 
	AST_LIST_TRAVERSE(&module_list, cur, entry) {
		total_mod_loaded += modentry(cur->resource, cur->info->description, cur->usecount, like);
	}

	if (unlock)
		AST_LIST_UNLOCK(&module_list);

	return total_mod_loaded;
}

/*! \brief Check if module exists */
int ast_module_check(const char *name)
{
	struct ast_module *cur;

	if (ast_strlen_zero(name))
		return 0;       /* FALSE */

	cur = find_resource(name, 1);

	return (cur != NULL);
}


int ast_loader_register(int (*v)(void))
{
	struct loadupdate *tmp;

	if (!(tmp = ast_malloc(sizeof(*tmp))))
		return -1;

	tmp->updater = v;
	AST_LIST_LOCK(&module_list);
	AST_LIST_INSERT_HEAD(&updaters, tmp, entry);
	AST_LIST_UNLOCK(&module_list);

	return 0;
}

int ast_loader_unregister(int (*v)(void))
{
	struct loadupdate *cur;

	AST_LIST_LOCK(&module_list);
	AST_LIST_TRAVERSE_SAFE_BEGIN(&updaters, cur, entry) {
		if (cur->updater == v)	{
			AST_LIST_REMOVE_CURRENT(entry);
			break;
		}
	}
	AST_LIST_TRAVERSE_SAFE_END;
	AST_LIST_UNLOCK(&module_list);

	return cur ? 0 : -1;
}

struct ast_module *ast_module_ref(struct ast_module *mod)
{
	ast_atomic_fetchadd_int(&mod->usecount, +1);
	ast_update_use_count();

	return mod;
}

void ast_module_unref(struct ast_module *mod)
{
	ast_atomic_fetchadd_int(&mod->usecount, -1);
	ast_update_use_count();
}

static char mandescr_modulecheck[] = 
"Description: Checks if Asterisk module is loaded\n"
"Variables: \n"
"  ActionID: <id>          Action ID for this transaction. Will be returned.\n"
"  Module: <name>          Asterisk module name (not including extension)\n"
"\n"
"Will return Success/Failure\n"
"For success returns, the module revision number is included.\n";

/* Manager function to check if module is loaded */
static int manager_modulecheck(struct mansession *s, const struct message *m)
{
	int res;
	const char *module = astman_get_header(m, "Module");
	const char *id = astman_get_header(m,"ActionID");
	char idText[BUFSIZ];
	const char *version;
	char filename[BUFSIZ/2];
	char *cut;

	snprintf(filename, sizeof(filename), module);
	if ((cut = strchr(filename, '.'))) {
		*cut = '\0';
	} else {
		cut = filename + strlen(filename);
	}
	sprintf(cut, ".so");
	ast_log(LOG_DEBUG, "**** ModuleCheck .so file %s\n", filename);
	res = ast_module_check(filename);
	if (!res) {
		astman_send_error(s, m, "Module not loaded");
		return 0;
	}
	sprintf(cut, ".c");
	ast_log(LOG_DEBUG, "**** ModuleCheck .c file %s\n", filename);
	version = ast_file_version_find(filename);

	if (!ast_strlen_zero(id))
		snprintf(idText, sizeof(idText), "ActionID: %s\r\n", id);
	astman_append(s, "Response: Success\r\n%s", idText);
	astman_append(s, "Version: %s\r\n\r\n", version ? version : "");
	return 0;
}


static char mandescr_moduleload[] = 
"Description: Loads, unloads or reloads an Asterisk module in a running system.\n"
"Variables: \n"
"  ActionID: <id>          Action ID for this transaction. Will be returned.\n"
"  Module: <name>          Asterisk module name (including .so extension)\n"
"                          or subsystem identifier:\n"
"				cdr, enum, dnsmgr, extconfig, manager, rtp, http\n"
"  LoadType: load | unload | reload\n"
"                          The operation to be done on module\n"
" If no module is specified for a reload loadtype, all modules are reloaded";

static int manager_moduleload(struct mansession *s, const struct message *m)
{
	int res;
	const char *module = astman_get_header(m, "Module");
	const char *loadtype = astman_get_header(m, "LoadType");

	if (!loadtype || strlen(loadtype) == 0)
		astman_send_error(s, m, "Incomplete ModuleLoad action.");
	if ((!module || strlen(module) == 0) && strcasecmp(loadtype, "reload") != 0)
		astman_send_error(s, m, "Need module name");

	if (!strcasecmp(loadtype, "load")) {
		res = ast_load_resource(module);
		if (res)
			astman_send_error(s, m, "Could not load module.");
		else
			astman_send_ack(s, m, "Module loaded.");
	} else if (!strcasecmp(loadtype, "unload")) {
		res = ast_unload_resource(module, AST_FORCE_SOFT);
		if (res)
			astman_send_error(s, m, "Could not unload module.");
		else
			astman_send_ack(s, m, "Module unloaded.");
	} else if (!strcasecmp(loadtype, "reload")) {
		if (module != NULL) {
			res = ast_module_reload(module);
			if (res == 0)
				astman_send_error(s, m, "No such module.");
			else if (res == 1)
				astman_send_error(s, m, "Module does not support reload action.");
			else
				astman_send_ack(s, m, "Module reloaded.");
		} else {
			ast_module_reload(NULL);	/* Reload all modules */
			astman_send_ack(s, m, "All modules reloaded");
		}
	} else 
		astman_send_error(s, m, "Incomplete ModuleLoad action.");
	return 0;
}
