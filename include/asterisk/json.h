/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2012 - 2013, Digium, Inc.
 *
 * David M. Lee, II <dlee@digium.com>
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

#ifndef _ASTERISK_JSON_H
#define _ASTERISK_JSON_H

/*! \file
 *
 * \brief Asterisk JSON abstraction layer.
 * \since 12.0.0
 *
 * This is a very thin wrapper around the Jansson API. For more details on it, see its
 * docs at http://www.digip.org/jansson/doc/2.4/apiref.html.

 * \author David M. Lee, II <dlee@digium.com>
 */

/*!@{*/

/*!
 * \brief Set custom allocators instead of the standard ast_malloc() and ast_free().
 * \since 12.0.0
 *
 * This is used by the unit tests to do JSON specific memory leak detection. Since it
 * affects all users of the JSON library, shouldn't normally be used.
 *
 * \param malloc_fn Custom allocation function.
 * \param free_fn Matching free function.
 */
void ast_json_set_alloc_funcs(void *(*malloc_fn)(size_t), void (*free_fn)(void*));

/*!
 * \brief Change alloc funcs back to the resource module defaults.
 * \since 12.0.0
 *
 * If you use ast_json_set_alloc_funcs() to temporarily change the allocator functions
 * (i.e., from in a unit test), this function sets them back to ast_malloc() and
 * ast_free().
 */
void ast_json_reset_alloc_funcs(void);

/*!
 * \struct ast_json
 * \brief Abstract JSON element (object, array, string, int, ...).
 * \since 12.0.0
 */
struct ast_json;

/*!
 * \brief Increase refcount on \a value.
 * \since 12.0.0
 *
 * \param value JSON value to reference.
 * \return The given \a value.
 */
struct ast_json *ast_json_ref(struct ast_json *value);

/*!
 * \brief Decrease refcount on \a value. If refcount reaches zero, \a value is freed.
 * \since 12.0.0
 */
void ast_json_unref(struct ast_json *value);

/*!@}*/

/*!@{*/

/*!
 * \brief Valid types of a JSON element.
 * \since 12.0.0
 */
enum ast_json_type
{
	AST_JSON_OBJECT,
	AST_JSON_ARRAY,
	AST_JSON_STRING,
	AST_JSON_INTEGER,
	AST_JSON_REAL,
	AST_JSON_TRUE,
	AST_JSON_FALSE,
	AST_JSON_NULL,
};

/*!
 * \brief Get the type of \a value.
 * \since 12.0.0
 * \param value Value to query.
 * \return Type of \a value.
 */
enum ast_json_type ast_json_typeof(const struct ast_json *value);

/*!@}*/

/*!@{*/

/*!
 * \brief Get the JSON true value.
 * \since 12.0.0
 *
 * The returned value is a singleton, and does not need to be
 * ast_json_unref()'ed.
 *
 * \return JSON true.
 */
struct ast_json *ast_json_true(void);

/*!
 * \brief Get the JSON false value.
 * \since 12.0.0
 *
 * The returned value is a singleton, and does not need to be
 * ast_json_unref()'ed.
 *
 * \return JSON false.
 */
struct ast_json *ast_json_false(void);

/*!
 * \brief Get the JSON boolean corresponding to \a value.
 * \since 12.0.0
 * \return JSON true if value is true (non-zero).
 * \return JSON false if value is false (zero).
 */
struct ast_json *ast_json_boolean(int value);

/*!
 * \brief Get the JSON null value.
 * \since 12.0.0
 *
 * The returned value is a singleton, and does not need to be
 * ast_json_unref()'ed.
 *
 * \return JSON null.
 */
struct ast_json *ast_json_null(void);

/*!
 * \brief Check if \a value is JSON true.
 * \since 12.0.0
 * \return True (non-zero) if \a value == \ref ast_json_true().
 * \return False (zero) otherwise..
 */
int ast_json_is_true(const struct ast_json *value);

/*!
 * \brief Check if \a value is JSON false.
 * \since 12.0.0
 * \return True (non-zero) if \a value == \ref ast_json_false().
 * \return False (zero) otherwise.
 */
int ast_json_is_false(const struct ast_json *value);

/*!
 * \brief Check if \a value is JSON null.
 * \since 12.0.0
 * \return True (non-zero) if \a value == \ref ast_json_false().
 * \return False (zero) otherwise.
 */
int ast_json_is_null(const struct ast_json *value);

