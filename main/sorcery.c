/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2012 - 2013, Digium, Inc.
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
 * \brief Sorcery Data Access Layer API
 *
 * \author Joshua Colp <jcolp@digium.com>
 */

/*** MODULEINFO
	<support_level>core</support_level>
 ***/

#include "asterisk.h"

ASTERISK_FILE_VERSION(__FILE__, "$Revision$")

#include "asterisk/logger.h"
#include "asterisk/sorcery.h"
#include "asterisk/astobj2.h"
#include "asterisk/strings.h"
#include "asterisk/config_options.h"
#include "asterisk/netsock2.h"
#include "asterisk/module.h"

/*! \brief Number of buckets for wizards (should be prime for performance reasons) */
#define WIZARD_BUCKETS 7

/*! \brief Number of buckets for types (should be prime for performance reasons) */
#define TYPE_BUCKETS 53

/*! \brief Maximum length of an object field name */
#define MAX_OBJECT_FIELD 128

/*! \brief Structure for registered object type */
struct ast_sorcery_object_type {
	/*! \brief Unique name of the object type */
	char name[MAX_OBJECT_TYPE];

	/*! \brief Optional transformation callback */
	sorcery_transform_handler transform;

	/*! \brief Optional object set apply callback */
	sorcery_apply_handler apply;

	/*! \brief Wizard instances */
	struct ao2_container *wizards;

	/*! \brief Object fields */
	struct ao2_container *fields;

	/*! \brief Configuration framework general information */
	struct aco_info *info;

	/*! \brief Configuration framework file information */
	struct aco_file *file;

	/*! \brief Type details */
	struct aco_type type;
};

/*! \brief Structure for registered object field */
struct ast_sorcery_object_field {
	/*! \brief Name of the field */
	char name[MAX_OBJECT_FIELD];

	/*! \brief Callback function for translation */
	sorcery_field_handler handler;

	/*! \brief Position of the field */
	intptr_t args[];
};

/*! \brief Structure for a wizard instance which operates on objects */
struct ast_sorcery_object_wizard {
	/*! \brief Wizard interface itself */
	struct ast_sorcery_wizard *wizard;

	/*! \brief Unique data for the wizard */
	void *data;

	/*! \brief Wizard is acting as an object cache */
	unsigned int caching:1;
};

/*! \brief Full structure for sorcery */
struct ast_sorcery {
	/*! \brief Container for known object types */
	struct ao2_container *types;
};

/*! \brief Structure for passing load/reload details */
struct sorcery_load_details {
	/*! \brief Sorcery structure in use */
	const struct ast_sorcery *sorcery;

	/*! \brief Type of object being loaded */
	const char *type;

	/*! \brief Whether this is a reload or not */
	unsigned int reload:1;
};

/*! \brief Registered sorcery wizards */
struct ao2_container *wizards;

static int int_handler_fn(const void *obj, const intptr_t *args, char **buf)
{
	int *field = (int *)(obj + args[0]);
	return (ast_asprintf(buf, "%d", *field) < 0) ? -1 : 0;
}

static int uint_handler_fn(const void *obj, const intptr_t *args, char **buf)
{
	unsigned int *field = (unsigned int *)(obj + args[0]);
	return (ast_asprintf(buf, "%u", *field) < 0) ? -1 : 0;
}

static int double_handler_fn(const void *obj, const intptr_t *args, char **buf)
{
	double *field = (double *)(obj + args[0]);
	return (ast_asprintf(buf, "%f", *field) < 0) ? -1 : 0;
}

static int stringfield_handler_fn(const void *obj, const intptr_t *args, char **buf)
{
	ast_string_field *field = (const char **)(obj + args[0]);
	return !(*buf = ast_strdup(*field)) ? -1 : 0;
}

static int bool_handler_fn(const void *obj, const intptr_t *args, char **buf)
{
	unsigned int *field = (unsigned int *)(obj + args[0]);
	return !(*buf = ast_strdup(*field ? "true" : "false")) ? -1 : 0;
}

