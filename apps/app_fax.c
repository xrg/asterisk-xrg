/*
 * Asterisk -- A telephony toolkit for Linux.
 *
 * Simple fax applications
 * 
 * 2007-2008, Dmitry Andrianov <asterisk@dima.spb.ru>
 *
 * Initial T.38 gateway code
 *
 * 2008, Daniel Ferenci <daniel.ferenci@nethemba.com>
 * Created by Nethemba s.r.o. http://www.nethemba.com
 * Sponsored by IPEX a.s. http://www.ipex.cz
 * Code based on original implementation by Steve Underwood <steveu@coppice.org>
 *
 * T.38 Integration into asterisk app_fax and rework
 *
 * 2008, Gregory Hinton Nietsky <gregory@dnstelecom.co.za>
 * dns Telecom http://www.dnstelecom.co.za
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License
 *
 */

/*** MODULEINFO
	 <depend>spandsp</depend>
***/
 
#include "asterisk.h"

ASTERISK_FILE_VERSION(__FILE__, "$Revision$")

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <pthread.h>
#include <errno.h>
#include <tiffio.h>

#include <spandsp.h>
#ifdef HAVE_SPANDSP_EXPOSE_H
#include <spandsp/expose.h>
#endif
#include <spandsp/version.h>

#include "asterisk/lock.h"
#include "asterisk/file.h"
#include "asterisk/logger.h"
#include "asterisk/channel.h"
#include "asterisk/pbx.h"
#include "asterisk/app.h"
#include "asterisk/dsp.h"
#include "asterisk/module.h"
#include "asterisk/manager.h"
#include "asterisk/monitor.h"

/*** DOCUMENTATION
	<application name="SendFAX" language="en_US">
		<synopsis>
			Send a Fax
		</synopsis>
		<syntax>
			<parameter name="filename" required="true">
				<para>Filename of TIFF file to fax</para>
			</parameter>
			<parameter name="a" required="false">
				<para>Makes the application behave as the answering machine</para>
				<para>(Default behavior is as calling machine)</para>
			</parameter>
		</syntax>
		<description>
			<para>Send a given TIFF file to the channel as a FAX.</para>
			<para>This application sets the following channel variables:</para>
			<variablelist>
				<variable name="LOCALSTATIONID">
					<para>To identify itself to the remote end</para>
				</variable>
				<variable name="LOCALHEADERINFO">
					<para>To generate a header line on each page</para>
				</variable>
				<variable name="FAXSTATUS">
					<value name="SUCCESS"/>
					<value name="FAILED"/>
				</variable>
				<variable name="FAXERROR">
					<para>Cause of failure</para>
				</variable>
				<variable name="REMOTESTATIONID">
					<para>The CSID of the remote side</para>
				</variable>
				<variable name="FAXPAGES">
					<para>Number of pages sent</para>
				</variable>
				<variable name="FAXBITRATE">
					<para>Transmission rate</para>
				</variable>
				<variable name="FAXRESOLUTION">
					<para>Resolution of sent fax</para>
				</variable>
			</variablelist>
		</description>
	</application>
	<application name="ReceiveFAX" language="en_US">
		<synopsis>
			Receive a Fax
		</synopsis>
		<syntax>
			<parameter name="filename" required="true">
				<para>Filename of TIFF file save incoming fax</para>
			</parameter>
			<parameter name="c" required="false">
				<para>Makes the application behave as the calling machine</para> 
				<para>(Default behavior is as answering machine)</para>
			</parameter>
		</syntax>
		<description>
			<para>Receives a FAX from the channel into the given filename 
			overwriting the file if it already exists.</para>
			<para>File created will be in TIFF format.</para>

			<para>This application sets the following channel variables:</para>
			<variablelist>
				<variable name="LOCALSTATIONID">
					<para>To identify itself to the remote end</para>
				</variable>
				<variable name="LOCALHEADERINFO">
					<para>To generate a header line on each page</para>
				</variable>
				<variable name="FAXSTATUS">
					<value name="SUCCESS"/>
					<value name="FAILED"/>
				</variable>
				<variable name="FAXERROR">
					<para>Cause of failure</para>
				</variable>
				<variable name="REMOTESTATIONID">
					<para>The CSID of the remote side</para>
				</variable>
				<variable name="FAXPAGES">
					<para>Number of pages sent</para>
				</variable>
				<variable name="FAXBITRATE">
					<para>Transmission rate</para>
				</variable>
				<variable name="FAXRESOLUTION">
					<para>Resolution of sent fax</para>
				</variable>
			</variablelist>
		</description>
	</application>

 ***/

static char *app_sndfax_name = "SendFAX";
static char *app_rcvfax_name = "ReceiveFAX";

static char *app_t38gateway_name = "FaxGateway";
static char *app_t38gateway_synopsis = "T38 FAX Gateway";
static char *app_t38gateway_desc = 
"  FaxGateway(technology/dialstring[, timeout]):\n"
"\n"
"Create a channel, connect to the dialstring and negotiate T.38 capabilities.\n"
"If required  T.30 <-> T.38 gateway mode is enabled.\n"
"\n"
"The default timeout (seconds) for answer is 35s as per T.30 specification\n"
"\n"
"This application sets the following channel variables upon completion:\n"
"     FAXSTATUS - status of operation:\n"
"		  SUCCESS | FAILED\n"
"     FAXERROR	- Error when FAILED\n"
"\n"
"Returns -1 in case of user hang up or any channel error.\n"
"Returns 0 on success.\n";

#define MAX_SAMPLES 240

/* Watchdog. I have seen situations when remote fax disconnects (because of poor line
   quality) while SpanDSP continues staying in T30_STATE_IV_CTC state forever.
   To avoid this, we terminate when we see that T30 state does not change for 5 minutes.
   We also terminate application when more than 30 minutes passed regardless of
   state changes. This is just a precaution measure - no fax should take that long */

