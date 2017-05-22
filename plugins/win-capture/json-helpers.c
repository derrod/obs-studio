#include <jansson.h>
#include <stdbool.h>
#include <util/platform.h>
#include <util/base.h>
#include <util/bmem.h>
#include <obs-module.h>
#include "compat-format-version.h"

const char *get_string_val(json_t *json_object, const char *key)
{
	json_t *str_val = json_object_get(json_object, key);
	if (!str_val || !json_is_string(str_val))
		return NULL;

	return json_string_value(str_val);
}

int get_int_val(json_t *json_object, const char *key)
{
	json_t *integer_val = json_object_get(json_object, key);
	if (!integer_val || !json_is_integer(integer_val))
		return 0;

	return (int)json_integer_value(integer_val);
}

bool get_bool_val(json_t *json_object, const char *key)
{
	json_t *bool_val = json_object_get(json_object, key);
	if (!bool_val || !json_is_boolean(bool_val))
		return false;

	return json_is_true(bool_val);
}

static json_t *open_json_file(const char *file)
{
	char         *file_data = os_quick_read_utf8_file(file);
	json_error_t error;
	json_t       *root;
	json_t       *list;
	int          format_ver;

	if (!file_data)
		return NULL;

	root = json_loads(file_data, JSON_REJECT_DUPLICATES, &error);
	bfree(file_data);

	if (!root) {
		blog(LOG_WARNING, "win-capture.c: [open_json_file] "
			"Error reading JSON file (%d): %s",
			error.line, error.text);
		return NULL;
	}

	format_ver = get_int_val(root, "format_version");

	if (format_ver != COMPAT_FORMAT_VERSION) {
		blog(LOG_WARNING, "win-capture.c: [open_json_file] "
			"Wrong format version (%d), expected %d",
			format_ver, COMPAT_FORMAT_VERSION);
		json_decref(root);
		return NULL;
	}

	list = json_object_get(root, "compat");
	if (list)
		json_incref(list);
	json_decref(root);

	if (!list) {
		blog(LOG_WARNING, "win-capture.c: [open_json_file] "
			"No compatability list");
		return NULL;
	}

	return list;
}

json_t *open_compat_file(void)
{
	char *file;
	json_t *root = NULL;

	file = obs_module_config_path("compat.json");
	if (file) {
		root = open_json_file(file);
		bfree(file);
	}

	if (!root) {
		file = obs_module_file("compat.json");
		if (file) {
			root = open_json_file(file);
			bfree(file);
		}
	}

	return root;
}