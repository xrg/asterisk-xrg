/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2010, Digium, Inc.
 *
 * Tilghman Lesher <tlesher AT digium DOT com>
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

/*!
 * \file
 * \author Tilghman Lesher \verbatim <tlesher AT digium DOT com> \endverbatim
 *
 * \brief kqueue timing interface
 *
 * \ingroup resource
 */

/*** MODULEINFO
	<depend>kqueue</depend>
	<conflict>launchd</conflict>
	<support_level>extended</support_level>
 ***/

#include "asterisk.h"

#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

#include "asterisk/module.h"
#include "asterisk/astobj2.h"
#include "asterisk/timing.h"
#include "asterisk/logger.h"
#include "asterisk/utils.h"
#include "asterisk/time.h"
#include "asterisk/test.h"
#include "asterisk/poll-compat.h"       /* for ast_poll() */

static void *timing_funcs_handle;

static void *kqueue_timer_open(void);
static void kqueue_timer_close(void *data);
static int kqueue_timer_set_rate(void *data, unsigned int rate);
static int kqueue_timer_ack(void *data, unsigned int quantity);
static int kqueue_timer_enable_continuous(void *data);
static int kqueue_timer_disable_continuous(void *data);
static enum ast_timer_event kqueue_timer_get_event(void *data);
static unsigned int kqueue_timer_get_max_rate(void *data);
static int kqueue_timer_fd(void *data);

static struct ast_timing_interface kqueue_timing = {
	.name = "kqueue",
	.priority = 150,
	.timer_open = kqueue_timer_open,
	.timer_close = kqueue_timer_close,
	.timer_set_rate = kqueue_timer_set_rate,
	.timer_ack = kqueue_timer_ack,
	.timer_enable_continuous = kqueue_timer_enable_continuous,
	.timer_disable_continuous = kqueue_timer_disable_continuous,
	.timer_get_event = kqueue_timer_get_event,
	.timer_get_max_rate = kqueue_timer_get_max_rate,
	.timer_fd = kqueue_timer_fd,
};

struct kqueue_timer {
	int handle;
	uint64_t nsecs;
	uint64_t unacked;
	unsigned int is_continuous:1;
};

static void timer_destroy(void *obj)
{
	struct kqueue_timer *timer = obj;
	close(timer->handle);
}

static void *kqueue_timer_open(void)
{
	struct kqueue_timer *timer;

	if (!(timer = ao2_alloc(sizeof(*timer), timer_destroy))) {
		ast_log(LOG_ERROR, "Could not allocate memory for kqueue_timer structure\n");
		return -1;
	}
	if ((timer->handle = kqueue()) < 0) {
		ast_log(LOG_ERROR, "Failed to create kqueue timer: %s\n", strerror(errno));
		ao2_ref(timer, -1);
		return -1;
	}

	return timer;
}

static void kqueue_timer_close(void *data)
{
	struct kqueue_timer *timer = data;

	ao2_ref(timer, -1);
}

static void kqueue_set_nsecs(struct kqueue_timer *our_timer, uint64_t nsecs)
{
	struct timespec nowait = { 0, 1 };
#ifdef HAVE_KEVENT64
	struct kevent64_s kev;

	EV_SET64(&kev, our_timer->handle, EVFILT_TIMER, EV_ADD | EV_ENABLE, NOTE_NSECONDS,
		nsecs, 0, 0, 0);
	kevent64(our_timer->handle, &kev, 1, NULL, 0, 0, &nowait);
#else
	struct kevent kev;

	EV_SET(&kev, our_timer->handle, EVFILT_TIMER, EV_ADD | EV_ENABLE,
#ifdef NOTE_NSECONDS
		nsecs <= 0xFFffFFff ? NOTE_NSECONDS :
#endif
#ifdef NOTE_USECONDS
		NOTE_USECONDS
#else /* Milliseconds, if no constants are defined */
		0
#endif
		,
#ifdef NOTE_NSECONDS
		nsecs <= 0xFFffFFff ? nsecs :
#endif
#ifdef NOTE_USECONDS
	nsecs / 1000
#else /* Milliseconds, if nothing else is defined */
	nsecs / 1000000
#endif
	, NULL);
	kevent(our_timer->handle, &kev, 1, NULL, 0, &nowait);
#endif
}

static int kqueue_timer_set_rate(void *data, unsigned int rate)
{
	struct kqueue_timer *timer = data;

	kqueue_set_nsecs(timer, (timer->nsecs = rate ? (long) (1000000000 / rate) : 0L));

	return 0;
}

static int kqueue_timer_ack(void *data, unsigned int quantity)
{
	struct kqueue_timer *timer = data;

	if (timer->unacked < quantity) {
		ast_debug(1, "Acking more events than have expired?!!\n");
		timer->unacked = 0;
		return -1;
	} else {
		timer->unacked -= quantity;
	}

	return 0;
}

static int kqueue_timer_enable_continuous(void *data)
{
	struct kqueue_timer *timer = data;

	kqueue_set_nsecs(timer, 1);
	timer->is_continuous = 1;
	timer->unacked = 0;

	return 0;
}

static int kqueue_timer_disable_continuous(void *data)
{
	struct kqueue_timer *timer = data;

	kqueue_set_nsecs(timer, timer->nsecs);
	timer->is_continuous = 0;
	timer->unacked = 0;

	return 0;
}