#define WATCHDOG_TOTAL_TIMEOUT	30 * 60
#define WATCHDOG_STATE_TIMEOUT	5 * 60

typedef struct {
	struct ast_channel *chan;
	struct ast_channel *peer;
	enum ast_t38_state t38state;	/* T38 state of the channel */
	int direction;			/* Fax direction: 0 - receiving, 1 - sending */
	int caller_mode;
	char *file_name;
	struct ast_control_t38_parameters t38parameters;
	volatile int finished;
} fax_session;

static void span_message(int level, const char *msg)
{
	if (level == SPAN_LOG_ERROR) {
		ast_log(LOG_ERROR, "%s", msg);
	} else if (level == SPAN_LOG_WARNING) {
		ast_log(LOG_WARNING, "%s", msg);
	} else {
		ast_log(LOG_DEBUG, "%s", msg);
	}
}

static int t38_tx_packet_handler(t38_core_state_t *s, void *user_data, const uint8_t *buf, int len, int count)
{
	struct ast_channel *chan = (struct ast_channel *) user_data;

	struct ast_frame outf = {
		.frametype = AST_FRAME_MODEM,
		.subclass = AST_MODEM_T38,
		.src = __FUNCTION__,
	};

	/* TODO: Asterisk does not provide means of resending the same packet multiple
	  times so count is ignored at the moment */

	AST_FRAME_SET_BUFFER(&outf, buf, 0, len);

	if (ast_write(chan, &outf) < 0) {
		ast_log(LOG_WARNING, "Unable to write frame to channel; %s\n", strerror(errno));
		return -1;
	}

	return 0;
}

static void phase_e_handler(t30_state_t *f, void *user_data, int result)
{
	const char *local_ident;
	const char *far_ident;
	char buf[20];
	fax_session *s = (fax_session *) user_data;
	t30_stats_t stat;
	int pages_transferred;

	ast_debug(1, "Fax phase E handler. result=%d\n", result);

	t30_get_transfer_statistics(f, &stat);

	s = (fax_session *) user_data;

	if (result != T30_ERR_OK) {
		s->finished = -1;

		/* FAXSTATUS is already set to FAILED */
		pbx_builtin_setvar_helper(s->chan, "FAXERROR", t30_completion_code_to_str(result));

		ast_log(LOG_WARNING, "Error transmitting fax. result=%d: %s.\n", result, t30_completion_code_to_str(result));

		return;
	}
	
	s->finished = 1; 
	
	local_ident = t30_get_tx_ident(f);
	far_ident = t30_get_rx_ident(f);
	pbx_builtin_setvar_helper(s->chan, "FAXSTATUS", "SUCCESS"); 
	pbx_builtin_setvar_helper(s->chan, "FAXERROR", NULL); 
	pbx_builtin_setvar_helper(s->chan, "REMOTESTATIONID", far_ident);
#if SPANDSP_RELEASE_DATE >= 20090220
	pages_transferred = (s->direction) ? stat.pages_tx : stat.pages_rx;
#else
	pages_transferred = stat.pages_transferred;
#endif
	snprintf(buf, sizeof(buf), "%d", pages_transferred);
	pbx_builtin_setvar_helper(s->chan, "FAXPAGES", buf);
	snprintf(buf, sizeof(buf), "%d", stat.y_resolution);
	pbx_builtin_setvar_helper(s->chan, "FAXRESOLUTION", buf);
	snprintf(buf, sizeof(buf), "%d", stat.bit_rate);
	pbx_builtin_setvar_helper(s->chan, "FAXBITRATE", buf); 
	
	ast_debug(1, "Fax transmitted successfully.\n");
	ast_debug(1, "  Remote station ID: %s\n", far_ident);
	ast_debug(1, "  Pages transferred: %d\n", pages_transferred);
	ast_debug(1, "  Image resolution:  %d x %d\n", stat.x_resolution, stat.y_resolution);
	ast_debug(1, "  Transfer Rate:     %d\n", stat.bit_rate);
	
	manager_event(EVENT_FLAG_CALL,
		      s->direction ? "FaxSent" : "FaxReceived", 
		      "Channel: %s\r\n"
		      "Exten: %s\r\n"
		      "CallerID: %s\r\n"
		      "RemoteStationID: %s\r\n"
		      "LocalStationID: %s\r\n"
		      "PagesTransferred: %d\r\n"
		      "Resolution: %d\r\n"
		      "TransferRate: %d\r\n"
		      "FileName: %s\r\n",
		      s->chan->name,
		      s->chan->exten,
		      S_OR(s->chan->cid.cid_num, ""),
		      far_ident,
		      local_ident,
		      pages_transferred,
		      stat.y_resolution,
		      stat.bit_rate,
		      s->file_name);
}

/* === Helper functions to configure fax === */

/* Setup SPAN logging according to Asterisk debug level */
static int set_logging(logging_state_t *state)
{
	int level = SPAN_LOG_WARNING + option_debug;

	span_log_set_message_handler(state, span_message);
	span_log_set_level(state, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | level); 

	return 0;
}

static void set_local_info(t30_state_t *state, fax_session *s)
{
	const char *x;

	x = pbx_builtin_getvar_helper(s->chan, "LOCALSTATIONID");
	if (!ast_strlen_zero(x))
		t30_set_tx_ident(state, x);

	x = pbx_builtin_getvar_helper(s->chan, "LOCALHEADERINFO");
	if (!ast_strlen_zero(x))
		t30_set_tx_page_header_info(state, x);
}