static int sockaddr_handler_fn(const void *obj, const intptr_t *args, char **buf)
{
	struct ast_sockaddr *field = (struct ast_sockaddr *)(obj + args[0]);
	return !(*buf = ast_strdup(ast_sockaddr_stringify(field))) ? -1 : 0;
}

static int noop_handler_fn(const void *obj, const intptr_t *args, char **buf)
{
	return 0;
}

static int chararray_handler_fn(const void *obj, const intptr_t *args, char **buf)
{
	char *field = (char *)(obj + args[0]);
	return !(*buf = ast_strdup(field)) ? -1 : 0;
}

static sorcery_field_handler sorcery_field_default_handler(enum aco_option_type type)
{
	switch(type) {
	case OPT_BOOL_T: return bool_handler_fn;
	case OPT_CHAR_ARRAY_T: return chararray_handler_fn;
	case OPT_DOUBLE_T: return double_handler_fn;
	case OPT_INT_T: return int_handler_fn;
	case OPT_NOOP_T: return noop_handler_fn;
	case OPT_SOCKADDR_T: return sockaddr_handler_fn;
	case OPT_STRINGFIELD_T: return stringfield_handler_fn;
	case OPT_UINT_T: return uint_handler_fn;

	default:
	case OPT_CUSTOM_T: return NULL;
	}

	return NULL;
}

/*! \brief Hashing function for sorcery wizards */
static int sorcery_wizard_hash(const void *obj, const int flags)
{
	const struct ast_sorcery_wizard *wizard = obj;
	const char *name = obj;

	return ast_str_hash(flags & OBJ_KEY ? name : wizard->name);
}

/*! \brief Comparator function for sorcery wizards */
static int sorcery_wizard_cmp(void *obj, void *arg, int flags)
{
	struct ast_sorcery_wizard *wizard1 = obj, *wizard2 = arg;
	const char *name = arg;

	return !strcmp(wizard1->name, flags & OBJ_KEY ? name : wizard2->name) ? CMP_MATCH | CMP_STOP : 0;
}

int ast_sorcery_init(void)
{
	ast_assert(wizards == NULL);

	if (!(wizards = ao2_container_alloc(WIZARD_BUCKETS, sorcery_wizard_hash, sorcery_wizard_cmp))) {
		return -1;
	}

	return 0;
}

int __ast_sorcery_wizard_register(const struct ast_sorcery_wizard *interface, struct ast_module *module)
{
	struct ast_sorcery_wizard *wizard;
	int res = -1;

	ast_assert(!ast_strlen_zero(interface->name));

	ao2_lock(wizards);

	if ((wizard = ao2_find(wizards, interface->name, OBJ_KEY | OBJ_NOLOCK))) {
		ast_log(LOG_WARNING, "Attempted to register sorcery wizard '%s' twice\n",
			interface->name);
		goto done;
	}

	if (!(wizard = ao2_alloc(sizeof(*wizard), NULL))) {
		goto done;
	}

	*wizard = *interface;
	wizard->module = module;

	ao2_link_flags(wizards, wizard, OBJ_NOLOCK);
	res = 0;

	ast_verb(2, "Sorcery registered wizard '%s'\n", interface->name);

done:
	ao2_cleanup(wizard);
	ao2_unlock(wizards);

	return res;
}

int ast_sorcery_wizard_unregister(const struct ast_sorcery_wizard *interface)
{
	RAII_VAR(struct ast_sorcery_wizard *, wizard, ao2_find(wizards, interface->name, OBJ_KEY | OBJ_UNLINK), ao2_cleanup);

	if (wizard) {
		ast_verb(2, "Sorcery unregistered wizard '%s'\n", interface->name);
		return 0;
	} else {
		return -1;
	}
}

/*! \brief Destructor called when sorcery structure is destroyed */
static void sorcery_destructor(void *obj)
{
	struct ast_sorcery *sorcery = obj;

	ao2_cleanup(sorcery->types);
}