/*!@}*/

/*!@{*/

/*!
 * \brief Construct a JSON string from \a value.
 * \since 12.0.0
 *
 * The given \a value must be a valid ASCII or UTF-8 encoded string.
 *
 * \param value Value of new JSON string.
 * \return Newly constructed string element.
 * \return \c NULL on error.
 */
struct ast_json *ast_json_string_create(const char *value);

/*!
 * \brief Get the value of a JSON string.
 * \since 12.0.0
 * \param string JSON string.
 * \return Value of the string.
 * \return \c NULL on error.
 */
const char *ast_json_string_get(const struct ast_json *string);

/*!
 * \brief Change the value of a JSON string.
 * \since 12.0.0
 *
 * The given \a value must be a valid ASCII or UTF-8 encoded string.
 *
 * \param string JSON string to modify.
 * \param value New value to store in \a string.
 * \return 0 on success.
 * \return -1 on error.
 */
int ast_json_string_set(struct ast_json *string, const char *value);

/*!
 * \brief Create a JSON string, printf style.
 * \since 12.0.0
 *
 * The formatted value must be a valid ASCII or UTF-8 encoded string.
 *
 * \param format \c printf style format string.
 * \return Newly allocated string.
 * \return \c NULL on error.
 */
struct ast_json *ast_json_stringf(const char *format, ...) __attribute__((format(printf, 1, 2)));

/*!
 * \brief Create a JSON string, vprintf style.
 * \since 12.0.0
 *
 * The formatted value must be a valid ASCII or UTF-8 encoded string.
 *
 * \param format \c printf style format string.
 * \return Newly allocated string.
 * \return \c NULL on error.
 */
struct ast_json *ast_json_vstringf(const char *format, va_list args) __attribute__((format(printf, 1, 0)));

/*!@}*/

/*!@{*/

/*!
 * \brief Create a JSON integer.
 * \since 12.0.0
 * \param value Value of the new JSON integer.
 * \return Newly allocated integer.
 * \return \c NULL on error.
 */
struct ast_json *ast_json_integer_create(intmax_t value);

/*!
 * \brief Get the value from a JSON integer.
 * \since 12.0.0
 * \param integer JSON integer.
 * \return Value of a JSON integer.
 * \return 0 if \a integer is not a JSON integer.
 */
intmax_t ast_json_integer_get(const struct ast_json *integer);

/*!
 * \brief Set the value of a JSON integer.
 * \since 12.0.0
 * \param integer JSON integer to modify.
 * \param value New value for \a integer.
 * \return 0 on success.
 * \return -1 on error.
 */
int ast_json_integer_set(struct ast_json *integer, intmax_t value);

/*!@}*/

/*!@{*/

/*!
 * \brief Create a empty JSON array.
 * \since 12.0.0
 * \return Newly allocated array.
 * \return \c NULL on error.
 */
struct ast_json *ast_json_array_create(void);

/*!
 * \brief Get the size of a JSON array.
 * \since 12.0.0
 * \param array JSON array.
 * \return Size of \a array.
 * \return 0 if array is not a JSON array.
 */
size_t ast_json_array_size(const struct ast_json *array);

/*!
 * \brief Get an element from an array.
 * \since 12.0.0
 *
 * The returned element is a borrowed reference; use ast_json_ref() to safely keep a
 * pointer to it.
 *
 * \param array JSON array.
 * \param index Zero-based index into \a array.
 * \return The specified element.
 * \return \c NULL if \a array not an array.
 * \return \c NULL if \a index is out of bounds.
 */
struct ast_json *ast_json_array_get(const struct ast_json *array, size_t index);

/*!
 * \brief Change an element in an array.
 * \since 12.0.0
 *
 * The \a array steals the \a value reference; use ast_json_ref() to safely keep a pointer
 * to it.
 *
 * \param array JSON array to modify.
 * \param index Zero-based index into array.
 * \param value New JSON value to store in \a array at \a index.
 * \return 0 on success.
 * \return -1 on error.
 */
int ast_json_array_set(struct ast_json *array, size_t index, struct ast_json *value);

/*!
 * \brief Append to an array.
 * \since 12.0.0
 *
 * The array steals the \a value reference; use ast_json_ref() to safely keep a pointer
 * to it.
 *
 * \param array JSON array to modify.
 * \param value New JSON value to store at the end of \a array.
 * \return 0 on success.
 * \return -1 on error.
 */