static void set_file(t30_state_t *state, fax_session *s)
{
	if (s->direction)
		t30_set_tx_file(state, s->file_name, -1, -1);
	else
		t30_set_rx_file(state, s->file_name, -1);
}

static void set_ecm(t30_state_t *state, int ecm)
{
	t30_set_ecm_capability(state, ecm);
	t30_set_supported_compressions(state, T30_SUPPORT_T4_1D_COMPRESSION | T30_SUPPORT_T4_2D_COMPRESSION | T30_SUPPORT_T6_COMPRESSION);
}

/* === Generator === */

/* This function is only needed to return passed params so
   generator_activate will save it to channel's generatordata */
static void *fax_generator_alloc(struct ast_channel *chan, void *params)
{
	return params;
}

static int fax_generator_generate(struct ast_channel *chan, void *data, int len, int samples)
{
	fax_state_t *fax = (fax_state_t*) data;
	uint8_t buffer[AST_FRIENDLY_OFFSET + MAX_SAMPLES * sizeof(uint16_t)];
	int16_t *buf = (int16_t *) (buffer + AST_FRIENDLY_OFFSET);
    
	struct ast_frame outf = {
		.frametype = AST_FRAME_VOICE,
		.subclass = AST_FORMAT_SLINEAR,
		.src = __FUNCTION__,
	};

	if (samples > MAX_SAMPLES) {
		ast_log(LOG_WARNING, "Only generating %d samples, where %d requested\n", MAX_SAMPLES, samples);
		samples = MAX_SAMPLES;
	}
	
	if ((len = fax_tx(fax, buf, samples)) > 0) {
		outf.samples = len;
		AST_FRAME_SET_BUFFER(&outf, buffer, AST_FRIENDLY_OFFSET, len * sizeof(int16_t));

		if (ast_write(chan, &outf) < 0) {
			ast_log(LOG_WARNING, "Failed to write frame to '%s': %s\n", chan->name, strerror(errno));
			return -1;
		}
	}

	return 0;
}

struct ast_generator generator = {
	alloc:		fax_generator_alloc,
	generate: 	fax_generator_generate,
};


/* === Transmission === */