/*! \brief Hashing function for sorcery types */
static int sorcery_type_hash(const void *obj, const int flags)
{
	const struct ast_sorcery_object_type *type = obj;
	const char *name = obj;

	return ast_str_hash(flags & OBJ_KEY ? name : type->name);
}

/*! \brief Comparator function for sorcery types */
static int sorcery_type_cmp(void *obj, void *arg, int flags)
{
	struct ast_sorcery_object_type *type1 = obj, *type2 = arg;
	const char *name = arg;

	return !strcmp(type1->name, flags & OBJ_KEY ? name : type2->name) ? CMP_MATCH | CMP_STOP : 0;
}

struct ast_sorcery *ast_sorcery_open(void)
{
	struct ast_sorcery *sorcery;

	if (!(sorcery = ao2_alloc(sizeof(*sorcery), sorcery_destructor))) {
		return NULL;
	}

	if (!(sorcery->types = ao2_container_alloc_options(AO2_ALLOC_OPT_LOCK_NOLOCK, TYPE_BUCKETS, sorcery_type_hash, sorcery_type_cmp))) {
		ao2_ref(sorcery, -1);
		sorcery = NULL;
	}

	return sorcery;
}

/*! \brief Destructor function for object types */
static void sorcery_object_type_destructor(void *obj)
{
	struct ast_sorcery_object_type *object_type = obj;

	ao2_cleanup(object_type->wizards);
	ao2_cleanup(object_type->fields);

	if (object_type->info) {
		aco_info_destroy(object_type->info);
		ast_free(object_type->info);
	}

	ast_free(object_type->file);
}

/*! \brief Internal function which allocates an object type structure */
static struct ast_sorcery_object_type *sorcery_object_type_alloc(const char *type)
{
	struct ast_sorcery_object_type *object_type;

	if (!(object_type = ao2_alloc(sizeof(*object_type), sorcery_object_type_destructor))) {
		return NULL;
	}

	/* Order matters for object wizards */
	if (!(object_type->wizards = ao2_container_alloc_options(AO2_ALLOC_OPT_LOCK_NOLOCK, 1, NULL, NULL))) {
		ao2_ref(object_type, -1);
		return NULL;
	}

	if (!(object_type->fields = ao2_container_alloc_options(AO2_ALLOC_OPT_LOCK_NOLOCK, 1, NULL, NULL))) {
		ao2_ref(object_type, -1);
		return NULL;
	}

	if (!(object_type->info = ast_calloc(1, sizeof(*object_type->info) + 2 * sizeof(object_type->info->files[0])))) {
		ao2_ref(object_type, -1);
		return NULL;
	}

	if (!(object_type->file = ast_calloc(1, sizeof(*object_type->file) + 2 * sizeof(object_type->file->types[0])))) {
		ao2_ref(object_type, -1);
		return NULL;
	}

	object_type->info->files[0] = object_type->file;
	object_type->info->files[1] = NULL;

	ast_copy_string(object_type->name, type, sizeof(object_type->name));

	return object_type;
}

/*! \brief Object wizard destructor */
static void sorcery_object_wizard_destructor(void *obj)
{
	struct ast_sorcery_object_wizard *object_wizard = obj;

	if (object_wizard->data) {
		object_wizard->wizard->close(object_wizard->data);
	}

	if (object_wizard->wizard) {
		ast_module_unref(object_wizard->wizard->module);
	}
}

