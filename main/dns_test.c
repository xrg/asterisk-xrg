/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2015, Digium, Inc.
 *
 * Mark Michelson <mmichelson@digium.com>
 *
 * Includes code and algorithms from the Zapata library.
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

/*** MODULEINFO
	<support_level>core</support_level>
 ***/

#include "asterisk.h"
#include "asterisk/dns_core.h"
#include "asterisk/dns_test.h"
#include "asterisk/utils.h"

#ifdef TEST_FRAMEWORK

const char DNS_HEADER[] = {
	/* ID  == 0 */
	0x00, 0x00,
	/* QR == 1, Opcode == 0, AA == 1, TC == 0, RD == 1 */
	0x85,
	/* RA == 1, Z == 0, RCODE == 0 */
	0x80,
	/* QDCOUNT == 1 */
	0x00, 0x01,
	/* ANCOUNT == 1 */
	0x00, 0x00,
	/* NSCOUNT == 0 */
	0x00, 0x00,
	/* ARCOUNT == 0 */
	0x00, 0x00,
};

/*!
 * \brief Generate a DNS header and write it to a buffer
 *
 * The DNS header is the first part of a DNS request or response. In our
 * case, the only part of the header that a test can affect is the number
 * of answers. The rest of the DNS header is based on hard-coded values.
 *
 * There is no buffer size passed to this function since we provide
 * the data ourselves and have sized the buffer to be way larger
 * than necessary for the tests.
 *
 * \param num_records The number of DNS records in this DNS response
 * \param buf The buffer to write the header into
 * \retval The number of bytes written to the buffer
 */
static int generate_dns_header(unsigned short num_records, char *buf)
{
	unsigned short net_num_records = htons(num_records);

	memcpy(buf, DNS_HEADER, ARRAY_LEN(DNS_HEADER));
	/* Overwrite the ANCOUNT with the actual number of answers */
	memcpy(&buf[6], &net_num_records, sizeof(num_records));

	return ARRAY_LEN(DNS_HEADER);
}

const char DNS_QUESTION [] = {
	/* goose */
	0x05, 0x67, 0x6f, 0x6f, 0x73, 0x65,
	/* feathers */
	0x08, 0x66, 0x65, 0x61, 0x74, 0x68, 0x65, 0x72, 0x73,
	/* end label */
	0x00,
	/* NAPTR type */
	0x00, 0x23,
	/* IN class */
	0x00, 0x01,
};

/*!
 * \brief Generate a DNS question and write it to a buffer
 *
 * The DNS question is the second part of a DNS request or response.
 * All DNS questions in this file are for the same domain and thus
 * the DNS question is a hard-coded value.
 *
 * There is no buffer size passed to this function since we provide
 * the data ourselves and have sized the buffer to be way larger
 * than necessary for the tests.
 *
 * \param buf The buffer to write the question into
 * \retval The number of bytes written to the buffer
 */
static int generate_dns_question(char *buf)
{
	memcpy(buf, DNS_QUESTION, ARRAY_LEN(DNS_QUESTION));
	return ARRAY_LEN(DNS_QUESTION);
}

const char NAPTR_ANSWER [] = {
	/* Domain points to name from question */
	0xc0, 0x0c,
	/* NAPTR type */
	0x00, 0x23,
	/* IN Class */
	0x00, 0x01,
	/* TTL (12345 by default) */
	0x00, 0x00, 0x30, 0x39,
};

/*!
 * \brief Generate a DNS answer and write it to a buffer
 *
 * The DNS answer is the third (and in our case final) part of a
 * DNS response. The DNS answer generated here is only partial.
 * The record-specific data is generated by a separate function.
 * DNS answers in our tests may have variable TTLs, but the rest
 * is hard-coded.
 *
 * There is no buffer size passed to this function since we provide
 * the data ourselves and have sized the buffer to be way larger
 * than necessary for the tests.
 *
 * \param buf The buffer to write the answer into
 * \retval The number of bytes written to the buffer
 */
static int generate_dns_answer(int ttl, char *buf)
{
	int net_ttl = htonl(ttl);

	memcpy(buf, NAPTR_ANSWER, ARRAY_LEN(NAPTR_ANSWER));
	/* Overwrite TTL if one is provided */
	if (ttl) {
		memcpy(&buf[6], &net_ttl, sizeof(int));
	}

	return ARRAY_LEN(NAPTR_ANSWER);
}

/*!
 * \brief Write a DNS string to a buffer
 *
 * This writes the DNS string to the buffer and returns the total
 * number of bytes written to the buffer.
 *
 * There is no buffer size passed to this function since we provide
 * the data ourselves and have sized the buffer to be way larger
 * than necessary for the tests.
 *
 * \param string The string to write
 * \param buf The buffer to write the string into
 * \return The number of bytes written to the buffer
 */
int ast_dns_test_write_string(const struct ast_dns_test_string *string, char *buf)
{
	uint8_t len = string->len;
	size_t actual_len = strlen(string->val);
	buf[0] = len;
	/*
	 * We use the actual length of the string instead of
	 * the stated value since sometimes we're going to lie about
	 * the length of the string
	 */
	if (actual_len) {
		memcpy(&buf[1], string->val, strlen(string->val));
	}

	return actual_len + 1;
}

/*!
 * \brief Write a DNS domain to a buffer
 *
 * A DNS domain consists of a series of labels separated
 * by dots. Each of these labels gets written as a DNS
 * string. A DNS domain ends with a NULL label, which is
 * essentially a zero-length DNS string.
 *
 *
 * There is no buffer size passed to this function since we provide
 * the data ourselves and have sized the buffer to be way larger
 * than necessary for the tests.
 *
 * \param string The DNS domain to write
 * \param buf The buffer to write the domain into
 * \return The number of bytes written to the buffer
 */
int ast_dns_test_write_domain(const char *string, char *buf)
{
	char *copy = ast_strdupa(string);
	char *part;
	char *ptr = buf;
	static const struct ast_dns_test_string null_label = {
		.len = 0,
		.val = "",
	};

	while (1) {
		struct ast_dns_test_string dns_str;
		part = strsep(&copy, ".");
		if (ast_strlen_zero(part)) {
			break;
		}
		dns_str.len = strlen(part);
		dns_str.val = part;

		ptr += ast_dns_test_write_string(&dns_str, ptr);
	}
	ptr += ast_dns_test_write_string(&null_label, ptr);

	return ptr - buf;
}

int ast_dns_test_generate_result(struct ast_dns_query *query, void *records, size_t num_records,
		size_t record_size, record_fn generate, char *buffer)
{
	char *ptr = buffer;
	char *record_iter;

	ptr += generate_dns_header(num_records, ptr);
	ptr += generate_dns_question(ptr);

	for (record_iter = records; record_iter < (char *) records + num_records * record_size; record_iter += record_size) {
		unsigned short rdlength;
		unsigned short net_rdlength;

		/* XXX Do we even want to override TTL? */
		ptr += generate_dns_answer(0, ptr);
		rdlength = generate(record_iter, ptr + 2);
		net_rdlength = htons(rdlength);
		memcpy(ptr, &net_rdlength, 2);
		ptr += 2;
		ptr += rdlength;
	}

	return ptr - buffer;
}

#else /* TEST_FRAMEWORK */

int ast_dns_test_write_string(struct ast_dns_test_string *string, char *buf)
{
	return 0;
}

int ast_dns_test_write_domain(const char *string, char *buf)
{
	return 0;
}

int ast_dns_test_generate_result(struct ast_dns_query *query, void *records, size_t num_records,
		size_t record_size, record_fn generate, char *buffer)
{
	return 0;
}

#endif