static int transmit_audio(fax_session *s)
{
	int res = -1;
	int original_read_fmt = AST_FORMAT_SLINEAR;
	int original_write_fmt = AST_FORMAT_SLINEAR;
	fax_state_t fax;
	t30_state_t *t30state;
	struct ast_frame *inf = NULL;
	int last_state = 0;
	struct timeval now, start, state_change;
	enum ast_t38_state t38_state;
	struct ast_control_t38_parameters t38_parameters = { .version = 0,
							     .max_ifp = 800,
							     .rate = AST_T38_RATE_9600,
							     .rate_management = AST_T38_RATE_MANAGEMENT_TRANSFERRED_TCF,
							     .fill_bit_removal = 1,
							     .transcoding_mmr = 1,
							     .transcoding_jbig = 1,
	};

	/* if in receive mode, try to use T.38 */
	if (!s->direction) {
		/* check if we are already in T.38 mode (unlikely), or if we can request
		 * a switch... if so, request it now and wait for the result, rather
		 * than starting an audio FAX session that will have to be cancelled
		 */
		if ((t38_state = ast_channel_get_t38_state(s->chan)) == T38_STATE_NEGOTIATED) {
			return 1;
		} else if ((t38_state != T38_STATE_UNAVAILABLE) &&
			   (t38_parameters.request_response = AST_T38_REQUEST_NEGOTIATE,
			    (ast_indicate_data(s->chan, AST_CONTROL_T38_PARAMETERS, &t38_parameters, sizeof(t38_parameters)) == 0))) {
			/* wait up to five seconds for negotiation to complete */
			unsigned int timeout = 5000;
			int ms;
			
			ast_debug(1, "Negotiating T.38 for receive on %s\n", s->chan->name);
			while (timeout > 0) {
				ms = ast_waitfor(s->chan, 1000);
				if (ms < 0) {
					ast_log(LOG_WARNING, "something bad happened while channel '%s' was polling.\n", s->chan->name);
					return -1;
				}
				if (!ms) {
					/* nothing happened */
					if (timeout > 0) {
						timeout -= 1000;
						continue;
					} else {
						ast_log(LOG_WARNING, "channel '%s' timed-out during the T.38 negotiation.\n", s->chan->name);
						break;
					}
				}
				if (!(inf = ast_read(s->chan))) {
					return -1;
				}
				if ((inf->frametype == AST_FRAME_CONTROL) &&
				    (inf->subclass == AST_CONTROL_T38_PARAMETERS) &&
				    (inf->datalen == sizeof(t38_parameters))) {
					struct ast_control_t38_parameters *parameters = inf->data.ptr;
					
					switch (parameters->request_response) {
					case AST_T38_NEGOTIATED:
						ast_debug(1, "Negotiated T.38 for receive on %s\n", s->chan->name);
						res = 1;
						break;
					case AST_T38_REFUSED:
						ast_log(LOG_WARNING, "channel '%s' refused to negotiate T.38\n", s->chan->name);
						break;
					default:
						ast_log(LOG_ERROR, "channel '%s' failed to negotiate T.38\n", s->chan->name);
						break;
					}
					ast_frfree(inf);
					if (res == 1) {
						return 1;
					} else {
						break;
					}
				}
				ast_frfree(inf);
			}
		}
	}

#if SPANDSP_RELEASE_DATE >= 20080725
        /* for spandsp shaphots 0.0.6 and higher */
        t30state = &fax.t30;
#else
        /* for spandsp release 0.0.5 */
        t30state = &fax.t30_state;
#endif

	original_read_fmt = s->chan->readformat;
	if (original_read_fmt != AST_FORMAT_SLINEAR) {
		res = ast_set_read_format(s->chan, AST_FORMAT_SLINEAR);
		if (res < 0) {
			ast_log(LOG_WARNING, "Unable to set to linear read mode, giving up\n");
			goto done;
		}
	}

	original_write_fmt = s->chan->writeformat;
	if (original_write_fmt != AST_FORMAT_SLINEAR) {
		res = ast_set_write_format(s->chan, AST_FORMAT_SLINEAR);
		if (res < 0) {
			ast_log(LOG_WARNING, "Unable to set to linear write mode, giving up\n");
			goto done;
		}
	}

	/* Initialize T30 terminal */
	fax_init(&fax, s->caller_mode);

	/* Setup logging */
	set_logging(&fax.logging);
	set_logging(&t30state->logging);

	/* Configure terminal */
	set_local_info(t30state, s);
	set_file(t30state, s);
	set_ecm(t30state, TRUE);

	fax_set_transmit_on_idle(&fax, TRUE);

	t30_set_phase_e_handler(t30state, phase_e_handler, s);

	start = state_change = ast_tvnow();

	ast_activate_generator(s->chan, &generator, &fax);

	while (!s->finished) {
		inf = NULL;

		if ((res = ast_waitfor(s->chan, 20)) < 0) {
			break;
		}

		/* if nothing arrived, check the watchdog timers */
		if (res == 0) {
			now = ast_tvnow();
			if (ast_tvdiff_sec(now, start) > WATCHDOG_TOTAL_TIMEOUT || ast_tvdiff_sec(now, state_change) > WATCHDOG_STATE_TIMEOUT) {
				ast_log(LOG_WARNING, "It looks like we hung. Aborting.\n");
				res = -1;
				break;
			} else {
				/* timers have not triggered, loop around to wait
				 * again
				 */
				continue;
			}
		}

		if (!(inf = ast_read(s->chan))) {
			ast_debug(1, "Channel hangup\n");
			res = -1;
			break;
		}

		ast_debug(10, "frame %d/%d, len=%d\n", inf->frametype, inf->subclass, inf->datalen);

		/* Check the frame type. Format also must be checked because there is a chance
		   that a frame in old format was already queued before we set channel format
		   to slinear so it will still be received by ast_read */
		if (inf->frametype == AST_FRAME_VOICE && inf->subclass == AST_FORMAT_SLINEAR) {
			if (fax_rx(&fax, inf->data.ptr, inf->samples) < 0) {
				/* I know fax_rx never returns errors. The check here is for good style only */
				ast_log(LOG_WARNING, "fax_rx returned error\n");
				res = -1;
				break;
			}
			if (last_state != t30state->state) {
				state_change = ast_tvnow();
				last_state = t30state->state;
			}
		} else if ((inf->frametype == AST_FRAME_CONTROL) &&
			   (inf->subclass == AST_CONTROL_T38_PARAMETERS)) {
			struct ast_control_t38_parameters *parameters = inf->data.ptr;

			if (parameters->request_response == AST_T38_NEGOTIATED) {
				/* T38 switchover completed */
				s->t38parameters = *parameters;
				ast_debug(1, "T38 negotiated, finishing audio loop\n");
				res = 1;
				break;
			} else if (parameters->request_response == AST_T38_REQUEST_NEGOTIATE) {
				t38_parameters.request_response = AST_T38_NEGOTIATED;
				ast_debug(1, "T38 request received, accepting\n");
				/* Complete T38 switchover */
				ast_indicate_data(s->chan, AST_CONTROL_T38_PARAMETERS, &t38_parameters, sizeof(t38_parameters));
				/* Do not break audio loop, wait until channel driver finally acks switchover
				 * with AST_T38_NEGOTIATED
				 */
			}
		}

		ast_frfree(inf);
	}

	ast_debug(1, "Loop finished, res=%d\n", res);

	if (inf)
		ast_frfree(inf);

	ast_deactivate_generator(s->chan);

	/* If we are switching to T38, remove phase E handler. Otherwise it will be executed
	   by t30_terminate, display diagnostics and set status variables although no transmittion
	   has taken place yet. */
	if (res > 0) {
		t30_set_phase_e_handler(t30state, NULL, NULL);
	}

	t30_terminate(t30state);
	fax_release(&fax);

done:
	if (original_write_fmt != AST_FORMAT_SLINEAR) {
		if (ast_set_write_format(s->chan, original_write_fmt) < 0)
			ast_log(LOG_WARNING, "Unable to restore write format on '%s'\n", s->chan->name);
	}

	if (original_read_fmt != AST_FORMAT_SLINEAR) {
		if (ast_set_read_format(s->chan, original_read_fmt) < 0)
			ast_log(LOG_WARNING, "Unable to restore read format on '%s'\n", s->chan->name);
	}

	return res;

}