/*! \brief Internal function which creates an object type and adds a wizard mapping */
static int sorcery_apply_wizard_mapping(struct ast_sorcery *sorcery, const char *type, const char *name, const char *data, unsigned int caching)
{
	RAII_VAR(struct ast_sorcery_object_type *, object_type,  ao2_find(sorcery->types, type, OBJ_KEY), ao2_cleanup);
	RAII_VAR(struct ast_sorcery_wizard *, wizard, ao2_find(wizards, name, OBJ_KEY), ao2_cleanup);
	RAII_VAR(struct ast_sorcery_object_wizard *, object_wizard, ao2_alloc(sizeof(*object_wizard), sorcery_object_wizard_destructor), ao2_cleanup);
	int created = 0;

	if (!wizard || !object_wizard) {
		return -1;
	}

	if (!object_type) {
		if (!(object_type = sorcery_object_type_alloc(type))) {
			return -1;
		}
		created = 1;
	}

	if (wizard->open && !(object_wizard->data = wizard->open(data))) {
		return -1;
	}

	ast_module_ref(wizard->module);

	object_wizard->wizard = wizard;
	object_wizard->caching = caching;

	ao2_link(object_type->wizards, object_wizard);

	if (created) {
		ao2_link(sorcery->types, object_type);
	}

	return 0;
}

int ast_sorcery_apply_config(struct ast_sorcery *sorcery, const char *name)
{
	struct ast_flags flags = { 0 };
	struct ast_config *config = ast_config_load2("sorcery.conf", "sorcery", flags);
	struct ast_variable *mapping;
	int res = 0;

	if (!config || (config == CONFIG_STATUS_FILEMISSING) || (config == CONFIG_STATUS_FILEINVALID)) {
		return -1;
	}

	for (mapping = ast_variable_browse(config, name); mapping; mapping = mapping->next) {
		RAII_VAR(char *, mapping_name, ast_strdup(mapping->name), ast_free);
		RAII_VAR(char *, mapping_value, ast_strdup(mapping->value), ast_free);
		char *options = mapping_name, *name = strsep(&options, "/");
		char *data = mapping_value, *wizard = strsep(&data, ",");
		unsigned int caching = 0;

		/* If no wizard exists just skip, nothing we can do */
		if (ast_strlen_zero(wizard)) {
			continue;
		}

		/* If the wizard is configured as a cache treat it as such */
		if (!ast_strlen_zero(options) && strstr(options, "cache")) {
			caching = 1;
		}

		/* Any error immediately causes us to stop */
		if ((res = sorcery_apply_wizard_mapping(sorcery, name, wizard, data, caching))) {
			break;
		}
	}

	ast_config_destroy(config);

	return res;
}

int ast_sorcery_apply_default(struct ast_sorcery *sorcery, const char *type, const char *name, const char *data)
{
	RAII_VAR(struct ast_sorcery_object_type *, object_type,  ao2_find(sorcery->types, type, OBJ_KEY), ao2_cleanup);

	/* Defaults can not be added if any existing mapping exists */
	if (object_type) {
		return -1;
	}

	return sorcery_apply_wizard_mapping(sorcery, type, name, data, 0);
}

int ast_sorcery_object_register(struct ast_sorcery *sorcery, const char *type, aco_type_item_alloc alloc, sorcery_transform_handler transform, sorcery_apply_handler apply)
{
	RAII_VAR(struct ast_sorcery_object_type *, object_type, ao2_find(sorcery->types, type, OBJ_KEY), ao2_cleanup);

	if (!object_type) {
		return -1;
	}

	object_type->type.type = ACO_ITEM;
	object_type->type.category = "";
	object_type->type.item_alloc = alloc;

	object_type->transform = transform;
	object_type->apply = apply;
	object_type->file->types[0] = &object_type->type;
	object_type->file->types[1] = NULL;

	if (aco_info_init(object_type->info)) {
		return -1;
	}

	return 0;
}

