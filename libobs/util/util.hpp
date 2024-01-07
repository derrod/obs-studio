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
#include <memory>

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

/* RAII wrapper that just exists to keep the config open until the last shared_ptr is released */
class ConfigPtr {
	config_t *config;

public:
	ConfigPtr() : config(nullptr){};
	ConfigPtr(config_t *config) : config(config) {}

	ConfigPtr(ConfigPtr &&) = delete;
	ConfigPtr(ConfigPtr const &) = delete;
	ConfigPtr &operator=(ConfigPtr const &) = delete;

	~ConfigPtr() { config_close(config); }

	operator config_t *() const { return config; }
	operator config_t **() { return &config; }
};

using ConfigRef = std::shared_ptr<ConfigPtr>;

class ConfigValue {
	friend class ConfigSection;

	ConfigRef config;
	std::string section;
	std::string key;

	ConfigValue(ConfigRef config, std::string section, const char *key)
		: config(std::move(config)),
		  section(std::move(section)),
		  key(key)
	{
	}

public:
	/* No copy, move or assignment operators */
	ConfigValue(ConfigValue &&) = delete;
	ConfigValue(ConfigValue const &) = delete;
	ConfigValue &operator=(ConfigValue const &) = delete;

	/* Getters w/ conversions */
	operator int64_t() const
	{
		return config_get_int(*config, section.c_str(), key.c_str());
	}

	operator int32_t() const
	{
		return static_cast<int32_t>(
			config_get_int(*config, section.c_str(), key.c_str()));
	}

	operator uint64_t() const
	{
		return config_get_uint(*config, section.c_str(), key.c_str());
	}

	operator uint32_t() const
	{
		return static_cast<uint32_t>(
			config_get_uint(*config, section.c_str(), key.c_str()));
	}

	operator double() const
	{
		return config_get_double(*config, section.c_str(), key.c_str());
	}

	operator float() const
	{
		return static_cast<float>(config_get_double(
			*config, section.c_str(), key.c_str()));
	}

	operator bool() const
	{
		return config_get_bool(*config, section.c_str(), key.c_str());
	}

	operator const char *() const
	{
		return config_get_string(*config, section.c_str(), key.c_str());
	}

	operator std::string() const
	{
		return config_get_string(*config, section.c_str(), key.c_str());
	}

	/* Setters */
	ConfigValue &operator=(const char *value)
	{
		config_set_string(*config, section.c_str(), key.c_str(), value);
		return *this;
	}

	ConfigValue &operator=(const std::string &value)
	{
		config_set_string(*config, section.c_str(), key.c_str(),
				  value.c_str());
		return *this;
	}

	ConfigValue &operator=(const bool value)
	{
		config_set_bool(*config, section.c_str(), key.c_str(), value);
		return *this;
	}

	ConfigValue &operator=(const int64_t value)
	{
		config_set_int(*config, section.c_str(), key.c_str(), value);
		return *this;
	}

	ConfigValue &operator=(const int32_t value)
	{
		config_set_int(*config, section.c_str(), key.c_str(), value);
		return *this;
	}

	ConfigValue &operator=(const uint32_t value)
	{
		config_set_uint(*config, section.c_str(), key.c_str(), value);
		return *this;
	}

	ConfigValue &operator=(const uint64_t value)
	{
		config_set_uint(*config, section.c_str(), key.c_str(), value);
		return *this;
	}

	ConfigValue &operator=(const double value)
	{
		config_set_double(*config, section.c_str(), key.c_str(), value);
		return *this;
	}

	ConfigValue &operator=(const float value)
	{
		config_set_double(*config, section.c_str(), key.c_str(), value);
		return *this;
	}

	/* Default setters */
	ConfigValue &operator^=(const char *value)
	{
		config_set_default_string(*config, section.c_str(), key.c_str(),
					  value);
		return *this;
	}

	ConfigValue &operator^=(const std::string &value)
	{
		config_set_default_string(*config, section.c_str(), key.c_str(),
					  value.c_str());
		return *this;
	}

	ConfigValue &operator^=(const bool value)
	{
		config_set_default_bool(*config, section.c_str(), key.c_str(),
					value);
		return *this;
	}

	ConfigValue &operator^=(const int64_t value)
	{
		config_set_default_int(*config, section.c_str(), key.c_str(),
				       value);
		return *this;
	}

	ConfigValue &operator^=(const int32_t value)
	{
		config_set_default_int(*config, section.c_str(), key.c_str(),
				       value);
		return *this;
	}

	ConfigValue &operator^=(const uint32_t value)
	{
		config_set_default_uint(*config, section.c_str(), key.c_str(),
					value);
		return *this;
	}

	ConfigValue &operator^=(const uint64_t value)
	{
		config_set_default_uint(*config, section.c_str(), key.c_str(),
					value);
		return *this;
	}

	ConfigValue &operator^=(const double value)
	{
		config_set_default_double(*config, section.c_str(), key.c_str(),
					  value);
		return *this;
	}

	ConfigValue &operator^=(const float value)
	{
		config_set_default_double(*config, section.c_str(), key.c_str(),
					  value);
		return *this;
	}

	/* Checks */

	bool remove() const
	{
		return config_remove_value(*config, section.c_str(),
					   key.c_str());
	}

	bool exists() const
	{
		return config_has_user_value(*config, section.c_str(),
					     key.c_str());
	}
};

class ConfigSection {
	friend class ConfigFile;

	ConfigRef config;
	std::string section;

	ConfigSection(ConfigRef config, const char *section)
		: config(std::move(config)),
		  section(section)
	{
	}

public:
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
		return config_remove_value(*config, section.c_str(), key);
	}

	bool contains(const char *key) const
	{
		return config_has_user_value(*config, section.c_str(), key);
	}
};

class ConfigFile {
	ConfigRef config;

	ConfigFile(ConfigFile const &) = delete;
	ConfigFile &operator=(ConfigFile const &) = delete;

public:
	ConfigFile() : config(new ConfigPtr) {}

	ConfigFile(ConfigFile &&other) noexcept
	{
		config.swap(other.config);
		other.config.reset();
	}

	~ConfigFile() { config.reset(); }

	void Swap(ConfigFile &other) { config.swap(other.config); }

	bool Create(const char *file)
	{
		config_t *conf = config_create(file);
		config.reset(new ConfigPtr(conf));
		return conf != nullptr;
	}

	int OpenString(const char *str)
	{
		config.reset(new ConfigPtr);
		return config_open_string(*config, str);
	}

	int Open(const char *file, config_open_type openType)
	{
		config.reset(new ConfigPtr);
		return config_open(*config, file, openType);
	}

	int Save() { return config_save(*config); }

	int SaveSafe(const char *temp_ext, const char *backup_ext = nullptr)
	{
		return config_save_safe(*config, temp_ext, backup_ext);
	}

	void Close() { config.reset(); }

	operator config_t *() const { return *config; }

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