static enum ast_timer_event kqueue_timer_get_event(void *data)
{
	struct kqueue_timer *timer = data;
	enum ast_timer_event res = -1;
	struct timespec sixty_seconds = { 60, 0 };
	struct kevent kev;

	/* If we have non-ACKed events, just return immediately */
	if (timer->unacked == 0) {
		if (kevent(timer->handle, NULL, 0, &kev, 1, &sixty_seconds) > 0) {
			timer->unacked += kev.data;
		}
	}

	if (timer->unacked > 0) {
		res = timer->is_continuous ? AST_TIMING_EVENT_CONTINUOUS : AST_TIMING_EVENT_EXPIRED;
	}

	return res;
}

static unsigned int kqueue_timer_get_max_rate(void *data)
{
	/* Actually, the max rate is 2^64-1 seconds, but that's not representable in a 32-bit integer. */
	return UINT_MAX;
}

static int kqueue_timer_fd(void *data)
{
	struct kqueue_timer *timer = data;

	return timer->handle;
}

#ifdef TEST_FRAMEWORK
AST_TEST_DEFINE(test_kqueue_timing)
{
	int res = AST_TEST_PASS, i;
	uint64_t diff;
	struct pollfd pfd = { 0, POLLIN, 0 };
	struct kqueue_timer *kt;
	struct timeval start;

	switch (cmd) {
	case TEST_INIT:
		info->name = "test_kqueue_timing";
		info->category = "/res/res_timing_kqueue/";
		info->summary = "Test KQueue timing interface";
		info->description = "Verify that the KQueue timing interface correctly generates timing events";
		return AST_TEST_NOT_RUN;
	case TEST_EXECUTE:
		break;
	}

	if (!(kt = kqueue_timer_open())) {
		ast_test_status_update(test, "Cannot open timer!\n");
		return AST_TEST_FAIL;
	}

	do {
		pfd.fd = ast_timer_fd(kt);
		if (kqueue_timer_set_rate(kt, 1000)) {
			ast_test_status_update(test, "Cannot set timer rate to 1000/s\n");
			res = AST_TEST_FAIL;
			break;
		}
		if (ast_poll(&pfd, 1, 1000) < 1) {
			ast_test_status_update(test, "Polling on a kqueue doesn't work\n");
			res = AST_TEST_FAIL;
			break;
		}
		if (pfd.revents != POLLIN) {
			ast_test_status_update(test, "poll() should have returned POLLIN, but instead returned %hd\n", pfd.revents);
			res = AST_TEST_FAIL;
			break;
		}
		if (kqueue_timer_get_event(kt) <= 0) {
			ast_test_status_update(test, "No events generated after a poll returned successfully?!!\n");
			res = AST_TEST_FAIL;
			break;
		}
#if 0
		if (kt->unacked == 0) {
			ast_test_status_update(test, "Unacked events is 0, but there should be at least 1.\n");
			res = AST_TEST_FAIL;
			break;
		}
#endif
		kqueue_timer_enable_continuous(kt);
		start = ast_tvnow();
		for (i = 0; i < 100; i++) {
			if (ast_poll(&pfd, 1, 1000) < 1) {
				ast_test_status_update(test, "Polling on a kqueue doesn't work\n");
				res = AST_TEST_FAIL;
				break;
			}
			if (kqueue_timer_get_event(kt) <= 0) {
				ast_test_status_update(test, "No events generated in continuous mode after 1 microsecond?!!\n");
				res = AST_TEST_FAIL;
				break;
			}
		}
		diff = ast_tvdiff_us(ast_tvnow(), start);
		ast_test_status_update(test, "diff is %llu\n", diff);
		/*
		if (abs(diff - kt->unacked) == 0) {
			ast_test_status_update(test, "Unacked events should be around 1000, not %llu\n", kt->unacked);
			res = AST_TEST_FAIL;
		}
		*/
	} while (0);
	kqueue_timer_close(kt);
	return res;
}
#endif

/*!
 * \brief Load the module
 *
 * Module loading including tests for configuration or dependencies.
 * This function can return AST_MODULE_LOAD_FAILURE, AST_MODULE_LOAD_DECLINE,
 * or AST_MODULE_LOAD_SUCCESS. If a dependency or environment variable fails
 * tests return AST_MODULE_LOAD_FAILURE. If the module can not load the 
 * configuration file or other non-critical problem return 
 * AST_MODULE_LOAD_DECLINE. On success return AST_MODULE_LOAD_SUCCESS.
 */
static int load_module(void)
{
	if (!(timing_funcs_handle = ast_register_timing_interface(&kqueue_timing))) {
		return AST_MODULE_LOAD_DECLINE;
	}

	AST_TEST_REGISTER(test_kqueue_timing);
	return AST_MODULE_LOAD_SUCCESS;
}

static int unload_module(void)
{
	AST_TEST_UNREGISTER(test_kqueue_timing);

	return ast_unregister_timing_interface(timing_funcs_handle);
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_LOAD_ORDER, "KQueue Timing Interface",
		.load = load_module,
		.unload = unload_module,
		.load_pri = AST_MODPRI_CHANNEL_DEPEND,
		);
