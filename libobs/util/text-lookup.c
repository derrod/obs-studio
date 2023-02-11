/*
 * Copyright (c) 2013 Hugh Bailey <obs.jim@gmail.com>
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

#include <ctype.h>

#include "dstr.h"
#include "text-lookup.h"
#include "lexer.h"
#include "platform.h"

#define USE_SWISSTABLES

#ifdef USE_SWISSTABLES
#include "util/experimental/cwisstable.h"
#else
#include <uthash/uthash.h>

#ifdef UTHASH_USE_OBS_ALLOCATOR
#undef uthash_malloc
#undef uthash_free
#define uthash_malloc(sz) bmalloc(sz)
#define uthash_free(ptr, sz) bfree(ptr);
#endif
#endif

/* ------------------------------------------------------------------------- */
#ifdef USE_SWISSTABLES

// NOTE: THIS REQUIRES FUCKING MSVC TO USE THE STANDARDS CONFORMING PREPROCESSOR AS WELL
// AS C11. YES THIS IS ANNOYING AND MICROSOFT IS STUPID, BUT IT'LL WORK!
// WELL, YOU ALSO NEED TWO OF THE PATCHES FROM GOOGLE'S PR TO ENABLE MSVC SUPPORT FOR
// CWISSTABLES. AND THAT PR IS SO OLD, MICROSOFT ACTUALLY CAUGHT UP WITH STANDARDS SUPPORT!
// SO YOU ONLY NEED THE ONE TO REMOVE ATOMICS AND THE ONE WITH NATIVE 128 BIT SHIT.
// AND THEN IT'LL "WORK" BUT IT'S SLOWER THAN FUCKING UTHASH SO IT'S NOT EVEN WORTH IT.
// ALSO I HAVE MY CAPSLOCK KEY DISABLED ON THIS KEYBOARD SO I HOPE YOU APPRECIATE ME
// MANUALLY HOLDING DOWN SHIFT TO YELL ALL OF THIS.

static inline void kCStrPolicy_copy(void *dst, const void *src)
{
	typedef struct {
		char *k;
		char *v;
	} entry;
	const entry *e = (const entry *)src;
	entry *d = (entry *)dst;

	d->k = bstrdup(e->k);
	d->v = bstrdup(e->v);
}

static inline void kCStrPolicy_dtor(void *val)
{
	typedef struct {
		char *k;
		char *v;
	} entry;

	entry *e = (entry *)val;
	bfree(e->k);
	bfree(e->v);
}

static inline size_t kCStrPolicy_hash(const void *val)
{
	const char *str = *(const char *const *)val;
	size_t len = strlen(str);
	// CWISS_FxHash_State state = 0;
	// CWISS_FxHash_Write(&state, str, len);
	CWISS_AbslHash_State state = CWISS_AbslHash_kInit;
	CWISS_AbslHash_Write(&state, str, len);
	return state;
}

static inline bool kCStrPolicy_eq(const void *a, const void *b)
{
	const char *ap = *(const char *const *)a;
	const char *bp = *(const char *const *)b;
	return strcmp(ap, bp) == 0;
}

CWISS_DECLARE_NODE_MAP_POLICY(kCStrPolicy, const char *, char *,
			      (obj_copy, kCStrPolicy_copy),
			      (obj_dtor, kCStrPolicy_dtor),
			      (key_hash, kCStrPolicy_hash),
			      (key_eq, kCStrPolicy_eq));

CWISS_DECLARE_HASHMAP_WITH(LookupMap, const char *, char *, kCStrPolicy);

struct text_lookup {
	struct dstr language;
	LookupMap map;
};
#else
struct text_item {
	char *lookup, *value;
	UT_hash_handle hh;
};

static inline void text_item_destroy(struct text_item *item)
{
	bfree(item->lookup);
	bfree(item->value);
	bfree(item);
}

struct text_lookup {
	struct dstr language;
	struct text_item *items;
};
#endif

/* ------------------------------------------------------------------------- */

static void lookup_getstringtoken(struct lexer *lex, struct strref *token)
{
	const char *temp = lex->offset;
	bool was_backslash = false;

	while (*temp != 0 && *temp != '\n') {
		if (!was_backslash) {
			if (*temp == '\\') {
				was_backslash = true;
			} else if (*temp == '"') {
				temp++;
				break;
			}
		} else {
			was_backslash = false;
		}

		++temp;
	}

	token->len += (size_t)(temp - lex->offset);

	if (*token->array == '"') {
		token->array++;
		token->len--;

		if (*(temp - 1) == '"')
			token->len--;
	}

	lex->offset = temp;
}

