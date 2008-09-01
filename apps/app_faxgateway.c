/*
 * Asterisk -- A telephony toolkit for Linux.
 *
 * Simple fax gateway applications
 * 
 * 2008, Daniel Ferenci <daniel.ferenci@nethemba.com> 
 * Created by Nethemba s.r.o. http://www.nethemba.com
 * Sponsored by IPEX a.s. http://www.ipex.cz
 * 
 * Code based on original implementation by Steve Underwood <steveu@coppice.org>
 * and Dmitry Andrianov <asterisk@dima.spb.ru>
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License
 *
 */

/*
Todo:
	1. switch over to T38 when CED or CNG detected
	2. to make things compatible use macros
*/
 
/*** MODULEINFO
	 <depend>spandsp</depend>
***/
 
#include "asterisk.h"

ASTERISK_FILE_VERSION(__FILE__, "$Revision: 1.19 $")

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <pthread.h>
#include <errno.h>
#include <tiffio.h>

#include <spandsp.h>

#include "asterisk/lock.h"
#include "asterisk/file.h"
#include "asterisk/logger.h"
#include "asterisk/channel.h"
#include "asterisk/pbx.h"
#include "asterisk/app.h"
#include "asterisk/dsp.h"
#include "asterisk/module.h"
#include "asterisk/manager.h"
#include "asterisk/causes.h"
#include "asterisk/musiconhold.h"
#include "asterisk/rtp.h"

static char *app_faxgw_name = "FaxGateway";
static char *app_faxgw_synopsis = "FAX Gateway";
static char *app_faxgw_desc = 
"  FaxGateway(dialstring[, timeout]):\n"
"Send a given TIFF file to the channel as a FAX.\n"
"This application uses following variables:\n"
"\n"
"This application sets the following channel variables upon completion:\n"
"     FAXSTATUS - status of operation:\n"
"		  SUCCESS | FAILED\n"
"     FAXERROR	- Error when FAILED\n"
"\n"
"Returns -1 in case of user hang up or any channel error.\n"
"Returns 0 on success.\n";

#define DONE_WITH_ERROR -1
#define RUNNING 1
#define DONE 0

#define MAX_BLOCK_SIZE 240

#define MAX_SAMPLES 240

#define ready_to_talk(chan,peer) ((!chan  ||  !peer  ||  ast_check_hangup(chan)  ||  ast_check_hangup(peer))  ?  0  :  1)

#define clean_frame(f) if(f) {ast_frfree(f); f = NULL;}

/* Watchdog. I have seen situations when remote fax disconnects (because of poor line
   quality) while SpanDSP continues staying in T30_STATE_IV_CTC state forever.
   To avoid this, we terminate when we see that T30 state does not change for 5 minutes.
   We also terminate application when more than 30 minutes passed regardless of
   state changes. This is just a precaution measure - no fax should take that long */

#define WATCHDOG_TOTAL_TIMEOUT	30 * 60
#define WATCHDOG_STATE_TIMEOUT	5 * 60

/* free the buffer if allocated, and set the pointer to the second arg */
#define S_REPLACE(s, new_val)		\
	do {				\
		if (s)			\
			ast_free(s);	\
		s = (new_val);		\
	} while (0)
 
typedef struct {
	struct ast_channel *chan;
	struct ast_channel *peer;
	enum ast_t38_state chan_t38state;	/* T38 state of the channel */
	enum ast_t38_state peer_t38state;	/* T38 state of the peer */
	int direction;			/* Fax direction: 0 - receiving, 1 - sending */
	int caller_mode;
	int verbose;
	char *dest;
	char * dest_type;
	int timeout;
	volatile int finished;
} fax_session;

static void senddialevent(struct ast_channel *src, struct ast_channel *dst, const char *dialstring)
{
	manager_event(EVENT_FLAG_CALL, "Dial",
		"SubEvent: Begin\r\n"
		"Channel: %s\r\n"
		"Destination: %s\r\n"
		"CallerIDNum: %s\r\n"
		"CallerIDName: %s\r\n"
		"UniqueID: %s\r\n"
		"DestUniqueID: %s\r\n"
		"Dialstring: %s\r\n",
		src->name, dst->name, S_OR(src->cid.cid_num, "<unknown>"),
		S_OR(src->cid.cid_name, "<unknown>"), src->uniqueid,
		dst->uniqueid, dialstring ? dialstring : "");
}