static int transmit_t38(fax_session *s)
{
	int res = 0;
	t38_terminal_state_t t38;
	struct ast_frame *inf = NULL;
	int last_state = 0;
	struct timeval now, start, state_change, last_frame;
	t30_state_t *t30state;
	t38_core_state_t *t38state;

#if SPANDSP_RELEASE_DATE >= 20080725
	/* for spandsp shaphots 0.0.6 and higher */
	t30state = &t38.t30;
	t38state = &t38.t38_fe.t38;
#else
	/* for spandsp releases 0.0.5 */
	t30state = &t38.t30_state;
	t38state = &t38.t38;
#endif

	/* Initialize terminal */
	memset(&t38, 0, sizeof(t38));
	if (t38_terminal_init(&t38, s->caller_mode, t38_tx_packet_handler, s->chan) == NULL) {
		ast_log(LOG_WARNING, "Unable to start T.38 termination.\n");
		return -1;
	}

	t38_set_max_datagram_size(t38state, s->t38parameters.max_ifp);

	if (s->t38parameters.fill_bit_removal) {
		t38_set_fill_bit_removal(t38state, TRUE);
	}
	if (s->t38parameters.transcoding_mmr) {
		t38_set_mmr_transcoding(t38state, TRUE);
	}
	if (s->t38parameters.transcoding_jbig) {
		t38_set_jbig_transcoding(t38state, TRUE);
	}

	/* Setup logging */
	set_logging(&t38.logging);
	set_logging(&t30state->logging);
	set_logging(&t38state->logging);

	/* Configure terminal */
	set_local_info(t30state, s);
	set_file(t30state, s);
	set_ecm(t30state, TRUE);

	t30_set_phase_e_handler(t30state, phase_e_handler, s);

	now = start = state_change = ast_tvnow();

	while (!s->finished) {
		inf = NULL;
		if ((res = ast_waitfor(s->chan, 20)) < 0) {
			break;
		}

		last_frame = now;
		now = ast_tvnow();
		/* if nothing arrived, check the watchdog timers */
		if (res == 0) {
			if (ast_tvdiff_sec(now, start) > WATCHDOG_TOTAL_TIMEOUT || ast_tvdiff_sec(now, state_change) > WATCHDOG_STATE_TIMEOUT) {
				ast_log(LOG_WARNING, "It looks like we hung. Aborting.\n");
				res = -1;
				break;
			} else {
				/* timers have not triggered, loop around to wait
				 * again
				 */
				t38_terminal_send_timeout(&t38, ast_tvdiff_us(now, last_frame) / (1000000 / 8000));
				continue;
			}
		}

		t38_terminal_send_timeout(&t38, ast_tvdiff_us(now, last_frame) / (1000000 / 8000));

		if (!(inf = ast_read(s->chan))) {
			ast_debug(1, "Channel hangup\n");
			res = -1;
			break;
		}

		ast_debug(10, "frame %d/%d, len=%d\n", inf->frametype, inf->subclass, inf->datalen);

		if (inf->frametype == AST_FRAME_MODEM && inf->subclass == AST_MODEM_T38) {
			t38_core_rx_ifp_packet(t38state, inf->data.ptr, inf->datalen, inf->seqno);
			if (last_state != t30state->state) {
				state_change = ast_tvnow();
				last_state = t30state->state;
			}
		} else if (inf->frametype == AST_FRAME_CONTROL && inf->subclass == AST_CONTROL_T38_PARAMETERS) {
			struct ast_control_t38_parameters *parameters = inf->data.ptr;
			if (parameters->request_response == AST_T38_TERMINATED) {
				ast_debug(1, "T38 down, finishing\n");
				break;
			}
		}

		ast_frfree(inf);
	}

	ast_debug(1, "Loop finished, res=%d\n", res);

	if (inf)
		ast_frfree(inf);

	t30_terminate(t30state);
	t38_terminal_release(&t38);

	return res;
}

static int transmit(fax_session *s)
{
	int res = 0;

	/* Clear all channel variables which to be set by the application.
	   Pre-set status to error so in case of any problems we can just leave */
	pbx_builtin_setvar_helper(s->chan, "FAXSTATUS", "FAILED"); 
	pbx_builtin_setvar_helper(s->chan, "FAXERROR", "Channel problems"); 

	pbx_builtin_setvar_helper(s->chan, "FAXMODE", NULL);
	pbx_builtin_setvar_helper(s->chan, "REMOTESTATIONID", NULL);
	pbx_builtin_setvar_helper(s->chan, "FAXPAGES", NULL);
	pbx_builtin_setvar_helper(s->chan, "FAXRESOLUTION", NULL);
	pbx_builtin_setvar_helper(s->chan, "FAXBITRATE", NULL); 

	if (s->chan->_state != AST_STATE_UP) {
		/* Shouldn't need this, but checking to see if channel is already answered
		 * Theoretically asterisk should already have answered before running the app */
		res = ast_answer(s->chan);
		if (res) {
			ast_log(LOG_WARNING, "Could not answer channel '%s'\n", s->chan->name);
			return res;
		}
	}

	s->t38state = ast_channel_get_t38_state(s->chan);
	if (s->t38state != T38_STATE_NEGOTIATED) {
		/* T38 is not negotiated on the channel yet. First start regular transmission. If it switches to T38, follow */	
		pbx_builtin_setvar_helper(s->chan, "FAXMODE", "audio"); 
		res = transmit_audio(s);
		if (res > 0) {
			/* transmit_audio reports switchover to T38. Update t38state */
			s->t38state = ast_channel_get_t38_state(s->chan);
			if (s->t38state != T38_STATE_NEGOTIATED) {
				ast_log(LOG_ERROR, "Audio loop reports T38 switchover but t38state != T38_STATE_NEGOTIATED\n");
			}
		}
	}

	if (s->t38state == T38_STATE_NEGOTIATED) {
		pbx_builtin_setvar_helper(s->chan, "FAXMODE", "T38"); 
		res = transmit_t38(s);
	}

	if (res) {
		ast_log(LOG_WARNING, "Transmission error\n");
		res = -1;
	} else if (s->finished < 0) {
		ast_log(LOG_WARNING, "Transmission failed\n");
	} else if (s->finished > 0) {
		ast_debug(1, "Transmission finished Ok\n");
	}

	return res;
}

/* === Application functions === */

