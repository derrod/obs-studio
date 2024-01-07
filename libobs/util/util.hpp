/*
 * Copyright (c) 2023 Lain Bailey <lain@obsproject.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* Useful C++ classes/bindings for util data and pointers */

#pragma once

#include <string.h>
#include <stdarg.h>
#include <utility>
#include <string>

#include "bmem.h"
#include "config-file.h"
#include "text-lookup.h"

/* RAII wrappers */

template<typename T> class BPtr {
	T *ptr;

	BPtr(BPtr const &) = delete;

	BPtr &operator=(BPtr const &) = delete;

public:
	inline BPtr(T *p = nullptr) : ptr(p) {}
	inline BPtr(BPtr &&other) { *this = std::move(other); }
	inline ~BPtr() { bfree(ptr); }

	inline T *operator=(T *p)
	{
		bfree(ptr);
		ptr = p;
		return p;
	}

	inline BPtr &operator=(BPtr &&other)
	{
		ptr = other.ptr;
		other.ptr = nullptr;
		return *this;
	}

	inline operator T *() { return ptr; }
	inline T **operator&()
	{
		bfree(ptr);
		ptr = nullptr;
		return &ptr;
	}

	inline bool operator!() { return ptr == NULL; }
	inline bool operator==(T p) { return ptr == p; }
	inline bool operator!=(T p) { return ptr != p; }

	inline T *Get() const { return ptr; }
};

class ConfigValue {
	config_t *config;
	const char *section;
	const char *key;

public:
	ConfigValue(config_t *config, const char *section, const char *key)
		: config(config),
		  section(section),
		  key(key)
	{
	}

	/* No copy, move or assignment operators */
	ConfigValue(ConfigValue &&) = delete;
	ConfigValue(ConfigValue const &) = delete;
	ConfigValue &operator=(ConfigValue const &) = delete;

	/* Getters w/ conversions */
	operator int64_t() const
	{
		return config_get_int(config, section, key);
	}

	operator int32_t() const
	{
		return static_cast<int32_t>(
			config_get_int(config, section, key));
	}

	operator uint64_t() const
	{
		return config_get_uint(config, section, key);
	}

	operator uint32_t() const
	{
		return static_cast<uint32_t>(
			config_get_uint(config, section, key));
	}

	operator double() const
	{
		return config_get_double(config, section, key);
	}

	operator float() const
	{
		return static_cast<float>(
			config_get_double(config, section, key));
	}

	operator bool() const { return config_get_bool(config, section, key); }

	operator const char *() const
	{
		return config_get_string(config, section, key);
	}

	operator std::string() const
	{
		return config_get_string(config, section, key);
	}

	/* Setters */
	ConfigValue &operator=(const char *value)
	{
		config_set_string(config, section, key, value);
		return *this;
	}

	ConfigValue &operator=(const std::string &value)
	{
		config_set_string(config, section, key, value.c_str());
		return *this;
	}

	ConfigValue &operator=(const bool value)
	{
		config_set_bool(config, section, key, value);
		return *this;
	}

	ConfigValue &operator=(const int64_t value)
	{
		config_set_int(config, section, key, value);
		return *this;
	}

	ConfigValue &operator=(const int32_t value)
	{
		config_set_int(config, section, key, value);
		return *this;
	}

	ConfigValue &operator=(const uint32_t value)
	{
		config_set_uint(config, section, key, value);
		return *this;
	}

	ConfigValue &operator=(const uint64_t value)
	{
		config_set_uint(config, section, key, value);
		return *this;
	}

	ConfigValue &operator=(const double value)
	{
		config_set_double(config, section, key, value);
		return *this;
	}

	ConfigValue &operator=(const float value)
	{
		config_set_double(config, section, key, value);
		return *this;
	}

	/* Default setters */
	ConfigValue &operator^=(const char *value)
	{
		config_set_default_string(config, section, key, value);
		return *this;
	}

	ConfigValue &operator^=(const std::string &value)
	{
		config_set_default_string(config, section, key, value.c_str());
		return *this;
	}

	ConfigValue &operator^=(const bool value)
	{
		config_set_default_bool(config, section, key, value);
		return *this;
	}