struct cause_args {
	struct ast_channel *chan;
	int busy;
	int congestion;
	int nochan;
};

static void handle_cause(int cause, struct cause_args *num)
{
	struct ast_cdr *cdr = num->chan->cdr;

	switch(cause) {
	case AST_CAUSE_BUSY:
		if (cdr)
			ast_cdr_busy(cdr);
		num->busy++;
		break;

	case AST_CAUSE_CONGESTION:
		if (cdr)
			ast_cdr_failed(cdr);
		num->congestion++;
		break;

	case AST_CAUSE_UNREGISTERED:
		if (cdr)
			ast_cdr_failed(cdr);
		num->nochan++;
		break;

	case AST_CAUSE_NORMAL_CLEARING:
		break;

	default:
		num->nochan++;
		break;
	}
}

static void span_message(int level, const char *msg)
{
	if (level == SPAN_LOG_ERROR)
		ast_log(LOG_ERROR, msg);
	else if (level == SPAN_LOG_WARNING)
		ast_log(LOG_WARNING, msg);
	else
		ast_log(LOG_DEBUG, msg);
}

static int t38_tx_packet_handler(t38_core_state_t *s, void *user_data, const uint8_t *buf, int len, int count)
{
	ast_log(LOG_DEBUG, "t38_tx_packet_handler entry.");
	
	struct ast_channel *chan = (struct ast_channel *) user_data;

	struct ast_frame outf = {
		.frametype = AST_FRAME_MODEM,
		.subclass = AST_MODEM_T38,
		.src = __FUNCTION__,
	};

	AST_FRAME_SET_BUFFER(&outf, buf, 0, len);

	if (ast_write(chan, &outf) < 0) {
		ast_log(LOG_WARNING, "Unable to write frame to channel; %s\n", strerror(errno));
		return -1;
	}

	return 0;
}

/* === Gateway === */