int ast_json_array_append(struct ast_json *array, struct ast_json *value);

/*!
 * \brief Insert into an array.
 * \since 12.0.0
 *
 * The array steals the \a value reference; use ast_json_ref() to safely keep a pointer
 * to it.
 *
 * \param array JSON array to modify.
 * \param index Zero-based index into array.
 * \param value New JSON value to store in \a array at \a index.
 * \return 0 on success.
 * \return -1 on error.
 */
int ast_json_array_insert(struct ast_json *array, size_t index, struct ast_json *value);

/*!
 * \brief Remove an element from an array.
 * \since 12.0.0
 * \param array JSON array to modify.
 * \param index Zero-based index into array.
 * \return 0 on success.
 * \return -1 on error.
 */
int ast_json_array_remove(struct ast_json *array, size_t index);

/*!
 * \brief Remove all elements from an array.
 * \since 12.0.0
 * \param array JSON array to clear.
 * \return 0 on success.
 * \return -1 on error.
 */
int ast_json_array_clear(struct ast_json *array);

/*!
 * \brief Append all elements from \a tail to \a array.
 * \since 12.0.0
 *
 * The \a tail argument is not changed, so ast_json_unref() it when you are done with it.
 *
 * \param array JSON array to modify.
 * \param tail JSON array with contents to append to \a array.
 * \return 0 on success.
 * \return -1 on error.
 */
int ast_json_array_extend(struct ast_json *array, struct ast_json *tail);

/*!@}*/

/*!@{*/

/*!
 * \brief Create a new JSON object.
 * \since 12.0.0
 * \return Newly allocated object.
 * \return \c NULL on error.
 */
struct ast_json *ast_json_object_create(void);

/*!
 * \brief Get size of JSON object.
 * \since 12.0.0
 * \param object JSON object.
 * \return Size of \a object.
 * \return Zero of \a object is not a JSON object.
 */
size_t ast_json_object_size(struct ast_json *object);

/*!
 * \brief Get a field from a JSON object.
 * \since 12.0.0
 *
 * The returned element is a borrowed reference; use ast_json_ref() to safely keep a
 * pointer to it.
 *
 * \param object JSON object.
 * \param key Key of field to look up.
 * \return Value with given \a key.
 * \return \c NULL on error.
 */
struct ast_json *ast_json_object_get(struct ast_json *object, const char *key);

/*!
 * \brief Set a field in a JSON object.
 * \since 12.0.0
 *
 * The object steals the \a value reference; use ast_json_ref() to safely keep a pointer
 * to it.
 *
 * \param object JSON object to modify.
 * \param key Key of field to set.
 * \param value JSON value to set for field.
 * \return 0 on success.
 * \return -1 on error.
 */
int ast_json_object_set(struct ast_json *object, const char *key, struct ast_json *value);

/*!
 * \brief Delete a field from a JSON object.
 * \since 12.0.0
 *
 * \param object JSON object to modify.
 * \param key Key of field to delete.
 * \return 0 on success, or -1 if key does not exist.
 */
int ast_json_object_del(struct ast_json *object, const char *key);

/*!
 * \brief Delete all elements from a JSON object.
 * \since 12.0.0
 * \param object JSON object to clear.
 * \return 0 on success.
 * \return -1 on error.
 */
int ast_json_object_clear(struct ast_json *object);

/*!
 * \brief Update \a object with all of the fields of \a other.
 * \since 12.0.0
 *
 * All of the fields of \a other are copied into \a object, overwriting existing keys.
 * The \a other object is not changed, so ast_json_unref() it when you are done with it.
 *
 * \param object JSON object to modify.
 * \param other JSON object to copy into \a object.
 * \return 0 on success.
 * \return -1 on error.
 */
int ast_json_object_update(struct ast_json *object, struct ast_json *other);

/*!
 * \brief Update existing fields in \a object with the fields of \a other.
 * \since 12.0.0
 *
 * Like ast_json_object_update(), but only existing fields are updated. No new fields
 * will get added. The \a other object is not changed, so ast_json_unref() it when you
 * are done with it.
 *
 * \param object JSON object to modify.
 * \param other JSON object to copy into \a object.
 * \return 0 on success.
 * \return -1 on error.
 */
int ast_json_object_update_existing(struct ast_json *object, struct ast_json *other);