static int sndfax_exec(struct ast_channel *chan, void *data)
{
	int res = 0;
	char *parse;
	fax_session session = { 0, };

	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(file_name);
		AST_APP_ARG(options);
	);

	if (chan == NULL) {
		ast_log(LOG_ERROR, "Fax channel is NULL. Giving up.\n");
		return -1;
	}

	/* The next few lines of code parse out the filename and header from the input string */
	if (ast_strlen_zero(data)) {
		/* No data implies no filename or anything is present */
		ast_log(LOG_ERROR, "SendFAX requires an argument (filename)\n");
		return -1;
	}

	parse = ast_strdupa(data);
	AST_STANDARD_APP_ARGS(args, parse);
	
	session.caller_mode = TRUE;

	if (args.options) {
		if (strchr(args.options, 'a'))
			session.caller_mode = FALSE;
	}

	/* Done parsing */
	session.direction = 1;
	session.file_name = args.file_name;
	session.chan = chan;
	session.finished = 0;

	res = transmit(&session);

	return res;
}

static void ast_t38_gateway(fax_session *s)
{
	int samples, len, timeout;
	struct ast_channel	*active = NULL;
	struct ast_frame	*f;
 	t38_core_state_t	*t38dsp;
	struct ast_channel	*channels[2];
	int16_t buffer[T38_MAX_HDLC_LEN];

	struct ast_frame outf = {
		.frametype = AST_FRAME_VOICE,
		.subclass = AST_FORMAT_SLINEAR,
		.src = "T38Gateway",
	};

	if (ast_channel_get_t38_state(s->chan) == T38_STATE_NEGOTIATED) {
		channels[0]=s->chan;
		channels[1]=s->peer;
	} else {
		channels[0]=s->peer;
		channels[1]=s->chan;
	}

	t38_gateway_state_t t38_state;
	t38_stats_t t38_stats;

#if SPANDSP_RELEASE_DATE >= 20081012
	/* for spandsp shaphots 0.0.6 and higher */
	t38dsp=&t38_state.t38x.t38;
#else
	/* for spandsp release 0.0.5 */
	t38dsp=&t38_state.t38;
#endif

	if (t38_gateway_init(&t38_state, t38_tx_packet_handler, channels[0])) {
		ast_debug(1, "T.38 Gateway Starting chan %s peer %s\n", s->chan->name, s->peer->name);

		t38_gateway_set_transmit_on_idle(&t38_state, TRUE);

		span_log_set_message_handler(&t38_state.logging, span_message);
		span_log_set_message_handler(&t38dsp->logging, span_message);
		span_log_set_level(&t38_state.logging, SPAN_LOG_WARNING + option_debug);
		span_log_set_level(&t38dsp->logging, SPAN_LOG_WARNING + option_debug);

		t38_set_t38_version(t38dsp, 0);
		t38_gateway_set_ecm_capability(&t38_state, TRUE);
		t38_set_sequence_number_handling(t38dsp, TRUE);
		t38_gateway_set_supported_modems(&t38_state, T30_SUPPORT_V27TER | T30_SUPPORT_V17 | T30_SUPPORT_V29); 

		s->finished=1;
		for(;;) {
			timeout=20;
			active = ast_waitfor_n(channels, 2, &timeout);
			if (active) {
				f = ast_read(active);
				if (f) {
					if (active == channels[0]) {
						if (f->frametype == AST_FRAME_MODEM && f->subclass == AST_MODEM_T38)
							t38_core_rx_ifp_packet(t38dsp, f->data.ptr, f->datalen, f->seqno);
					} else if (active) {
						/* we should not be T.38 if we are something went wrong with T.38 negotiation*/
						if (f->frametype == AST_FRAME_MODEM && f->subclass == AST_MODEM_T38) {
							s->finished=-1;
							break;
						}
						t38_gateway_rx(&t38_state, f->data.ptr, f->samples);
						samples = (f->samples <= T38_MAX_HDLC_LEN)  ?  f->samples : T38_MAX_HDLC_LEN;
						if ((len = t38_gateway_tx(&t38_state, buffer, samples))) {
							AST_FRAME_SET_BUFFER(&outf, buffer, 0, len*sizeof(int16_t));
							outf.samples=len;
							ast_write(channels[1], &outf);
						}
					}
					ast_frfree(f);
				} else
					break;
			}
		}
		t38_gateway_get_transfer_statistics(&t38_state, &t38_stats);
		ast_debug(1, "Connection Statistics\n\tBit Rate :%i\n\tECM : %s\n\tPages : %i\n",t38_stats.bit_rate, (t38_stats.error_correcting_mode?"Yes":"No"), t38_stats.pages_transferred);
	} else {
		ast_log(LOG_ERROR, "T.38 gateway failed to init\n");
		s->finished=-1;
	}
}