static int ast_bridge_frames(fax_session *s)
{
    struct ast_channel *active = NULL;
    struct ast_channel *inactive = NULL;
    struct ast_channel *channels[2];
    struct ast_frame *f;
    struct ast_frame *fr2;
    int timeout;
    int running = RUNNING;
    struct ast_dsp *dsp_cng = NULL;
    struct ast_dsp *dsp_ced = NULL;
    enum ast_control_t38 t38control;

    ast_log(LOG_DEBUG, "faxGw - ast_bridge_frames entry \n");
    
    if ((dsp_cng = ast_dsp_new()) == NULL)
    {
        ast_log(LOG_WARNING, "Unable to allocate DSP!\n");
    }
    else
    {
        ast_dsp_set_threshold(dsp_cng, 256); 
        ast_dsp_set_features(dsp_cng, DSP_FEATURE_FAX_DETECT | DSP_FAXMODE_DETECT_CNG);
        ast_dsp_set_digitmode(dsp_cng, DSP_DIGITMODE_DTMF | DSP_DIGITMODE_RELAXDTMF);
    }

    if ((dsp_ced = ast_dsp_new()) == NULL)
    {
        ast_log(LOG_WARNING, "Unable to allocate DSP!\n");
    }
    else
    {
        ast_dsp_set_threshold(dsp_ced, 256); 
        ast_dsp_set_features(dsp_ced, DSP_FAXMODE_DETECT_CED);
    }

    
    channels[0] = s->chan;
    channels[1] = s->peer;

    while (running == RUNNING  &&  (running = ready_to_talk(channels[0], channels[1])))
    {

//      ast_log(LOG_NOTICE, "br: t38 status: [%d,%d]\n", s->chan_t38state, s->peer_t38state);
//	ast_log(LOG_NOTICE, "br: %s %s \n", s->chan->name , s->peer->name);
	
	timeout = s->timeout;
        if ((active = ast_waitfor_n(channels, 2, &timeout)))
        {
            inactive = (active == channels[0])  ?   channels[1]  :  channels[0];
            if ((f = ast_read(active)))
            {

                if (dsp_ced  &&  dsp_cng)
                    fr2 = ast_frdup(f);
                else
                    fr2 = NULL;

    		if (( s->chan_t38state == T38_STATE_NEGOTIATING ) || ( s->peer_t38state == T38_STATE_NEGOTIATING )) 
		{
		/*  TODO 
		    This is a very BASIC method to mute a channel. It should be improved
		    and we should send EMPTY frames (not just avoid sending them) 
		*/
		    ast_log(LOG_DEBUG, "channels are muted.\n");
		}
		else
		{
		    //ast_log(LOG_NOTICE, "do write on %p\n", inactive);	
            	    int ret = ast_write(inactive, f);
		    if (ret !=0 )
		    {
			   ast_log(LOG_NOTICE, "write error \n"); 
		    }
		}
		    
                clean_frame(f);
                channels[0] = inactive;
                channels[1] = active;

                if (active == s->chan)
                {
                    /* Look for FAX CNG tone */
                    if (fr2  &&  dsp_cng)
                    {
                        if ((fr2 = ast_dsp_process(active, dsp_cng, fr2)))
                        {
                            if (fr2->frametype == AST_FRAME_DTMF)
                            {
                                if (fr2->subclass == 'f')
                                {
                                    ast_log(LOG_DEBUG, "FAX CNG detected in T38 Gateway !!!\n");
                                    // switch over to t38
				    t38control = AST_T38_REQUEST_NEGOTIATE;
				    ast_indicate_data(s->chan, AST_CONTROL_T38, &t38control, sizeof(t38control));
                                }
                            }
                        }
                    }
                }
                else
                {
                    /* Look for FAX CED tone or V.21 preamble */
                    if (fr2  &&  dsp_ced)
                    {
                        if ((fr2 = ast_dsp_process(active, dsp_ced, fr2)))
                        {
                            if (fr2->frametype == AST_FRAME_DTMF)
                            {
                                if (fr2->subclass == 'F')
                                {
                                    ast_log(LOG_DEBUG, "FAX CED detected in T38 Gateway !!!\n");
                                    // switch over to t38
				    t38control = AST_T38_REQUEST_NEGOTIATE;
				    ast_indicate_data(s->chan, AST_CONTROL_T38, &t38control, sizeof(t38control));
	                        }
                            }
                        }
                    }
                }
                if (f != fr2)
                {
                    if (fr2)
                        ast_frfree(fr2);
                    fr2 = NULL;
                }
            }
            else
            {
                running = DONE;
            }
        }

	s->chan_t38state = ast_channel_get_t38_state(s->chan);
	s->peer_t38state = ast_channel_get_t38_state(s->peer);

        /* Check if we need to change to gateway operation */
        if ( 
	        ( s->chan_t38state != T38_STATE_NEGOTIATING ) 
	     && ( s->peer_t38state != T38_STATE_NEGOTIATING )
	     && ( s->chan_t38state != s->peer_t38state) 
	   ) {
            ast_log(LOG_DEBUG, "Stop bridging frames. [ %d,%d]\n", s->chan_t38state, s->peer_t38state);
            running = RUNNING;
            break;
	}
    }

    if (dsp_cng)
        ast_dsp_free(dsp_cng);
    if (dsp_ced)
        ast_dsp_free(dsp_ced);

    return running;
}