/*!
 * \brief Add new fields to \a object with the fields of \a other.
 * \since 12.0.0
 *
 * Like ast_json_object_update(), but only missing fields are added. No existing fields
 * will be modified. The \a other object is not changed, so ast_json_unref() it when you
 * are done with it.
 *
 * \param object JSON object to modify.
 * \param other JSON object to copy into \a object.
 * \return 0 on success.
 * \return -1 on error.
 */
int ast_json_object_update_missing(struct ast_json *object, struct ast_json *other);

/*!
 * \struct ast_json_iter
 * \brief Iterator for JSON object key/values.
 * \since 12.0.0
 *
 * Note that iteration order is not specified, and may change as fields are added to
 * and removed from the object.
 */
struct ast_json_iter;

/*!
 * \brief Get an iterator pointing to the first field in a JSON object.
 * \since 12.0.0
 *
 * The order of the fields in an object are not specified. However, iterating forward
 * from this iterator will cover all fields in \a object. Adding or removing fields from
 * \a object may invalidate its iterators.
 *
 * \param object JSON object.
 * \return Iterator to the first field in \a object.
 * \return \c NULL \a object is empty.
 * \return \c NULL on error.
 */
struct ast_json_iter *ast_json_object_iter(struct ast_json *object);

/*!
 * \brief Get an iterator pointing to a specified \a key in \a object.
 * \since 12.0.0
 *
 * Iterating forward from this iterator may not to cover all elements in \a object.
 *
 * \param object JSON object to iterate.
 * \param key Key of field to lookup.
 * \return Iterator pointing to the field with the given \a key.
 * \return \c NULL if \a key does not exist.
 * \return \c NULL on error.
 */
struct ast_json_iter *ast_json_object_iter_at(struct ast_json *object, const char *key);

/*!
 * \brief Get the next iterator.
 * \since 12.0.0
 * \param object JSON object \a iter was obtained from.
 * \param iter JSON object iterator.
 * \return Iterator to next field in \a object.
 * \return \c NULL if \a iter was the last field.
 */
struct ast_json_iter *ast_json_object_iter_next(struct ast_json *object, struct ast_json_iter *iter);

/*!
 * \brief Get the key from an iterator.
 * \since 12.0.0
 * \param iter JSON object iterator.
 * \return Key of the field \a iter points to.
 */
const char *ast_json_object_iter_key(struct ast_json_iter *iter);

/*!
 * \brief Get the value from an iterator.
 * \since 12.0.0
 *
 * The returned element is a borrowed reference; use ast_json_ref() to safely
 * keep a pointer to it.
 *
 * \param iter JSON object iterator.
 * \return Value of the field \a iter points to.
 */
struct ast_json *ast_json_object_iter_value(struct ast_json_iter *iter);

/*!
 * \brief Set the value of the field pointed to by an iterator.
 * \since 12.0.0
 *
 * The array steals the value reference; use ast_json_ref() to safely keep a
 * pointer to it.
 *
 * \param object JSON object \a iter was obtained from.
 * \param iter JSON object iterator.
 * \param value JSON value to store in \iter's field.
 * \return 0 on success.
 * \return -1 on error.
 */
int ast_json_object_iter_set(struct ast_json *object, struct ast_json_iter *iter, struct ast_json *value);

/*!@}*/

/*!@{*/

/*!
 * \brief Encode a JSON value to a string.
 * \since 12.0.0
 *
 * Returned string must be freed by calling ast_free().
 *
 * \param JSON value.
 * \return String encoding of \a root.
 * \return \c NULL on error.
 */
char *ast_json_dump_string(struct ast_json *root);

/*!
 * \brief Encode a JSON value to an \ref ast_str.
 * \since 12.0.0
 *
 * If \a dst is too small, it will be grown as needed.
 *
 * \param root JSON value.
 * \param dst \ref ast_str to store JSON encoding.
 * \return 0 on success.
 * \return -1 on error. The contents of \a dst are undefined.
 */
int ast_json_dump_str(struct ast_json *root, struct ast_str **dst);

/*!
 * \brief Encode a JSON value to a \c FILE.
 * \since 12.0.0
 *
 * \param root JSON value.
 * \param output File to write JSON encoding to.
 * \return 0 on success.
 * \return -1 on error. The contents of \a output are undefined.
 */
int ast_json_dump_file(struct ast_json *root, FILE *output);

/*!
 * \brief Encode a JSON value to a file at the given location.
 * \since 12.0.0
 *
 * \param root JSON value.
 * \param path Path to file to write JSON encoding to.
 * \return 0 on success.
 * \return -1 on error. The contents of \a output are undefined.
 */