static void ast_bridge_frames(fax_session *s)
{
	struct ast_dsp *dsp;
	struct ast_channel *active = NULL;
	struct ast_channel *inactive = NULL;
	struct ast_channel *channels[2]={s->chan, s->peer};
	enum ast_t38_state state[2];
	enum ast_t38_state cstate[2];
	struct ast_frame *f;
	int timeout, ftone=0;
	enum ast_control_t38 t38control = AST_T38_REQUEST_NEGOTIATE;

	state[0]=ast_channel_get_t38_state(s->chan);
	state[1]=ast_channel_get_t38_state(s->peer);

	/* Setup DSP CNG/CED processing */
	if ((dsp=ast_dsp_new())) {
		ast_dsp_set_features(dsp, DSP_FEATURE_FAX_DETECT);
		ast_dsp_set_faxmode(dsp, DSP_FAXMODE_DETECT_CNG | DSP_FAXMODE_DETECT_CED);
	} else
		ast_debug(1, "Unable to allocate Fax Detect DSP This may lead to problems with T.38 switchover!\n");

	for(;;) {
		timeout=20;
		if ((active = ast_waitfor_n(channels, 2, &timeout))) {
			inactive = (active == channels[0])  ?   channels[1]  :  channels[0];
			cstate[0] = (active == channels[0])  ? state[0] : state[1];
			cstate[1] = (active == channels[0])  ? state[1] : state[0];

			/* update channel status if T.38 is still Possible*/
			if ((cstate[0] == T38_STATE_UNKNOWN ) || (cstate[0] == T38_STATE_NEGOTIATING ))
				cstate[0] = ast_channel_get_t38_state(active);
			if ((cstate[1] == T38_STATE_UNKNOWN ) || (cstate[1] == T38_STATE_NEGOTIATING ))
				cstate[1] = ast_channel_get_t38_state(inactive);

			/* Leave and gateway if all channels are in a stable T.38 state and both are not T.38 */
			if (((cstate[0] == T38_STATE_REJECTED) || (cstate[1] == T38_STATE_REJECTED) ||
		             (cstate[0] == T38_STATE_UNAVAILABLE) || (cstate[1] == T38_STATE_UNAVAILABLE)) &&
			    ((cstate[0] != T38_STATE_UNKNOWN) && (cstate[1] != T38_STATE_UNKNOWN) &&
			     (cstate[0] != T38_STATE_NEGOTIATING) && (cstate[1] != T38_STATE_NEGOTIATING)) &&
			    (cstate[0] != cstate[1]))
				break;
			if ((f = ast_read(active))) {
				/* Dont send packets to a channel negotiating T.38 ignore them*/
				if ((cstate[1] != T38_STATE_NEGOTIATED) && (cstate[1] != T38_STATE_NEGOTIATING)) {
					if ((dsp) && (cstate[1] == T38_STATE_UNKNOWN)) {
						f = ast_dsp_process(active, dsp, f);
						if ((f->frametype == AST_FRAME_DTMF) &&
						    ((f->subclass == 'f') || (f->subclass == 'e'))) {
							switch (f->subclass) {
								case 'f':ftone=1;
								case 'e':ftone=2;
							}
							/* tickle the channel as we have fax tone */
							ast_indicate_data(inactive, AST_CONTROL_T38, &t38control, sizeof(t38control));
							t38control = AST_T38_REQUEST_NEGOTIATE;
							ast_debug(1, "Fax %s Tone Detected On %s\n", (ftone == 1) ? "CNG" : "CED", active->name);
						} else {
							ast_write(inactive, f);
						}
					} else
						ast_write(inactive, f);
				}
				ast_frfree(f);
			} else {
				s->finished=1;
				break;
			}
		}
	}
        if (dsp)
		ast_dsp_free(dsp);
}

static int app_t38gateway_exec(struct ast_channel *chan, void *data)
{
	int res = 0;
	char *parse;
	fax_session *s=ast_malloc(sizeof(fax_session));
	int state, priority, timeout, fdtimeout;
	const char *account=NULL, *cid_name=NULL, *cid_num=NULL, *context=NULL, *exten=NULL;
	struct ast_variable *vars=NULL;
	struct outgoing_helper oh;

	if (!chan || !s) {
		ast_debug(1, "Fax channel is NULL Or No Fax Handler Possible. Giving up.\n");
		ast_free(s);
		return -1;
	}

	s->chan = chan;
	
	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(dest);
		AST_APP_ARG(timeout);
		AST_APP_ARG(fdtimeout);
	);
	parse = ast_strdupa(data);
	AST_STANDARD_APP_ARGS(args, parse);

	/* Get a technology/[device:]number pair */
	char *number = args.dest;
	char *tech = strsep(&number, "/");
	if (args.timeout)
		timeout = atoi(args.timeout) * 1000;
	else
		timeout = 35000;

	if (args.fdtimeout)
		fdtimeout = atoi(args.fdtimeout) * 1000;
	else
		fdtimeout = 4000;

	if (!number) {
		ast_debug(1, "dialstring argument takes format (technology/[device:]number1)\n");
		ast_free(s);
		return -1;
	}
	char numsubst[256];
	ast_copy_string(numsubst, number, sizeof(numsubst));

	/* Setup the outgoing helper and dial waiting for timeout or answer*/
	if (!ast_strlen_zero(chan->cid.cid_num))
		cid_num=ast_strdupa(chan->cid.cid_num);
	if (!ast_strlen_zero(chan->cid.cid_name))
		cid_name=ast_strdupa(chan->cid.cid_name);
	if (!ast_strlen_zero(chan->context))
		context=ast_strdupa(chan->context);
	if (!ast_strlen_zero(chan->exten))
		exten=ast_strdupa(chan->exten);
	priority=chan->priority;

	oh.context = context;
	oh.exten = exten;
	oh.priority = priority;
	oh.cid_num = cid_num;
	oh.cid_name = cid_name;
	oh.account = account;
	oh.vars = vars;

	oh.parent_channel=chan;
	if (!(s->peer = __ast_request_and_dial(tech, AST_FORMAT_SLINEAR, numsubst, timeout, &state, chan->cid.cid_num, chan->cid.cid_name, &oh))) {
		chan->hangupcause = state;
		ast_free(s);
		return -1;
	}

	/* pick up originating call */
	if (chan->_state != AST_STATE_UP) {
		res = ast_answer(chan);
		if (res) {
			ast_debug(1, "Could not answer channel '%s' cant run a gateway on a down channel\n", chan->name);
			ast_free(s);
			return res;
		}
	}

	pbx_builtin_setvar_helper(chan, "DIALSTATUS", ast_strdup(ast_cause2str(state)));
	manager_event(EVENT_FLAG_CALL, "Dial",
		"SubEvent: Begin\r\n"
		"Channel: %s\r\n"
		"Destination: %s\r\n"
		"CallerIDNum: %s\r\n"
		"CallerIDName: %s\r\n"
		"UniqueID: %s\r\n"
		"DestUniqueID: %s\r\n"
		"Dialstring: %s\r\n",
		chan->name, s->peer->name, S_OR(chan->cid.cid_num, "<unknown>"),
		S_OR(chan->cid.cid_name, "<unknown>"), chan->uniqueid,
		s->peer->uniqueid, number ? number : "");

        if (chan->cdr)
                ast_cdr_setdestchan(chan->cdr, s->peer->name);

	/* Copy important bits from incoming to outgoing */
