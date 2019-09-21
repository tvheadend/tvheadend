/*
 *  Sorted String List Functions
 *  Copyright (C) 2017 Tvheadend Foundation CIC
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef STRING_LIST_H
#define STRING_LIST_H

#include "redblack.h"

/*
 * External forward decls
 */
struct htsmsg;

/// Simple _sorted_ string list type and helper functions.
/// The htsmsg implements lists and maps but they are unsorted.
/// This implements a simple api for keeping track of sorted
/// strings. Only one copy of the string is kept in the list
/// (duplicates are not stored). The list owns the memory
/// for the strings (takes a copy).
///
/// Example:
/// string_list_create_t *l = string_list_create();
/// string_list_insert(l, "str1");
/// string_list_insert(l, "str2");
/// string_list_destroy(l);

struct string_list_item {
  RB_ENTRY(string_list_item) h_link;
  char id[0];
};

typedef struct string_list_item string_list_item_t;
typedef RB_HEAD(string_list, string_list_item) string_list_t;

/// Initialize the memory used by the list.
void string_list_init(string_list_t *l);
/// Dynamically allocate and initialize a list.
string_list_t *string_list_create(void);

/// Free up the memory used by the list.
void string_list_destroy(string_list_t *l);

/// Insert a copy of id in to the sorted string list.
void string_list_insert(string_list_t *l, const char *id);

/// Insert a copy of lowercase id in to the sorted string list.
void string_list_insert_lowercase(string_list_t *l, const char *id);

/// Remove the first entry from the list and return ownership to the
/// caller.
char *string_list_remove_first(string_list_t *l)
  __attribute__((warn_unused_result));

/// Conversion function from sorted string list to an htsmsg.
/// @return NULL if empty.
struct htsmsg *string_list_to_htsmsg(const string_list_t *l)
    __attribute__((warn_unused_result));
string_list_t *htsmsg_to_string_list(const struct htsmsg *m)
    __attribute__((warn_unused_result));
void string_list_serialize(const string_list_t *l, struct htsmsg *m, const char *f);
string_list_t *string_list_deserialize(const struct htsmsg *m, const char *f)
    __attribute__((warn_unused_result));
char *string_list_2_csv(const string_list_t *l, char delim, int human)
    __attribute__((warn_unused_result));
int string_list_cmp(const string_list_t *m1, const string_list_t *m2)
    __attribute__((warn_unused_result));
/// Deep clone (shares no pointers, so have to string_list_destroy both.
string_list_t *string_list_copy(const string_list_t *src)
    __attribute__((warn_unused_result));

/// Searching
int string_list_contains_string(const string_list_t *src, const char *find);

#endif