int __ast_sorcery_object_field_register(struct ast_sorcery *sorcery, const char *type, const char *name, const char *default_val, enum aco_option_type opt_type,
					aco_option_handler config_handler, sorcery_field_handler sorcery_handler, unsigned int flags, size_t argc, ...)
{
	RAII_VAR(struct ast_sorcery_object_type *, object_type, ao2_find(sorcery->types, type, OBJ_KEY), ao2_cleanup);
	RAII_VAR(struct ast_sorcery_object_field *, object_field, NULL, ao2_cleanup);
	int pos;
	va_list args;

	if (!object_type || !object_type->type.item_alloc) {
		return -1;
	}

	if (!sorcery_handler) {
		sorcery_handler = sorcery_field_default_handler(opt_type);
	}

	if (!(object_field = ao2_alloc(sizeof(*object_field) + argc * sizeof(object_field->args[0]), NULL))) {
		return -1;
	}

	ast_copy_string(object_field->name, name, sizeof(object_field->name));
	object_field->handler = sorcery_handler;

	va_start(args, argc);
	for (pos = 0; pos < argc; pos++) {
		object_field->args[pos] = va_arg(args, size_t);
	}
	va_end(args);

	ao2_link(object_type->fields, object_field);

	/* TODO: Improve this hack */
	if (!argc) {
		__aco_option_register(object_type->info, name, ACO_EXACT, object_type->file->types, default_val, opt_type, config_handler, flags, argc);
	} else if (argc == 1) {
		__aco_option_register(object_type->info, name, ACO_EXACT, object_type->file->types, default_val, opt_type, config_handler, flags, argc,
				      object_field->args[0]);
	} else if (argc == 2) {
		__aco_option_register(object_type->info, name, ACO_EXACT, object_type->file->types, default_val, opt_type, config_handler, flags, argc,
				      object_field->args[0], object_field->args[1]);
	} else if (argc == 3) {
		__aco_option_register(object_type->info, name, ACO_EXACT, object_type->file->types, default_val, opt_type, config_handler, flags, argc,
				      object_field->args[0], object_field->args[1], object_field->args[2]);
	} else {
		ast_assert(0); /* The hack... she does us no good for this */
	}

	return 0;
}

static int sorcery_wizard_load(void *obj, void *arg, int flags)
{
	struct ast_sorcery_object_wizard *wizard = obj;
	struct sorcery_load_details *details = arg;
	void (*load)(void *data, const struct ast_sorcery *sorcery, const char *type);

	load = !details->reload ? wizard->wizard->load : wizard->wizard->reload;

	if (load) {
		load(wizard->data, details->sorcery, details->type);
	}

	return 0;
}

static int sorcery_object_load(void *obj, void *arg, int flags)
{
	struct ast_sorcery_object_type *type = obj;
	struct sorcery_load_details *details = arg;

	details->type = type->name;
	ao2_callback(type->wizards, OBJ_NODATA, sorcery_wizard_load, details);

	return 0;
}

void ast_sorcery_load(const struct ast_sorcery *sorcery)
{
	struct sorcery_load_details details = {
		.sorcery = sorcery,
		.reload = 0,
	};

	ao2_callback(sorcery->types, OBJ_NODATA, sorcery_object_load, &details);
}

void ast_sorcery_reload(const struct ast_sorcery *sorcery)
{
	struct sorcery_load_details details = {
		.sorcery = sorcery,
		.reload = 1,
	};

	ao2_callback(sorcery->types, OBJ_NODATA, sorcery_object_load, &details);
}

void ast_sorcery_ref(struct ast_sorcery *sorcery)
{
	ao2_ref(sorcery, +1);
}

struct ast_variable *ast_sorcery_objectset_create(const struct ast_sorcery *sorcery, const void *object)
{
	const struct ast_sorcery_object_details *details = object;
	RAII_VAR(struct ast_sorcery_object_type *, object_type, ao2_find(sorcery->types, details->type, OBJ_KEY), ao2_cleanup);
	struct ao2_iterator i;
	struct ast_sorcery_object_field *object_field;
	struct ast_variable *fields = NULL;
	int res = 0;

	if (!object_type) {
		return NULL;
	}

	i = ao2_iterator_init(object_type->fields, 0);

	for (; (object_field = ao2_iterator_next(&i)); ao2_ref(object_field, -1)) {
		RAII_VAR(char *, buf, NULL, ast_free);
		struct ast_variable *tmp;

		/* Any fields with no handler just get skipped */
		if (!object_field->handler) {
			continue;
		}

		if ((res = object_field->handler(object, object_field->args, &buf)) ||
		    !(tmp = ast_variable_new(object_field->name, S_OR(buf, ""), ""))) {
			res = -1;
			ao2_ref(object_field, -1);
			break;
		}

		tmp->next = fields;
		fields = tmp;
	}