int ast_json_dump_new_file(struct ast_json *root, const char *path);

#define AST_JSON_ERROR_TEXT_LENGTH    160
#define AST_JSON_ERROR_SOURCE_LENGTH   80

/*!
 * \brief JSON parsing error information.
 * \since 12.0.0
 */
struct ast_json_error {
	/*! Line number error occured on */
	int line;
	/*! Character (not byte, can be different for UTF-8) column on which the error occurred. */
	int column;
	/*! Position in bytes from start of input */
	int position;
	/*! Error message */
	char text[AST_JSON_ERROR_TEXT_LENGTH];
	/*! Source of the error (filename or <string>) */
	char source[AST_JSON_ERROR_TEXT_LENGTH];
};

/*!
 * \brief Parse null terminated string into a JSON object or array.
 * \since 12.0.0
 * \param input String to parse.
 * \param[out] error Filled with information on error.
 * \return Parsed JSON element.
 * \return \c NULL on error.
 */
struct ast_json *ast_json_load_string(const char *input, struct ast_json_error *error);

/*!
 * \brief Parse \ref ast_str into a JSON object or array.
 * \since 12.0.0
 * \param input \ref ast_str to parse.
 * \param[out] error Filled with information on error.
 * \return Parsed JSON element.
 * \return \c NULL on error.
 */
struct ast_json *ast_json_load_str(const struct ast_str *input, struct ast_json_error *error);

/*!
 * \brief Parse buffer with known length into a JSON object or array.
 * \since 12.0.0
 * \param buffer Buffer to parse.
 * \param buflen Length of \a buffer.
 * \param[out] error Filled with information on error.
 * \return Parsed JSON element.
 * \return \c NULL on error.
 */
struct ast_json *ast_json_load_buf(const char *buffer, size_t buflen, struct ast_json_error *error);

/*!
 * \brief Parse a \c FILE into JSON object or array.
 * \since 12.0.0
 * \param input \c FILE to parse.
 * \param[out] error Filled with information on error.
 * \return Parsed JSON element.
 * \return \c NULL on error.
 */
struct ast_json *ast_json_load_file(FILE *input, struct ast_json_error *error);

/*!
 * \brief Parse file at \a path into JSON object or array.
 * \since 12.0.0
 * \param path Path of file to parse.
 * \param[out] error Filled with information on error.
 * \return Parsed JSON element.
 * \return \c NULL on error.
 */
struct ast_json *ast_json_load_new_file(const char *path, struct ast_json_error *error);

/*!
 * \brief Helper for creating complex JSON values.
 * \since 12.0.0
 *
 * See original Jansson docs at http://www.digip.org/jansson/doc/2.4/apiref.html#apiref-pack
 * for more details.
 */
struct ast_json *ast_json_pack(char const *format, ...);

/*!
 * \brief Helper for creating complex JSON values simply.
 * \since 12.0.0
 *
 * See original Jansson docs at http://www.digip.org/jansson/doc/2.4/apiref.html#apiref-pack
 * for more details.
 */
struct ast_json *ast_json_vpack(char const *format, va_list ap);

/*!@}*/

/*!@{*/

/*!
 * \brief Compare two JSON objects.
 * \since 12.0.0
 *
 * Two JSON objects are equal if they are of the same type, and their contents are equal.
 *
 * \param lhs Value to compare.
 * \param rhs Other value to compare.
 * \return True (non-zero) if \a lhs and \a rhs are equal.
 * \return False (zero) if they are not.
 */
int ast_json_equal(const struct ast_json *lhs, const struct ast_json *rhs);

/*!
 * \brief Copy a JSON value, but not its children.
 * \since 12.0.0
 *
 * If \a value is a JSON object or array, its children are shared with the returned copy.
 *
 * \param value JSON value to copy.
 * \return Shallow copy of \a value.
 * \return \c NULL on error.
 */
struct ast_json *ast_json_copy(const struct ast_json *value);

/*!
 * \brief Copy a JSON value, and its children.
 * \since 12.0.0
 *
 * If \a value is a JSON object or array, they are also copied.
 *
 * \param value JSON value to copy.
 * \return Deep copy of \a value.
 * \return \c NULL on error.
 */
struct ast_json *ast_json_deep_copy(const struct ast_json *value);

/*!@}*/

#endif /* _ASTERISK_JSON_H */