static int ast_t38_gateway(fax_session *s)
{
    struct ast_channel *active = NULL;
    struct ast_channel *inactive = NULL;
    struct ast_channel *channels[2];
    enum ast_t38_state states[2];

    struct ast_frame *f;
    struct ast_frame outf;
    int timeout;
    int running = RUNNING;
    int original_read_fmt;
    int original_write_fmt;
    int res;
    int samples;
    int len;
    s->verbose = 1;
    
    enum ast_control_t38 t38control;
    t38_gateway_state_t t38_state;
    
    uint8_t __buf[sizeof(uint16_t)*MAX_BLOCK_SIZE + 2*AST_FRIENDLY_OFFSET];
    uint8_t *buf = __buf + AST_FRIENDLY_OFFSET;
    
    ast_log(LOG_DEBUG, "faxGw - ast_t38_gateway entry \n");

    if ( s->chan_t38state == T38_STATE_NEGOTIATED )
    {
        channels[0] = s->chan;
        channels[1] = s->peer;
	states[0] = s->chan_t38state;
	states[1] = s->peer_t38state;
    }
    else
    {
        channels[0] = s->peer;
        channels[1] = s->chan;
	states[0] = s->peer_t38state;
	states[1] = s->chan_t38state;
    }
    
    ast_log(LOG_DEBUG, "chan %p  peer %p channels[0] %p channels[1] %p\n", s->chan, s->peer, channels[0], channels[1]);

    original_read_fmt = channels[1]->readformat;
    original_write_fmt = channels[1]->writeformat;
    if ( states[1] != T38_STATE_NEGOTIATED)
    {
        if (original_read_fmt != AST_FORMAT_SLINEAR)
        {
            if ((res = ast_set_read_format(channels[1], AST_FORMAT_SLINEAR)) < 0)
            {
                ast_log(LOG_WARNING, "Unable to set to linear read mode, giving up\n");
                return -1;
            }
        }
        if (original_write_fmt != AST_FORMAT_SLINEAR)
        {
            if ((res = ast_set_write_format(channels[1], AST_FORMAT_SLINEAR)) < 0)
            {
                ast_log(LOG_WARNING, "Unable to set to linear write mode, giving up\n");
                if ((res = ast_set_read_format(channels[1], original_read_fmt)))
                    ast_log(LOG_WARNING, "Unable to restore read format on '%s'\n", channels[1]->name);
                return -1;
            }
        }
    }

    if (t38_gateway_init(&t38_state, t38_tx_packet_handler, channels[0]) == NULL)
    {
        ast_log(LOG_WARNING, "Unable to start the T.38 gateway\n");
        return -1;
    }
    ast_log(LOG_NOTICE, "T.38 gateway started\n");
    t38_gateway_set_transmit_on_idle(&t38_state, TRUE);

    span_log_set_message_handler(&t38_state.logging, span_message);
    span_log_set_message_handler(&t38_state.t38.logging, span_message);
    if (s->verbose)
    {
        span_log_set_level(&t38_state.logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
        span_log_set_level(&t38_state.t38.logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    }
    t38_set_t38_version(&t38_state.t38, 0);
    t38_gateway_set_ecm_capability(&t38_state, 1);


    while (running == RUNNING  &&  (running = ready_to_talk(channels[0], channels[1])))
    {
        // ast_log(LOG_NOTICE, "gw: t38status: [%d,%d]\n", s->chan_t38state, s->peer_t38state);
        if ( 
	        ( s->chan_t38state == T38_STATE_NEGOTIATED ) 
	     && ( s->peer_t38state == T38_STATE_NEGOTIATED )
	   ) {
            ast_log(LOG_DEBUG, "Stop gateway-ing frames (both channels are in t38 mode). [ %d,%d]\n", s->chan_t38state, s->peer_t38state);
            running = RUNNING;
            break;
	}
	
	timeout = s->timeout;
        if ((active = ast_waitfor_n(channels, 2, &timeout)))
        {
//	    char tmpString[20];
//	    if (active == s->chan)
//	    {
//		 strncpy(tmpString, "chan", 20);   
//	    }
//	    if (active == s->peer)
//	    {
//  		 strncpy(tmpString, "peer", 20);   
//	    }
//	    ast_log(LOG_DEBUG, "Fax gw - active - %s \n" , tmpString);
            if (active == channels[0])
            {
                if ((f = ast_read(active)))
                {
//		    ast_log(LOG_DEBUG, "ast_t38_gateway received %d \n" , f->frametype);
		    if (f->frametype == AST_FRAME_MODEM && f->subclass == AST_MODEM_T38)
		    {
			    t38_core_rx_ifp_packet(&t38_state.t38, f->data.ptr, f->datalen, f->seqno);
			    
		    }
		    else if (f->frametype == AST_FRAME_CONTROL && f->subclass == AST_CONTROL_T38 &&
				f->datalen == sizeof(enum ast_control_t38)) {

			t38control = *((enum ast_control_t38 *) f->data.ptr);

			if (t38control == AST_T38_TERMINATED || t38control == AST_T38_REFUSED) {
				ast_log(LOG_DEBUG, "T38 down, terminating\n");
				res = -1;
				break;
			}
		    }
                    clean_frame(f);
                }
                else
                {
                    running = DONE;
                }
            }
            else
            {
                if ((f = ast_read(active)))
                {
                    if (t38_gateway_rx(&t38_state, f->data.ptr, f->samples))
                        break;

                    samples = (f->samples <= MAX_BLOCK_SIZE)  ?  f->samples  :  MAX_BLOCK_SIZE;

                    if ((len = t38_gateway_tx(&t38_state, (int16_t *) &buf[AST_FRIENDLY_OFFSET], samples)))
                    {
			outf.frametype = AST_FRAME_VOICE;
			outf.subclass = AST_FORMAT_SLINEAR;
			outf.src = "T38Gateway";
                        outf.datalen = len*sizeof(int16_t);
                        outf.samples = len;
                        outf.data.ptr = &buf[AST_FRIENDLY_OFFSET];
                        outf.offset = AST_FRIENDLY_OFFSET;
			outf.mallocd = 0;
			outf.offset = 0;
			
//			ast_log(LOG_DEBUG, "t38 gateway write to %p\n", channels[1]);
                        if (ast_write(channels[1], &outf) < 0)
                        {
                            ast_log(LOG_WARNING, "Unable to write frame to channel; %s\n", strerror(errno));
                            break;
                        }
                    }
                    clean_frame(f);
                }
                else
                {
                    running = DONE;
                }
                inactive = (active == channels[0])  ?   channels[1]  :  channels[0];
            }
        }
    }

    if (original_read_fmt != AST_FORMAT_SLINEAR)
    {
        if ((res = ast_set_read_format(channels[1], original_read_fmt)))
            ast_log(LOG_WARNING, "Unable to restore read format on '%s'\n", channels[1]->name);
    }
    if (original_write_fmt != AST_FORMAT_SLINEAR)
    {
        if ((res = ast_set_write_format(channels[1], original_write_fmt)))
            ast_log(LOG_WARNING, "Unable to restore write format on '%s'\n", channels[1]->name);
    }
    return running;
}


static int transmit(fax_session *s)
{
	int res = 0;
	struct ast_channel *active = NULL;
	struct ast_channel *channels[2];
	char status[256];
	struct ast_frame *f;
	int state = 0, ready = 0;
	
	ast_log(LOG_DEBUG, "faxGw - transmit entry. \n");
	
	/* Clear all channel variables which to be set by the application.
	   Pre-set status to error so in case of any problems we can just leave */
	pbx_builtin_setvar_helper(s->chan, "FAXSTATUS", "FAILED"); 
	pbx_builtin_setvar_helper(s->chan, "FAXERROR", "Channel problems"); 
	
	ast_log(LOG_DEBUG, "FaxGw - transmit - type %s destination %s\n", s->dest_type, s->dest);
	/* Call to the peer side of the gateway */
	/* By default try to initiate t38 call */
	pbx_builtin_setvar_helper(s->peer, "T38CALL", "1"); 
	
	res = ast_call(s->peer, s->dest, 0);
	/* check the results of ast_call */
	if (res) {
		ast_log(LOG_WARNING, "Could not answer channel '%s'\n", s->chan->name);
		ast_debug(1, "ast call on peer returned %d\n", res);
		ast_verb(3, "Couldn't call %s\n", s->chan->name);
		ast_hangup(s->peer);
		s->peer = NULL;
		return res;
	} else {
		senddialevent(s->chan, s->peer, s->dest);
		ast_verb(3, "Called %s\n", s->dest);
	}

	if (s->chan->cdr)
		ast_cdr_setdestchan(s->chan->cdr, s->peer->name);
	
	ast_log(LOG_DEBUG, "faxGw - after ast_call.\n");
	
	strncpy(status, "CHANUNAVAIL", sizeof(status) - 1); /* assume as default */
	channels[0] = s->peer;
	channels[1] = s->chan;

	/* Building up a peer connection while we haven't timed out and we still have no channel up */
	while (s->timeout  &&  (s->peer->_state != AST_STATE_UP))
	{
		/* check the activity on both channels */
		/* active = pointer to active channel */
		ast_log(LOG_DEBUG, "faxGw - waiting for activity on channels \n");
		
		timeout = s->timeout;
		active = ast_waitfor_n(channels, 2, &timeout);
		if (active)
		{
			/* Timed out, so we are done trying */
			if (s->timeout == 0)
			{
				strncpy(status, "NOANSWER", sizeof(status) - 1);
				ast_log(LOG_DEBUG, "Timeout on peer\n");
				break;
			}
			/* -1 means go forever */
			if (s->timeout > -1)
			{
				/* res holds the number of milliseconds remaining */
				if (s->timeout < 0)
				{
					s->timeout = 0;
					strncpy(status, "NOANSWER", sizeof(status) - 1);
				}
			}
			/* peer activity => check what is it about */
			if (active == s->peer)
			{
				ast_log(LOG_DEBUG, "faxGw - something happend on peer\n");
				/* Read the frame f */	
				f = ast_read(active);
				if (f == NULL)
				{
					ast_log(LOG_NOTICE, "faxGw - peer hanged up\n");
					/* HANG UP */
					state = AST_CONTROL_HANGUP;
					s->chan->hangupcause = s->peer->hangupcause;
					res = 0;
					break;
				}
				
				/* the frame is signaling = control frame */
				if (f->frametype == AST_FRAME_CONTROL)
				{
					ast_log(LOG_DEBUG, "faxGw - AST_FRAME_CONTROL\n");
					if (f->subclass == AST_CONTROL_RINGING)
					{
						ast_log(LOG_DEBUG, "faxGw - AST_CONTROL_RINGING\n");
						/* ringing */
						 state = f->subclass;
						 ast_indicate(s->chan, AST_CONTROL_RINGING);
						 ast_frfree(f);
						 continue;
					}
					else if ((f->subclass == AST_CONTROL_BUSY)  ||  (f->subclass == AST_CONTROL_CONGESTION))
					{
						ast_log(LOG_DEBUG, "faxGw - AST_CONTROL_BUSY\n");
						/* busy */
						 state = f->subclass;
						 s->chan->hangupcause = s->peer->hangupcause;
						 ast_frfree(f);
						 break;
					}
					else if (f->subclass == AST_CONTROL_ANSWER)
					{
						ast_log(LOG_DEBUG, "faxGw - AST_CONTROL_ANSWER\n");
						/* This is what we are hoping for :-) */
						state = f->subclass;
						ast_frfree(f);
						ready = 1;
						break;
					}
					/* else who cares :-) */
				}
			}
			else
			{
				ast_log(LOG_DEBUG, "faxGw - something happend on channel\n");
				/* orig channel reports something */
				f = ast_read(active);
				if (f == NULL)
				{
					/* initiator hanged */
					state = AST_CONTROL_HANGUP;
					ast_log(LOG_DEBUG, "Hangup from remote channel\n");
					res = 0;
					break;
				}
				if (f->frametype == AST_FRAME_CONTROL)
				{
					if (f->subclass == AST_CONTROL_HANGUP)
					{
						state = f->subclass;
						res = 0;
						ast_frfree(f);
						break;
					}
				}
			}
			ast_frfree(f);
		}
		else
		{
			ast_log(LOG_DEBUG, "Timeout while waiting\n");
		}
	}
	
	/* SS7 channel doesn't return control packets */
	if (strncmp(s->peer->tech->type, "SS7", 3) == 0)
	{
		ready = 1;
	}
	
		
	res = 1;
	int res2 = ready_to_talk(s->chan, s->peer);
	ast_log(LOG_DEBUG, "faxGw - connections build - ready %d and erady to talk - %d\n", ready, res2);
	
	if (ready  &&  res2)
	{

		ast_log(LOG_DEBUG, "faxGw - ready to do gatewaing \n");

		if (!ast_channel_make_compatible(s->chan, s->peer))
		{
			/* pick up originating call */
			if (s->chan->_state != AST_STATE_UP) {
				/* Shouldn't need this, but checking to see if channel is already answered
				 * Theoretically asterisk should already have answered before running the app */
				// res = ast_answer(s->chan);
				res = __ast_answer(s->chan, 0); 
				if (res) {
					ast_log(LOG_WARNING, "Could not answer channel '%s'\n", s->chan->name);
					return res;
				}
				ast_log(LOG_NOTICE, "faxGw - original call answered \n");
			}

			s->peer->appl = app_faxgw_name;
			
			ast_set_callerid(s->peer, s->chan->cid.cid_name, s->chan->cid.cid_num, s->chan->cid.cid_num);
			s->chan->hangupcause = AST_CAUSE_NORMAL_CLEARING;
			
			res = RUNNING;
			s->chan_t38state = ast_channel_get_t38_state(s->chan);
			s->peer_t38state = ast_channel_get_t38_state(s->peer);
			
			while ( res == RUNNING ) 
			{
				s->chan_t38state = ast_channel_get_t38_state(s->chan);
				s->peer_t38state = ast_channel_get_t38_state(s->peer);

				if ( res && (s->chan_t38state == s->peer_t38state))
				{
					// Same on both sides, so just bridge 
					ast_log(LOG_NOTICE, "Bridging frames [ %d,%d]\n", s->chan_t38state, s->peer_t38state);
					res = ast_bridge_frames(s);
				}
				
				if (
					( res == RUNNING )
					&& ( ( s->chan_t38state == T38_STATE_UNKNOWN ) || ( s->peer_t38state == T38_STATE_UNKNOWN ) || 
					     ( s->chan_t38state == T38_STATE_UNAVAILABLE ) || ( s->peer_t38state == T38_STATE_UNAVAILABLE ))
					&& ( s->chan_t38state != s->peer_t38state ) 
				)
				{
					// Different on each side, so gateway 
					ast_log(LOG_NOTICE, "Doing T.38 gateway [ %d,%d]\n", s->chan_t38state, s->peer_t38state);
					res = ast_t38_gateway(s);
				}
				// ast_log(LOG_NOTICE," res = %d, RUNNING defined as %d, chan_Status [%d,%d] UNKNOWN set to %d ", res, RUNNING, s->chan_t38state, s->peer_t38state, T38_STATE_UNKNOWN  );
			}
		}
		else
		{
			ast_log(LOG_ERROR, "failed to make remote_channel %s/%s Compatible\n", s->dest_type, s->dest);
		}
	}
	else
	{
		ast_log(LOG_ERROR, "failed to get remote_channel %s %s\n", s->dest_type, s->dest);
	}
	
	if (state == AST_CONTROL_ANSWER)
		strncpy(status, "ANSWER", sizeof(status) - 1);
	if (state == AST_CONTROL_BUSY)
		strncpy(status, "BUSY", sizeof(status) - 1);
	if (state == AST_CONTROL_CONGESTION)
		strncpy(status, "CONGESTION", sizeof(status) - 1);
	if (state == AST_CONTROL_HANGUP)
		strncpy(status, "CANCEL", sizeof(status) - 1);
	pbx_builtin_setvar_helper(s->chan, "DIALSTATUS", status);
	
	ast_log(LOG_NOTICE, "FaxGateway exit with %s\n", status);
	if (s->peer)
		ast_hangup(s->peer);

	if (res) {
		ast_log(LOG_WARNING, "Transmission error\n");
		pbx_builtin_setvar_helper(s->chan, "FAXSTATUS", "FAILED"); 
		pbx_builtin_setvar_helper(s->chan, "FAXERROR", "Transmission error"); 
		res = -1;
	} else if (s->finished < 0) {
		ast_log(LOG_WARNING, "Transmission failed\n");
		pbx_builtin_setvar_helper(s->chan, "FAXSTATUS", "FAILED"); 
		pbx_builtin_setvar_helper(s->chan, "FAXERROR", "Channel problems"); 
	} else if (s->finished > 0) {
		ast_log(LOG_DEBUG, "Transmission finished Ok\n");
		pbx_builtin_setvar_helper(s->chan, "FAXSTATUS", "PASSED"); 
		pbx_builtin_setvar_helper(s->chan, "FAXERROR", "OK"); 
	}

	return res;
}

/* === Application functions === */
static int faxgw_exec(struct ast_channel *chan, void *data)
{
	int res = 0;
	char *parse;
	fax_session session;
	int cause;
	struct cause_args num = { chan, 0, 0, 0 };
	
	session.chan = chan;
	session.timeout = 6000;
	
	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(dest);
		AST_APP_ARG(timeout);
	);

	if (chan == NULL) {
		ast_log(LOG_ERROR, "Fax channel is NULL. Giving up.\n");
		return -1;
	}

	parse = ast_strdupa(data);
	AST_STANDARD_APP_ARGS(args, parse);
	
	ast_channel_lock(chan);
	session.caller_mode = FALSE;
	ast_channel_unlock(chan);
	
	ast_cdr_reset(chan->cdr, NULL);
	
	struct ast_channel *tc; /* channel for this destination */
	/* Get a technology/[device:]number pair */
	char *number = args.dest;
	char *tech = strsep(&number, "/");
	if (!number) {
		ast_log(LOG_WARNING, "Dial argument takes format (technology/[device:]number1)\n");
		return -1;
	}
	char numsubst[256];
	session.dest = number;
	session.dest_type = tech;
	ast_copy_string(numsubst, number, sizeof(numsubst));
	/* Request the peer */
 	tc = ast_request(tech, session.chan->nativeformats, numsubst, &cause);
	if (!tc) 
	{
		/* If we can't, just go on to the next call */
		ast_log(LOG_WARNING, "Unable to create channel of type '%s' (cause %d - %s)\n",	tech, cause, ast_cause2str(cause));
		handle_cause(cause, &num);
		session.chan->hangupcause = cause;
			return -1;
	}
	session.peer = tc;
	pbx_builtin_setvar_helper(tc, "DIALEDPEERNUMBER", numsubst);

	/* Setup outgoing SDP to match incoming one */
	ast_rtp_make_compatible(tc, session.chan, 1);
		
	/* Inherit specially named variables from parent channel */
	ast_channel_inherit_variables(session.chan, tc);
	tc->appl = "AppDial";
	tc->data = "(Outgoing Line)";
	tc->whentohangup.tv_sec = 0;
	tc->whentohangup.tv_usec = 0;

	S_REPLACE(tc->cid.cid_num, ast_strdup(chan->cid.cid_num));
	S_REPLACE(tc->cid.cid_name, ast_strdup(chan->cid.cid_name));
	S_REPLACE(tc->cid.cid_ani, ast_strdup(chan->cid.cid_ani));
	S_REPLACE(tc->cid.cid_rdnis, ast_strdup(chan->cid.cid_rdnis));
		
	/* Copy language from incoming to outgoing */
	ast_string_field_set(tc, language, chan->language);
	ast_string_field_set(tc, accountcode, chan->accountcode);
	tc->cdrflags = chan->cdrflags;
	if (ast_strlen_zero(tc->musicclass))
		ast_string_field_set(tc, musicclass, chan->musicclass);
	/* Pass callingpres, type of number, tns, ADSI CPE, transfer capability */
	tc->cid.cid_pres = chan->cid.cid_pres;
	tc->cid.cid_ton = chan->cid.cid_ton;
	tc->cid.cid_tns = chan->cid.cid_tns;
	tc->cid.cid_ani2 = chan->cid.cid_ani2;
	tc->adsicpe = chan->adsicpe;
	tc->transfercapability = chan->transfercapability;

	/* Inherit context and extension */
	if (!ast_strlen_zero(chan->macrocontext))
		ast_copy_string(tc->dialcontext, chan->macrocontext, sizeof(tc->dialcontext));
	else
		ast_copy_string(tc->dialcontext, chan->context, sizeof(tc->dialcontext));
	if (!ast_strlen_zero(chan->macroexten))
		ast_copy_string(tc->exten, chan->macroexten, sizeof(tc->exten));
	else
		ast_copy_string(tc->exten, chan->exten, sizeof(tc->exten));

//        ast_log(LOG_DEBUG, "chan %p peer %p \n", session.chan, session.peer);
	/* Done parsing */
	session.finished = 0;

	res = transmit(&session);

	return res;
}

static int unload_module(void)
{
	int res;

	res = ast_unregister_application(app_faxgw_name);	

	return res;
}

static int load_module(void)
{
	int res ;

	res = ast_register_application(app_faxgw_name, faxgw_exec, app_faxgw_synopsis, app_faxgw_desc);

	/* The default SPAN message handler prints to stderr. It is something we do not want */
	span_set_message_handler(NULL);

	return res;
}


AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_DEFAULT, "Simple FAX Gateway",
		.load = load_module,
		.unload = unload_module,
		);