	ao2_iterator_destroy(&i);

	/* If any error occurs we destroy all fields handled before so a partial objectset is not returned */
	if (res) {
		ast_variables_destroy(fields);
		fields = NULL;
	}

	return fields;
}

int ast_sorcery_objectset_apply(const struct ast_sorcery *sorcery, void *object, struct ast_variable *objectset)
{
	const struct ast_sorcery_object_details *details = object;
	RAII_VAR(struct ast_sorcery_object_type *, object_type, ao2_find(sorcery->types, details->type, OBJ_KEY), ao2_cleanup);
	RAII_VAR(struct ast_variable *, transformed, NULL, ast_variables_destroy);
	struct ast_variable *field;
	int res = 0;

	if (!object_type) {
		return -1;
	}

	if (object_type->transform && (transformed = object_type->transform(objectset))) {
		field = transformed;
	} else {
		field = objectset;
	}

	for (; field; field = field->next) {
		if ((res = aco_process_var(&object_type->type, details->id, field, object))) {
			break;
		}
	}

	if (!res && object_type->apply) {
		object_type->apply(sorcery, object);
	}

	return res;
}

static const struct ast_variable *sorcery_find_field(const struct ast_variable *fields, const char *name)
{
	const struct ast_variable *field;

	/* Search the linked list of fields to find the correct one */
	for (field = fields; field; field = field->next) {
		if (!strcmp(field->name, name)) {
			return field;
		}
	}

	return NULL;
}

int ast_sorcery_changeset_create(const struct ast_variable *original, const struct ast_variable *modified, struct ast_variable **changes)
{
	const struct ast_variable *field;
	int res = 0;

	*changes = NULL;

	/* Unless the ast_variable list changes when examined... it can't differ from itself */
	if (original == modified) {
		return 0;
	}

	for (field = modified; field; field = field->next) {
		const struct ast_variable *old = sorcery_find_field(original, field->name);

		if (!old || strcmp(old->value, field->value)) {
			struct ast_variable *tmp;

			if (!(tmp = ast_variable_new(field->name, field->value, ""))) {
				res = -1;
				break;
			}

			tmp->next = *changes;
			*changes = tmp;
		}
	}

	/* If an error occurred do not return a partial changeset */
	if (res) {
		ast_variables_destroy(*changes);
		*changes = NULL;
	}

	return res;
}

void *ast_sorcery_alloc(const struct ast_sorcery *sorcery, const char *type, const char *id)
{
	RAII_VAR(struct ast_sorcery_object_type *, object_type, ao2_find(sorcery->types, type, OBJ_KEY), ao2_cleanup);
	struct ast_sorcery_object_details *details;

	if (!object_type || !object_type->type.item_alloc ||
	    !(details = object_type->type.item_alloc(""))) {
		return NULL;
	}

	if (ast_strlen_zero(id)) {
		struct ast_uuid *uuid = ast_uuid_generate();

		if (!uuid) {
			ao2_ref(details, -1);
			return NULL;
		}

		ast_uuid_to_str(uuid, details->id, AST_UUID_STR_LEN);
		ast_free(uuid);
	} else {
		ast_copy_string(details->id, id, sizeof(details->id));
	}

	ast_copy_string(details->type, type, sizeof(details->type));

	if (aco_set_defaults(&object_type->type, id, details)) {
		ao2_ref(details, -1);
		return NULL;
	}

	return details;
}

void *ast_sorcery_copy(const struct ast_sorcery *sorcery, const void *object)
{
	const struct ast_sorcery_object_details *details = object;
	struct ast_sorcery_object_details *copy = ast_sorcery_alloc(sorcery, details->type, details->id);
	RAII_VAR(struct ast_variable *, objectset, NULL, ast_variables_destroy);

	if (!copy ||
	    !(objectset = ast_sorcery_objectset_create(sorcery, object)) ||
	    ast_sorcery_objectset_apply(sorcery, copy, objectset)) {
		ao2_cleanup(copy);
		return NULL;
	}

	return copy;
}

