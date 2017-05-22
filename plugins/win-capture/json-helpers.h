#pragma once

extern const char *get_string_val(json_t *json_object, const char *key);
extern int get_int_val(json_t *json_object, const char *key);
extern bool get_bool_val(json_t *json_object, const char *key);
extern json_t *open_compat_file(void);