static bool lookup_gettoken(struct lexer *lex, struct strref *str)
{
	struct base_token temp;

	base_token_clear(&temp);
	strref_clear(str);

	while (lexer_getbasetoken(lex, &temp, PARSE_WHITESPACE)) {
		char ch = *temp.text.array;

		if (!str->array) {
			/* comments are designated with a #, and end at LF */
			if (ch == '#') {
				while (ch != '\n' && ch != 0)
					ch = *(++lex->offset);
			} else if (temp.type == BASETOKEN_WHITESPACE) {
				strref_copy(str, &temp.text);
				break;
			} else {
				strref_copy(str, &temp.text);
				if (ch == '"') {
					lookup_getstringtoken(lex, str);
					break;
				} else if (ch == '=') {
					break;
				}
			}
		} else {
			if (temp.type == BASETOKEN_WHITESPACE ||
			    *temp.text.array == '=') {
				lex->offset -= temp.text.len;
				break;
			}

			if (ch == '#') {
				lex->offset--;
				break;
			}

			str->len += temp.text.len;
		}
	}

	return (str->len != 0);
}

static inline bool lookup_goto_nextline(struct lexer *p)
{
	struct strref val;
	bool success = true;

	strref_clear(&val);

	while (true) {
		if (!lookup_gettoken(p, &val)) {
			success = false;
			break;
		}
		if (*val.array == '\n')
			break;
	}

	return success;
}

static char *convert_string(const char *str, size_t len)
{
	struct dstr out;
	out.array = bstrdup_n(str, len);
	out.capacity = len + 1;
	out.len = len;

	dstr_replace(&out, "\\n", "\n");
	dstr_replace(&out, "\\t", "\t");
	dstr_replace(&out, "\\r", "\r");
	dstr_replace(&out, "\\\"", "\"");

	return out.array;
}

static void lookup_addfiledata(struct text_lookup *lookup,
			       const char *file_data)
{
	struct lexer lex;
	struct strref name, value;

	lexer_init(&lex);
	lexer_start(&lex, file_data);
	strref_clear(&name);
	strref_clear(&value);

	while (lookup_gettoken(&lex, &name)) {
		bool got_eq = false;

		if (*name.array == '\n')
			continue;
	getval:
		if (!lookup_gettoken(&lex, &value))
			break;
		if (*value.array == '\n')
			continue;
		else if (!got_eq && *value.array == '=') {
			got_eq = true;
			goto getval;
		}

#ifdef USE_SWISSTABLES
		LookupMap_Entry item = {0};
		item.key = bstrdup_n(name.array, name.len);
		item.val = convert_string(value.array, value.len);

		LookupMap_insert(&lookup->map, &item);
#else
		item = bzalloc(sizeof(struct text_item));
		item->lookup = bstrdup_n(name.array, name.len);
		item->value = convert_string(value.array, value.len);

		HASH_REPLACE_STR(lookup->items, lookup, item, old);

		if (old)
			text_item_destroy(old);
#endif
		if (!lookup_goto_nextline(&lex))
			break;
	}

	lexer_free(&lex);
}

static inline bool lookup_getstring(const char *lookup_val, const char **out,
				    struct text_lookup *lookup)
{
#ifdef USE_SWISSTABLES
	if (!&lookup->map)
		return false;

	LookupMap_Iter it = LookupMap_find(&lookup->map, &lookup_val);
	LookupMap_Entry *e = LookupMap_Iter_get(&it);

	if (!e)
		return false;

	*out = e->val;

#else
	struct text_item *item;
	if (!lookup->items)
		return false;

	HASH_FIND_STR(lookup->items, lookup_val, item);

	if (!item)
		return false;

	*out = item->value;
#endif
	return true;
}

/* ------------------------------------------------------------------------- */

lookup_t *text_lookup_create(const char *path)
{
	struct text_lookup *lookup = bzalloc(sizeof(struct text_lookup));

	lookup->map = LookupMap_new(8);

	if (!text_lookup_add(lookup, path)) {
		bfree(lookup);
		lookup = NULL;
	}

	return lookup;
}

bool text_lookup_add(lookup_t *lookup, const char *path)
{
	struct dstr file_str;
	char *temp = NULL;
	FILE *file;

	file = os_fopen(path, "rb");
	if (!file)
		return false;

	os_fread_utf8(file, &temp);
	dstr_init_move_array(&file_str, temp);
	fclose(file);

	if (!file_str.array)
		return false;

	dstr_replace(&file_str, "\r", " ");
	lookup_addfiledata(lookup, file_str.array);
	dstr_free(&file_str);

	return true;
}

void text_lookup_destroy(lookup_t *lookup)
{
	if (lookup) {
#ifndef USE_SWISSTABLES
		struct text_item *item, *tmp;
		HASH_ITER(hh, lookup->items, item, tmp)
		{
			HASH_DELETE(hh, lookup->items, item);
			text_item_destroy(item);
		}
#endif
		dstr_free(&lookup->language);
		bfree(lookup);
	}
}

bool text_lookup_getstr(lookup_t *lookup, const char *lookup_val,
			const char **out)
{
	if (lookup)
		return lookup_getstring(lookup_val, out, lookup);
	return false;
}