int ast_sorcery_diff(const struct ast_sorcery *sorcery, const void *original, const void *modified, struct ast_variable **changes)
{
	RAII_VAR(struct ast_variable *, objectset1, NULL, ast_variables_destroy);
	RAII_VAR(struct ast_variable *, objectset2, NULL, ast_variables_destroy);

	*changes = NULL;

	if (strcmp(ast_sorcery_object_get_type(original), ast_sorcery_object_get_type(modified))) {
		return -1;
	}

	objectset1 = ast_sorcery_objectset_create(sorcery, original);
	objectset2 = ast_sorcery_objectset_create(sorcery, modified);

	return ast_sorcery_changeset_create(objectset1, objectset2, changes);
}

/*! \brief Internal function used to create an object in caching wizards */
static int sorcery_cache_create(void *obj, void *arg, int flags)
{
	struct ast_sorcery_object_wizard *object_wizard = obj;

	if (!object_wizard->caching || !object_wizard->wizard->create) {
		return 0;
	}

	object_wizard->wizard->create(object_wizard->data, arg);

	return 0;
}

void *ast_sorcery_retrieve_by_id(const struct ast_sorcery *sorcery, const char *type, const char *id)
{
	RAII_VAR(struct ast_sorcery_object_type *, object_type, ao2_find(sorcery->types, type, OBJ_KEY), ao2_cleanup);
	void *object = NULL;
	struct ao2_iterator i;
	struct ast_sorcery_object_wizard *wizard;
	unsigned int cached = 0;

	if (!object_type || ast_strlen_zero(id)) {
		return NULL;
	}

	i = ao2_iterator_init(object_type->wizards, 0);
	for (; (wizard = ao2_iterator_next(&i)); ao2_ref(wizard, -1)) {
		if (wizard->wizard->retrieve_id &&
		    !(object = wizard->wizard->retrieve_id(sorcery, wizard->data, object_type->name, id))) {
			continue;
		}

		cached = wizard->caching;

		ao2_ref(wizard, -1);
		break;
	}
        ao2_iterator_destroy(&i);

	if (!cached && object) {
		ao2_callback(object_type->wizards, 0, sorcery_cache_create, object);
	}

	return object;
}

void *ast_sorcery_retrieve_by_fields(const struct ast_sorcery *sorcery, const char *type, unsigned int flags, struct ast_variable *fields)
{
	RAII_VAR(struct ast_sorcery_object_type *, object_type, ao2_find(sorcery->types, type, OBJ_KEY), ao2_cleanup);
	void *object = NULL;
	struct ao2_iterator i;
	struct ast_sorcery_object_wizard *wizard;
	unsigned int cached = 0;

	if (!object_type) {
		return NULL;
	}

	/* If returning multiple objects create a container to store them in */
	if ((flags & AST_RETRIEVE_FLAG_MULTIPLE)) {
		if (!(object = ao2_container_alloc_options(AO2_ALLOC_OPT_LOCK_NOLOCK, 1, NULL, NULL))) {
			return NULL;
		}
	}

	/* Inquire with the available wizards for retrieval */
	i = ao2_iterator_init(object_type->wizards, 0);
	for (; (wizard = ao2_iterator_next(&i)); ao2_ref(wizard, -1)) {
		if ((flags & AST_RETRIEVE_FLAG_MULTIPLE)) {
			if (wizard->wizard->retrieve_multiple) {
				wizard->wizard->retrieve_multiple(sorcery, wizard->data, object_type->name, object, fields);
			}
		} else if (fields && wizard->wizard->retrieve_fields) {
			if (wizard->wizard->retrieve_fields) {
				object = wizard->wizard->retrieve_fields(sorcery, wizard->data, object_type->name, fields);
			}
		}

		if ((flags & AST_RETRIEVE_FLAG_MULTIPLE) || !object) {
			continue;
		}

		cached = wizard->caching;

		ao2_ref(wizard, -1);
		break;
	}
	ao2_iterator_destroy(&i);

	/* If we are returning a single object and it came from a non-cache source create it in any caches */
	if (!(flags & AST_RETRIEVE_FLAG_MULTIPLE) && !cached && object) {
		ao2_callback(object_type->wizards, 0, sorcery_cache_create, object);
	}

	return object;
}