/*
	s->peer->appl = app_faxgw_name;
	ast_string_field_set(s->peer, language, chan->language);
	ast_string_field_set(s->peer, accountcode, chan->accountcode);

	s->peer->cdrflags = chan->cdrflags;
	if (ast_strlen_zero(s->peer->musicclass))
		ast_string_field_set(s->peer, musicclass, chan->musicclass);
	s->peer->adsicpe = chan->adsicpe;
	s->peer->transfercapability = chan->transfercapability;

	if (chan->cid.cid_rdnis)
		s->peer->cid.cid_rdnis=ast_strdup(chan->cid.cid_rdnis);
	s->peer->cid.cid_pres = chan->cid.cid_pres;
	s->peer->cid.cid_ton = chan->cid.cid_ton;
	s->peer->cid.cid_tns = chan->cid.cid_tns;
	s->peer->cid.cid_ani2 = chan->cid.cid_ani2;

*/
	/* Inherit context and extension */
	if (!ast_strlen_zero(chan->macrocontext))
		ast_copy_string(s->peer->dialcontext, chan->macrocontext, sizeof(s->peer->dialcontext));
	if (!ast_strlen_zero(chan->macroexten))
		ast_copy_string(s->peer->exten, chan->macroexten, sizeof(s->peer->exten));
	s->peer->macropriority=chan->macropriority;

        /* Clear all channel variables which to be set by the application.
           Pre-set status to error so in case of any problems we can just leave */
	pbx_builtin_setvar_helper(s->peer, "DIALEDPEERNUMBER", numsubst);

        pbx_builtin_setvar_helper(chan, "FAXSTATUS", "FAILED");
        pbx_builtin_setvar_helper(chan, "FAXERROR", "Channel problems");

	/* Stop monitor and set channels to signed linear codec */
	ast_monitor_stop(chan, 1);
	ast_set_write_format(s->chan, AST_FORMAT_SLINEAR);
	ast_set_read_format(s->chan, AST_FORMAT_SLINEAR);
	ast_set_write_format(s->peer, AST_FORMAT_SLINEAR);
	ast_set_read_format(s->peer, AST_FORMAT_SLINEAR);

	ast_channel_make_compatible(s->chan, s->peer);

	/* start the gateway*/
	s->finished = 0;

	/* Start bridging packets until all sides have negotiated T.38 capabilities and only one is capable*/
	ast_bridge_frames(s);

	/* We are now T.38 One Side Gateway*/
	if (!s->finished)
		ast_t38_gateway(s);

	if (s->finished < 0) {
		ast_log(LOG_WARNING, "Transmission failed\n");
		pbx_builtin_setvar_helper(chan, "FAXSTATUS", "FAILED"); 
		pbx_builtin_setvar_helper(chan, "FAXERROR", "Channel problems"); 
		res = -1;
	} else if (s->finished > 0) {
		ast_debug(1, "Transmission finished Ok\n");
		pbx_builtin_setvar_helper(chan, "FAXSTATUS", "PASSED"); 
		pbx_builtin_setvar_helper(chan, "FAXERROR", "OK"); 
		res = 0;
	} else {
		ast_log(LOG_WARNING, "Transmission error\n");
		pbx_builtin_setvar_helper(chan, "FAXSTATUS", "FAILED"); 
		pbx_builtin_setvar_helper(chan, "FAXERROR", "Transmission error"); 
		res = -1;
	}
	if (s->peer) {
		ast_hangup(s->peer);
	}
	ast_free(s);
	return res;
}

static int rcvfax_exec(struct ast_channel *chan, void *data)
{
	int res = 0;
	char *parse;
	fax_session session;

	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(file_name);
		AST_APP_ARG(options);
	);

	if (chan == NULL) {
		ast_log(LOG_ERROR, "Fax channel is NULL. Giving up.\n");
		return -1;
	}

	/* The next few lines of code parse out the filename and header from the input string */
	if (ast_strlen_zero(data)) {
		/* No data implies no filename or anything is present */
		ast_log(LOG_ERROR, "ReceiveFAX requires an argument (filename)\n");
		return -1;
	}

	parse = ast_strdupa(data);
	AST_STANDARD_APP_ARGS(args, parse);
	
	session.caller_mode = FALSE;

	if (args.options) {
		if (strchr(args.options, 'c'))
			session.caller_mode = TRUE;
	}

	/* Done parsing */
	session.direction = 0;
	session.file_name = args.file_name;
	session.chan = chan;
	session.finished = 0;

	res = transmit(&session);

	return res;
}

static int unload_module(void)
{
	int res;

	res = ast_unregister_application(app_sndfax_name);	
	res |= ast_unregister_application(app_rcvfax_name);	
	res |= ast_unregister_application(app_t38gateway_name);

	return res;
}

static int load_module(void)
{
	int res ;

	res = ast_register_application_xml(app_sndfax_name, sndfax_exec);
	res |= ast_register_application_xml(app_rcvfax_name, rcvfax_exec);
	/* TODO: write synopsis,description in XML */
	res |= ast_register_application(app_t38gateway_name, app_t38gateway_exec, app_t38gateway_synopsis, app_t38gateway_desc);

	/* The default SPAN message handler prints to stderr. It is something we do not want */
	span_set_message_handler(NULL);

	return res;
}


AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_DEFAULT, "Simple FAX Application",
		.load = load_module,
		.unload = unload_module,
		);