	ConfigValue &operator^=(const int64_t value)
	{
		config_set_default_int(config, section, key, value);
		return *this;
	}

	ConfigValue &operator^=(const int32_t value)
	{
		config_set_default_int(config, section, key, value);
		return *this;
	}

	ConfigValue &operator^=(const uint32_t value)
	{
		config_set_default_uint(config, section, key, value);
		return *this;
	}

	ConfigValue &operator^=(const uint64_t value)
	{
		config_set_default_uint(config, section, key, value);
		return *this;
	}

	ConfigValue &operator^=(const double value)
	{
		config_set_default_double(config, section, key, value);
		return *this;
	}

	ConfigValue &operator^=(const float value)
	{
		config_set_default_double(config, section, key, value);
		return *this;
	}

	/* Checks */

	bool remove() const
	{
		return config_remove_value(config, section, key);
	}

	bool exists() const
	{
		return config_has_user_value(config, section, key);
	}
};

class ConfigSection {
	config_t *config;
	const char *section;

public:
	ConfigSection(config_t *config, const char *section)
		: config(config),
		  section(section)
	{
	}

	/* No copy, move or assignment operators */
	ConfigSection(ConfigSection &&) = delete;
	ConfigSection(ConfigSection const &) = delete;
	ConfigSection &operator=(ConfigSection const &) = delete;

	ConfigValue operator[](const char *key)
	{
		return {config, section, key};
	}

	template<typename T> T get(const char *key)
	{
		return ConfigValue(config, section, key);
	}

	template<typename T> void set(const char *key, T value)
	{
		operator[](key).operator=(value);
	}

	template<typename T> void set_default(const char *key, T value)
	{
		operator[](key).operator^=(value);
	}

	bool remove(const char *key) const
	{
		return config_remove_value(config, section, key);
	}

	bool exists(const char *key) const
	{
		return config_has_user_value(config, section, key);
	}
};

class ConfigFile {
	config_t *config;

	ConfigFile(ConfigFile const &) = delete;
	ConfigFile &operator=(ConfigFile const &) = delete;

public:
	inline ConfigFile() : config(NULL) {}
	inline ConfigFile(ConfigFile &&other) noexcept : config(other.config)
	{
		other.config = nullptr;
	}
	inline ~ConfigFile() { config_close(config); }

	inline bool Create(const char *file)
	{
		Close();
		config = config_create(file);
		return config != NULL;
	}

	inline void Swap(ConfigFile &other)
	{
		config_t *newConfig = other.config;
		other.config = config;
		config = newConfig;
	}

	inline int OpenString(const char *str)
	{
		Close();
		return config_open_string(&config, str);
	}

	inline int Open(const char *file, config_open_type openType)
	{
		Close();
		return config_open(&config, file, openType);
	}

	inline int Save() { return config_save(config); }

	inline int SaveSafe(const char *temp_ext,
			    const char *backup_ext = nullptr)
	{
		return config_save_safe(config, temp_ext, backup_ext);
	}

	inline void Close()
	{
		config_close(config);
		config = NULL;
	}

	inline operator config_t *() const { return config; }

	ConfigSection operator[](const char *section)
	{
		return {config, section};
	}
};

class TextLookup {
	lookup_t *lookup;

	TextLookup(TextLookup const &) = delete;

	TextLookup &operator=(TextLookup const &) = delete;

public:
	inline TextLookup(lookup_t *lookup = nullptr) : lookup(lookup) {}
	inline TextLookup(TextLookup &&other) noexcept : lookup(other.lookup)
	{
		other.lookup = nullptr;
	}
	inline ~TextLookup() { text_lookup_destroy(lookup); }

	inline TextLookup &operator=(lookup_t *val)
	{
		text_lookup_destroy(lookup);
		lookup = val;
		return *this;
	}

	inline operator lookup_t *() const { return lookup; }

	inline const char *GetString(const char *lookupVal) const
	{
		const char *out;
		if (!text_lookup_getstr(lookup, lookupVal, &out))
			return lookupVal;

		return out;
	}
};