/*! \brief Internal function which returns if the wizard has created the object */
static int sorcery_wizard_create(void *obj, void *arg, int flags)
{
	const struct ast_sorcery_object_wizard *object_wizard = obj;

	return (!object_wizard->caching && !object_wizard->wizard->create(object_wizard->data, arg)) ? CMP_MATCH | CMP_STOP : 0;
}

int ast_sorcery_create(const struct ast_sorcery *sorcery, void *object)
{
	const struct ast_sorcery_object_details *details = object;
	RAII_VAR(struct ast_sorcery_object_type *, object_type, ao2_find(sorcery->types, details->type, OBJ_KEY), ao2_cleanup);
	RAII_VAR(struct ast_sorcery_object_wizard *, object_wizard, NULL, ao2_cleanup);

	if (!object_type) {
		return -1;
	}

	object_wizard = ao2_callback(object_type->wizards, 0, sorcery_wizard_create, object);

	return object_wizard ? 0 : -1;
}

/*! \brief Internal function which returns if a wizard has updated the object */
static int sorcery_wizard_update(void *obj, void *arg, int flags)
{
	const struct ast_sorcery_object_wizard *object_wizard = obj;

	return (object_wizard->wizard->update && !object_wizard->wizard->update(object_wizard->data, arg) &&
		!object_wizard->caching) ? CMP_MATCH | CMP_STOP : 0;
}

int ast_sorcery_update(const struct ast_sorcery *sorcery, void *object)
{
	const struct ast_sorcery_object_details *details = object;
	RAII_VAR(struct ast_sorcery_object_type *, object_type, ao2_find(sorcery->types, details->type, OBJ_KEY), ao2_cleanup);
	RAII_VAR(struct ast_sorcery_object_wizard *, object_wizard, NULL, ao2_cleanup);

	if (!object_type) {
		return -1;
	}

	object_wizard = ao2_callback(object_type->wizards, 0, sorcery_wizard_update, object);

	return object_wizard ? 0 : -1;
}

/*! \brief Internal function which returns if a wizard has deleted the object */
static int sorcery_wizard_delete(void *obj, void *arg, int flags)
{
	const struct ast_sorcery_object_wizard *object_wizard = obj;

	return (object_wizard->wizard->delete && !object_wizard->wizard->delete(object_wizard->data, arg) &&
		!object_wizard->caching) ? CMP_MATCH | CMP_STOP : 0;
}

int ast_sorcery_delete(const struct ast_sorcery *sorcery, void *object)
{
	const struct ast_sorcery_object_details *details = object;
	RAII_VAR(struct ast_sorcery_object_type *, object_type, ao2_find(sorcery->types, details->type, OBJ_KEY), ao2_cleanup);
	RAII_VAR(struct ast_sorcery_object_wizard *, object_wizard, NULL, ao2_cleanup);

	if (!object_type) {
		return -1;
	}

	object_wizard = ao2_callback(object_type->wizards, 0, sorcery_wizard_delete, object);

	return object_wizard ? 0 : -1;
}

void ast_sorcery_unref(struct ast_sorcery *sorcery)
{
	ao2_cleanup(sorcery);
}

const char *ast_sorcery_object_get_id(const void *object)
{
	const struct ast_sorcery_object_details *details = object;
	return details->id;
}

const char *ast_sorcery_object_get_type(const void *object)
{
	const struct ast_sorcery_object_details *details = object;
	return details->type;
}
