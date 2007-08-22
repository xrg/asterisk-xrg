/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 1999 - 2005, Digium, Inc.
 * and Edvina AB, Sollentuna, Sweden
 *
 * Mark Spencer <markster@digium.com> (Comedian Mail)
 * and Olle E. Johansson, Edvina.net <oej@edvina.net> (Mini-Voicemail changes)
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

/*! \file
 *
 * \brief MiniVoiceMail - A Minimal Voicemail System for Asterisk
 *
 * A voicemail system in small building blocks, working together
 * based on the Comedian Mail voicemail system (app_voicemail.c).
 * 
 * \par See also
 * \arg \ref Config_minivm
 * \arg \ref Config_minivm_examples
 * \arg \ref App_minivm
 *
 * \ingroup applications
 *
 * \page App_minivm	Asterisk Mini-voicemail - A minimal voicemail system
 *	
 *	This is a minimal voicemail system, building blocks for something
 *	else. It is built for multi-language systems.
 *	The current version is focused on accounts where voicemail is 
 *	forwarded to users in e-mail. It's work in progress, with loosed ends hanging
 *	around from the old voicemail system and it's configuration.
 *
 *	Hopefully, we can expand this to be a full replacement of voicemail() and voicemailmain()
 *	in the future.
 *
 *	Dialplan applications
 *	- minivmRecord - record voicemail and send as e-mail ( \ref minivm_record_exec() )
 *	- minivmGreet - Play user's greeting or default greeting ( \ref minivm_greet_exec() )
 *	- minivmNotify - Notify user of message ( \ref minivm_notify_exec() )
 * 	- minivmDelete - Delete voicemail message ( \ref minivm_delete_exec() )
 *	- minivmAccMess - Record personal messages (busy | unavailable | temporary)
 *
 *	Dialplan functions
 *	- MINIVMACCOUNT() - A dialplan function
 *	- MINIVMCOUNTER() - Manage voicemail-related counters for accounts or domains
 *
 *	CLI Commands
 *	- minivm list accounts
 *	- minivm list zones
 *	- minivm list templates
 *	- minivm show stats
 *	- minivm show settings
 *
 *	Some notes
 *	- General configuration in minivm.conf
 *	- Users in realtime or configuration file
 *	- Or configured on the command line with just the e-mail address
 *		
 *	Voicemail accounts are identified by userid and domain
 *
 *	Language codes are like setlocale - langcode_countrycode
 *	\note Don't use language codes like the rest of Asterisk, two letter countrycode. Use
 *	language_country like setlocale(). 
 *	
 *	Examples:
 *		- Swedish, Sweden	sv_se
 *		- Swedish, Finland	sv_fi
 *		- English, USA		en_us
 *		- English, GB		en_gb
 *	
 * \par See also
 * \arg \ref Config_minivm
 * \arg \ref Config_minivm_examples
 * \arg \ref Minivm_directories
 * \arg \ref app_minivm.c
 * \arg Comedian mail: app_voicemail.c
 * \arg \ref descrip_minivm_accmess
 * \arg \ref descrip_minivm_greet
 * \arg \ref descrip_minivm_record
 * \arg \ref descrip_minivm_delete
 * \arg \ref descrip_minivm_notify
 *
 * \arg \ref App_minivm_todo
 */
/*! \page Minivm_directories Asterisk Mini-Voicemail Directory structure
 *
 *	The directory structure for storing voicemail
 *		- AST_SPOOL_DIR - usually /var/spool/asterisk (configurable in asterisk.conf)
 *		- MVM_SPOOL_DIR - should be configurable, usually AST_SPOOL_DIR/voicemail
 *		- Domain	MVM_SPOOL_DIR/domain
 *		- Username	MVM_SPOOL_DIR/domain/username
 *			- /greet	: Recording of account owner's name
 *			- /busy		: Busy message
 *			- /unavailable  : Unavailable message
 *			- /temp		: Temporary message
 *
 *	For account anita@localdomain.xx the account directory would as a default be
 *		\b /var/spool/asterisk/voicemail/localdomain.xx/anita
 *
 *	To avoid transcoding, these sound files should be converted into several formats
 *	They are recorded in the format closest to the incoming streams
 *
 *
 * Back: \ref App_minivm
 */

/*! \page Config_minivm_examples Example dialplan for Mini-Voicemail
 * \section Example dialplan scripts for Mini-Voicemail
 *  \verbinclude extensions_minivm.conf.sample
 *
 * Back: \ref App_minivm
 */

/*! \page App_minivm_todo Asterisk Mini-Voicemail - todo
 *	- configure accounts from AMI?
 *	- test, test, test, test
 *	- fix "vm-theextensionis.gsm" voiceprompt from Allison in various formats
 *		"The extension you are calling"
 *	- For trunk, consider using channel storage for information passing between small applications
 *	- Set default directory for voicemail
 *	- New app for creating directory for account if it does not exist
 *	- Re-insert code for IMAP storage at some point
 *	- Jabber integration for notifications
 *	- Figure out how to handle video in voicemail
 *	- Integration with the HTTP server
 *	- New app for moving messages between mailboxes, and optionally mark it as "new"
 *
 *	For Asterisk 1.4/trunk
 *	- Use string fields for minivm_account
 *
 * Back: \ref App_minivm
 */

#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <time.h>
#include <dirent.h>
#include <locale.h>

#include "asterisk.h"

ASTERISK_FILE_VERSION(__FILE__, "$Revision$")

#include "asterisk/astobj.h"
#include "asterisk/lock.h"
#include "asterisk/file.h"
#include "asterisk/logger.h"
#include "asterisk/channel.h"
#include "asterisk/pbx.h"
#include "asterisk/options.h"
#include "asterisk/config.h"
#include "asterisk/say.h"
#include "asterisk/module.h"
#include "asterisk/app.h"
#include "asterisk/manager.h"
#include "asterisk/dsp.h"
#include "asterisk/localtime.h"
#include "asterisk/cli.h"
#include "asterisk/utils.h"
#include "asterisk/linkedlists.h"
#include "asterisk/callerid.h"

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif


#define MVM_REVIEW		(1 << 0)	/*!< Review message */
#define MVM_OPERATOR		(1 << 1)	/*!< Operator exit during voicemail recording */
#define MVM_REALTIME		(1 << 2)	/*!< This user is a realtime account */
#define MVM_SVMAIL		(1 << 3)
#define MVM_ENVELOPE		(1 << 4)
#define MVM_PBXSKIP		(1 << 9)
#define MVM_ALLOCED		(1 << 13)

/*! \brief Default mail command to mail voicemail. Change it with the
    mailcmd= command in voicemail.conf */
#define SENDMAIL "/usr/sbin/sendmail -t"

#define SOUND_INTRO		"vm-intro"
#define B64_BASEMAXINLINE 	256	/*!< Buffer size for Base 64 attachment encoding */
#define B64_BASELINELEN 	72	/*!< Line length for Base 64 endoded messages */
#define EOL			"\r\n"

#define MAX_DATETIME_FORMAT	512
#define MAX_NUM_CID_CONTEXTS	10

#define ERROR_LOCK_PATH		-100
#define	VOICEMAIL_DIR_MODE	0700

#define VOICEMAIL_CONFIG "minivm.conf"
#define ASTERISK_USERNAME "asterisk"	/*!< Default username for sending mail is asterisk\@localhost */

/*! \brief Message types for notification */
enum mvm_messagetype {
	MVM_MESSAGE_EMAIL,
	MVM_MESSAGE_PAGE
	/* For trunk: MVM_MESSAGE_JABBER, */
};

static char MVM_SPOOL_DIR[PATH_MAX];

/* Module declarations */
static char *app_minivm_record = "MinivmRecord"; 	/* Leave a message */
static char *app_minivm_greet = "MinivmGreet";		/* Play voicemail prompts */
static char *app_minivm_notify = "MinivmNotify";	/* Notify about voicemail by using one of several methods */
static char *app_minivm_delete = "MinivmDelete";	/* Notify about voicemail by using one of several methods */
static char *app_minivm_accmess = "MinivmAccMess";	/* Record personal voicemail messages */

static char *synopsis_minivm_record = "Receive Mini-Voicemail and forward via e-mail";
static char *descrip_minivm_record = 
	"Syntax: MinivmRecord(username@domain[,options])\n"
	"This application is part of the Mini-Voicemail system, configured in minivm.conf.\n"
	"MiniVM records audio file in configured format and forwards message to e-mail and pager.\n"
	"If there's no user account for that address, a temporary account will\n"
	"be used with default options.\n"
	"The recorded file name and path will be stored in MINIVM_FILENAME and the \n"
	"duration of the message will be stored in MINIVM_DURATION\n"
	"\nNote: If the caller hangs up after the recording, the only way to send\n"
	"the message and clean up is to execute in the \"h\" extension.\n"
	"\nThe application will exit if any of the following DTMF digits are \n"
	"received and the requested extension exist in the current context.\n"
	"    0 - Jump to the 'o' extension in the current dialplan context.\n"
	"    * - Jump to the 'a' extension in the current dialplan context.\n"
	"\n"
	"Result is given in channel variable MINIVM_RECORD_STATUS\n"
	"        The possible values are:     SUCCESS | USEREXIT | FAILED\n\n"
	"  Options:\n"
	"    g(#) - Use the specified amount of gain when recording the voicemail\n"
	"           message. The units are whole-number decibels (dB).\n"
	"\n";

static char *synopsis_minivm_greet = "Play Mini-Voicemail prompts";
static char *descrip_minivm_greet = 
	"Syntax: MinivmGreet(username@domain[,options])\n"
	"This application is part of the Mini-Voicemail system, configured in minivm.conf.\n"
	"MinivmGreet() plays default prompts or user specific prompts for an account.\n"
	"Busy and unavailable messages can be choosen, but will be overridden if a temporary\n"
	"message exists for the account.\n"
	"\n"
	"Result is given in channel variable MINIVM_GREET_STATUS\n"
	"        The possible values are:     SUCCESS | USEREXIT | FAILED\n\n"
	"  Options:\n"
	"    b    - Play the 'busy' greeting to the calling party.\n"
	"    s    - Skip the playback of instructions for leaving a message to the\n"
	"           calling party.\n"
	"    u    - Play the 'unavailable greeting.\n"
	"\n";

static char *synopsis_minivm_notify = "Notify voicemail owner about new messages.";
static char *descrip_minivm_notify = 
	"Syntax: MinivmNotify(username@domain[,template])\n"
	"This application is part of the Mini-Voicemail system, configured in minivm.conf.\n"
	"MiniVMnotify forwards messages about new voicemail to e-mail and pager.\n"
	"If there's no user account for that address, a temporary account will\n"
	"be used with default options (set in minivm.conf).\n"
	"The recorded file name and path will be read from MVM_FILENAME and the \n"
	"duration of the message will be accessed from MVM_DURATION (set by MinivmRecord() )\n"
	"If the channel variable MVM_COUNTER is set, this will be used in the\n"
	"message file name and available in the template for the message.\n"
	"If not template is given, the default email template will be used to send email and\n"
	"default pager template to send paging message (if the user account is configured with\n"
	"a paging address.\n"
	"\n"
	"Result is given in channel variable MINIVM_NOTIFY_STATUS\n"
	"        The possible values are:     SUCCESS | FAILED\n"
	"\n";

static char *synopsis_minivm_delete = "Delete Mini-Voicemail voicemail messages";
static char *descrip_minivm_delete = 
	"Syntax: MinivmDelete(filename)\n"
	"This application is part of the Mini-Voicemail system, configured in minivm.conf.\n"
	"It deletes voicemail file set in MVM_FILENAME or given filename.\n"
	"\n"
	"Result is given in channel variable MINIVM_DELETE_STATUS\n"
	"        The possible values are:     SUCCESS |  FAILED\n"
	"	 FAILED is set if the file does not exist or can't be deleted.\n"
	"\n";

static char *synopsis_minivm_accmess = "Record account specific messages";
static char *descrip_minivm_accmess = 
	"Syntax: MinivmAccmess(username@domain,option)\n"
	"This application is part of the Mini-Voicemail system, configured in minivm.conf.\n"
	"Use this application to record account specific audio/video messages for\n"
	"busy, unavailable and temporary messages.\n"
	"Account specific directories will be created if they do not exist.\n"
	"\nThe option selects message to be recorded:\n"
	"   u      Unavailable\n"
	"   b      Busy\n"
	"   t      Temporary (overrides busy and unavailable)\n"
	"   n      Account name\n"
	"\n"
	"Result is given in channel variable MINIVM_ACCMESS_STATUS\n"
	"        The possible values are:     SUCCESS |  FAILED\n"
	"	 FAILED is set if the file can't be created.\n"
	"\n";

enum {
	OPT_SILENT =	   (1 << 0),
	OPT_BUSY_GREETING =    (1 << 1),
	OPT_UNAVAIL_GREETING = (1 << 2),
	OPT_TEMP_GREETING = (1 << 3),
	OPT_NAME_GREETING = (1 << 4),
	OPT_RECORDGAIN =  (1 << 5),
} minivm_option_flags;

enum {
	OPT_ARG_RECORDGAIN = 0,
	OPT_ARG_ARRAY_SIZE = 1,
} minivm_option_args;

AST_APP_OPTIONS(minivm_app_options, {
	AST_APP_OPTION('s', OPT_SILENT),
	AST_APP_OPTION('b', OPT_BUSY_GREETING),
	AST_APP_OPTION('u', OPT_UNAVAIL_GREETING),
	AST_APP_OPTION_ARG('g', OPT_RECORDGAIN, OPT_ARG_RECORDGAIN),
});

AST_APP_OPTIONS(minivm_accmess_options, {
	AST_APP_OPTION('b', OPT_BUSY_GREETING),
	AST_APP_OPTION('u', OPT_UNAVAIL_GREETING),
	AST_APP_OPTION('t', OPT_TEMP_GREETING),
	AST_APP_OPTION('n', OPT_NAME_GREETING),
});

/*! \brief Structure for linked list of Mini-Voicemail users: \ref minivm_accounts */
struct minivm_account {
	char username[AST_MAX_CONTEXT];	/*!< Mailbox username */
	char domain[AST_MAX_CONTEXT];	/*!< Voicemail domain */
	
	char pincode[10];		/*!< Secret pin code, numbers only */
	char fullname[120];		/*!< Full name, for directory app */
	char email[80];			/*!< E-mail address - override */
	char pager[80];			/*!< E-mail address to pager (no attachment) */
	char accountcode[AST_MAX_ACCOUNT_CODE];	/*!< Voicemail account account code */
	char serveremail[80];		/*!< From: Mail address */
	char externnotify[160];		/*!< Configurable notification command */
	char language[MAX_LANGUAGE];    /*!< Config: Language setting */
	char zonetag[80];		/*!< Time zone */
	char uniqueid[20];		/*!< Unique integer identifier */
	char exit[80];			/*!< Options for exiting from voicemail() */
	char attachfmt[80];		/*!< Format for voicemail audio file attachment */
	char etemplate[80];		/*!< Pager template */
	char ptemplate[80];		/*!< Voicemail format */
	unsigned int flags;		/*!< MVM_ flags */	
	struct ast_variable *chanvars;	/*!< Variables for e-mail template */
	double volgain;			/*!< Volume gain for voicemails sent via e-mail */
	AST_LIST_ENTRY(minivm_account) list;	
};

/*! \brief The list of e-mail accounts */
static AST_LIST_HEAD_STATIC(minivm_accounts, minivm_account);

/*! \brief Linked list of e-mail templates in various languages 
	These are used as templates for e-mails, pager messages and jabber messages
	\ref message_templates
*/
struct minivm_template {
	char	name[80];		/*!< Template name */
	char	*body;			/*!< Body of this template */
	char	fromaddress[100];	/*!< Who's sending the e-mail? */
	char	serveremail[80];	/*!< From: Mail address */
	char	subject[100];		/*!< Subject line */
	char	charset[32];		/*!< Default character set for this template */
	char	locale[20];		/*!< Locale for setlocale() */
	char	dateformat[80];		/*!< Date format to use in this attachment */
	int	attachment;		/*!< Attachment of media yes/no - no for pager messages */
	AST_LIST_ENTRY(minivm_template) list;	/*!< List mechanics */
};

/*! \brief The list of e-mail templates */
static AST_LIST_HEAD_STATIC(message_templates, minivm_template);

/*! \brief Options for leaving voicemail with the voicemail() application */
struct leave_vm_options {
	unsigned int flags;
	signed char record_gain;
};

/*! \brief Structure for base64 encoding */
struct b64_baseio {
	int iocp;
	int iolen;
	int linelength;
	int ateof;
	unsigned char iobuf[B64_BASEMAXINLINE];
};

/*! \brief Voicemail time zones */
struct minivm_zone {
	char name[80];				/*!< Name of this time zone */
	char timezone[80];			/*!< Timezone definition */
	char msg_format[BUFSIZ];		/*!< Not used in minivm ...yet */
	AST_LIST_ENTRY(minivm_zone) list;	/*!< List mechanics */
};

/*! \brief The list of e-mail time zones */
static AST_LIST_HEAD_STATIC(minivm_zones, minivm_zone);

/*! \brief Structure for gathering statistics */
struct minivm_stats {
	int voicemailaccounts;		/*!< Number of static accounts */
	int timezones;			/*!< Number of time zones */
	int templates;			/*!< Number of templates */

	struct timeval reset;			/*!< Time for last reset */
	int receivedmessages;		/*!< Number of received messages since reset */
	struct timeval lastreceived;		/*!< Time for last voicemail sent */
};

/*! \brief Statistics for voicemail */
static struct minivm_stats global_stats;

AST_MUTEX_DEFINE_STATIC(minivmlock);	/*!< Lock to protect voicemail system */
AST_MUTEX_DEFINE_STATIC(minivmloglock);	/*!< Lock to protect voicemail system log file */

FILE *minivmlogfile;			/*!< The minivm log file */

static int global_vmminmessage;		/*!< Minimum duration of messages */
static int global_vmmaxmessage;		/*!< Maximum duration of message */
static int global_maxsilence;		/*!< Maximum silence during recording */
static int global_maxgreet;		/*!< Maximum length of prompts  */
static int global_silencethreshold = 128;
static char global_mailcmd[160];	/*!< Configurable mail cmd */
static char global_externnotify[160]; 	/*!< External notification application */
static char global_logfile[PATH_MAX];	/*!< Global log file for messages */
static char default_vmformat[80];

static struct ast_flags globalflags = {0};	/*!< Global voicemail flags */
static int global_saydurationminfo;
static char global_charset[32];			/*!< Global charset in messages */

static double global_volgain;	/*!< Volume gain for voicmemail via e-mail */

/*! \brief Default dateformat, can be overridden in configuration file */
#define DEFAULT_DATEFORMAT 	"%A, %B %d, %Y at %r"
#define DEFAULT_CHARSET		"ISO-8859-1"

/* Forward declarations */
static char *message_template_parse_filebody(char *filename);
static char *message_template_parse_emailbody(const char *body);
static int create_vmaccount(char *name, struct ast_variable *var, int realtime);
static struct minivm_account *find_user_realtime(const char *domain, const char *username);
static int handle_minivm_reload(int fd, int argc, char *argv[]);

/*! \brief Create message template */
static struct minivm_template *message_template_create(const char *name)
{
	struct minivm_template *template;

	template = ast_calloc(1, sizeof(*template));
	if (!template)
		return NULL;

	/* Set some defaults for templates */
	ast_copy_string(template->name, name, sizeof(template->name));
	ast_copy_string(template->dateformat, DEFAULT_DATEFORMAT, sizeof(template->dateformat));
	ast_copy_string(template->charset, DEFAULT_CHARSET, sizeof(template->charset));
	ast_copy_string(template->subject, "New message in mailbox ${MVM_USERNAME}@${MVM_DOMAIN}", sizeof(template->subject));
	template->attachment = TRUE;

	return template;
}

/*! \brief Release memory allocated by message template */
static void message_template_free(struct minivm_template *template)
{
	if (template->body)
		ast_free(template->body);

	ast_free (template);
}

/*! \brief Build message template from configuration */
static int message_template_build(const char *name, struct ast_variable *var)
{
	struct minivm_template *template;
	int error = 0;

	template = message_template_create(name);
	if (!template) {
		ast_log(LOG_ERROR, "Out of memory, can't allocate message template object %s.\n", name);
		return -1;
	}

	while (var) {
		ast_debug(3, "-_-_- Configuring template option %s = \"%s\" for template %s\n", var->name, var->value, name);
		if (!strcasecmp(var->name, "fromaddress")) {
			ast_copy_string(template->fromaddress, var->value, sizeof(template->fromaddress));
		} else if (!strcasecmp(var->name, "fromemail")) {
			ast_copy_string(template->serveremail, var->value, sizeof(template->serveremail));
		} else if (!strcasecmp(var->name, "subject")) {
			ast_copy_string(template->subject, var->value, sizeof(template->subject));
		} else if (!strcasecmp(var->name, "locale")) {
			ast_copy_string(template->locale, var->value, sizeof(template->locale));
		} else if (!strcasecmp(var->name, "attachmedia")) {
			template->attachment = ast_true(var->value);
		} else if (!strcasecmp(var->name, "dateformat")) {
			ast_copy_string(template->dateformat, var->value, sizeof(template->dateformat));
		} else if (!strcasecmp(var->name, "charset")) {
			ast_copy_string(template->charset, var->value, sizeof(template->charset));
		} else if (!strcasecmp(var->name, "templatefile")) {
			if (template->body) 
				ast_free(template->body);
			template->body = message_template_parse_filebody(var->value);
			if (!template->body) {
				ast_log(LOG_ERROR, "Error reading message body definition file %s\n", var->value);
				error++;
			}
		} else if (!strcasecmp(var->name, "messagebody")) {
			if (template->body) 
				ast_free(template->body);
			template->body = message_template_parse_emailbody(var->value);
			if (!template->body) {
				ast_log(LOG_ERROR, "Error parsing message body definition:\n          %s\n", var->value);
				error++;
			}
		} else {
			ast_log(LOG_ERROR, "Unknown message template configuration option \"%s=%s\"\n", var->name, var->value);
			error++;
		}
		var = var->next;
	}
	if (error)
		ast_log(LOG_ERROR, "-- %d errors found parsing message template definition %s\n", error, name);

	AST_LIST_LOCK(&message_templates);
	AST_LIST_INSERT_TAIL(&message_templates, template, list);
	AST_LIST_UNLOCK(&message_templates);

	global_stats.templates++;

	return error;
}

/*! \brief Find named template */
static struct minivm_template *message_template_find(const char *name)
{
	struct minivm_template *this, *res = NULL;

	if (ast_strlen_zero(name))
		return NULL;

	AST_LIST_LOCK(&message_templates);
	AST_LIST_TRAVERSE(&message_templates, this, list) {
		if (!strcasecmp(this->name, name)) {
			res = this;
			break;
		}
	}
	AST_LIST_UNLOCK(&message_templates);

	return res;
}


/*! \brief Clear list of templates */
static void message_destroy_list(void)
{
	struct minivm_template *this;
	AST_LIST_LOCK(&message_templates);
	while ((this = AST_LIST_REMOVE_HEAD(&message_templates, list))) 
		message_template_free(this);
		
	AST_LIST_UNLOCK(&message_templates);
}

/*! \brief read buffer from file (base64 conversion) */
static int b64_inbuf(struct b64_baseio *bio, FILE *fi)
{
	int l;

	if (bio->ateof)
		return 0;

	if ((l = fread(bio->iobuf, 1, B64_BASEMAXINLINE,fi)) <= 0) {
		if (ferror(fi))
			return -1;

		bio->ateof = 1;
		return 0;
	}

	bio->iolen= l;
	bio->iocp= 0;

	return 1;
}

/*! \brief read character from file to buffer (base64 conversion) */
static int b64_inchar(struct b64_baseio *bio, FILE *fi)
{
	if (bio->iocp >= bio->iolen) {
		if (!b64_inbuf(bio, fi))
			return EOF;
	}

	return bio->iobuf[bio->iocp++];
}

/*! \brief write buffer to file (base64 conversion) */
static int b64_ochar(struct b64_baseio *bio, int c, FILE *so)
{
	if (bio->linelength >= B64_BASELINELEN) {
		if (fputs(EOL,so) == EOF)
			return -1;

		bio->linelength= 0;
	}

	if (putc(((unsigned char) c), so) == EOF)
		return -1;

	bio->linelength++;

	return 1;
}

/*! \brief Encode file to base64 encoding for email attachment (base64 conversion) */
static int base_encode(char *filename, FILE *so)
{
	unsigned char dtable[B64_BASEMAXINLINE];
	int i,hiteof= 0;
	FILE *fi;
	struct b64_baseio bio;

	memset(&bio, 0, sizeof(bio));
	bio.iocp = B64_BASEMAXINLINE;

	if (!(fi = fopen(filename, "rb"))) {
		ast_log(LOG_WARNING, "Failed to open file: %s: %s\n", filename, strerror(errno));
		return -1;
	}

	for (i= 0; i<9; i++) {
		dtable[i]= 'A'+i;
		dtable[i+9]= 'J'+i;
		dtable[26+i]= 'a'+i;
		dtable[26+i+9]= 'j'+i;
	}
	for (i= 0; i < 8; i++) {
		dtable[i+18]= 'S'+i;
		dtable[26+i+18]= 's'+i;
	}
	for (i= 0; i < 10; i++) {
		dtable[52+i]= '0'+i;
	}
	dtable[62]= '+';
	dtable[63]= '/';

	while (!hiteof){
		unsigned char igroup[3], ogroup[4];
		int c,n;

		igroup[0]= igroup[1]= igroup[2]= 0;

		for (n= 0; n < 3; n++) {
			if ((c = b64_inchar(&bio, fi)) == EOF) {
				hiteof= 1;
				break;
			}
			igroup[n]= (unsigned char)c;
		}

		if (n> 0) {
			ogroup[0]= dtable[igroup[0]>>2];
			ogroup[1]= dtable[((igroup[0]&3)<<4) | (igroup[1]>>4)];
			ogroup[2]= dtable[((igroup[1]&0xF)<<2) | (igroup[2]>>6)];
			ogroup[3]= dtable[igroup[2]&0x3F];

			if (n<3) {
				ogroup[3]= '=';

				if (n<2)
					ogroup[2]= '=';
			}

			for (i= 0;i<4;i++)
				b64_ochar(&bio, ogroup[i], so);
		}
	}

	/* Put end of line - line feed */
	if (fputs(EOL, so) == EOF)
		return 0;

	fclose(fi);

	return 1;
}

static int get_date(char *s, int len)
{
	struct ast_tm tm;
	struct timeval tv = ast_tvnow();

	ast_localtime(&tv, &tm, NULL);
	return ast_strftime(s, len, "%a %b %e %r %Z %Y", &tm);
}


/*! \brief Free user structure - if it's allocated */
static void free_user(struct minivm_account *vmu)
{
	if (vmu->chanvars)
		ast_variables_destroy(vmu->chanvars);
	ast_free(vmu);
}



/*! \brief Prepare for voicemail template by adding channel variables 
	to the channel
*/
static void prep_email_sub_vars(struct ast_channel *channel, const struct minivm_account *vmu, const char *cidnum, const char *cidname, const char *dur, const char *date, const char *counter)
{
	char callerid[256];
	struct ast_variable *var;
	
	if (!channel) {
		ast_log(LOG_ERROR, "No allocated channel, giving up...\n");
		return;
	}

	for (var = vmu->chanvars ; var ; var = var->next)
                pbx_builtin_setvar_helper(channel, var->name, var->value);

	/* Prepare variables for substition in email body and subject */
	pbx_builtin_setvar_helper(channel, "MVM_NAME", vmu->fullname);
	pbx_builtin_setvar_helper(channel, "MVM_DUR", dur);
	pbx_builtin_setvar_helper(channel, "MVM_DOMAIN", vmu->domain);
	pbx_builtin_setvar_helper(channel, "MVM_USERNAME", vmu->username);
	pbx_builtin_setvar_helper(channel, "MVM_CALLERID", ast_callerid_merge(callerid, sizeof(callerid), cidname, cidnum, "Unknown Caller"));
	pbx_builtin_setvar_helper(channel, "MVM_CIDNAME", (cidname ? cidname : "an unknown caller"));
	pbx_builtin_setvar_helper(channel, "MVM_CIDNUM", (cidnum ? cidnum : "an unknown caller"));
	pbx_builtin_setvar_helper(channel, "MVM_DATE", date);
	if (!ast_strlen_zero(counter))
		pbx_builtin_setvar_helper(channel, "MVM_COUNTER", counter);
}

/*! \brief Set default values for Mini-Voicemail users */
static void populate_defaults(struct minivm_account *vmu)
{
	ast_copy_flags(vmu, (&globalflags), AST_FLAGS_ALL);	
	ast_copy_string(vmu->attachfmt, default_vmformat, sizeof(vmu->attachfmt));
	vmu->volgain = global_volgain;
}

/*! \brief Fix quote of mail headers for non-ascii characters */
static char *mailheader_quote(const char *from, char *to, size_t len)
{
	char *ptr = to;
	*ptr++ = '"';
	for (; ptr < to + len - 1; from++) {
		if (*from == '"')
			*ptr++ = '\\';
		else if (*from == '\0')
			break;
		*ptr++ = *from;
	}
	if (ptr < to + len - 1)
		*ptr++ = '"';
	*ptr = '\0';
	return to;
}


/*! \brief Allocate new vm user and set default values */
static struct minivm_account *mvm_user_alloc(void)
{
	struct minivm_account *new;

	new = ast_calloc(1, sizeof(*new));
	if (!new)
		return NULL;
	populate_defaults(new);

	return new;
}


/*! \brief Clear list of users */
static void vmaccounts_destroy_list(void)
{
	struct minivm_account *this;
	AST_LIST_LOCK(&minivm_accounts);
	while ((this = AST_LIST_REMOVE_HEAD(&minivm_accounts, list))) 
		ast_free(this);
	AST_LIST_UNLOCK(&minivm_accounts);
}


/*! \brief Find user from static memory object list */
static struct minivm_account *find_account(const char *domain, const char *username, int createtemp)
{
	struct minivm_account *vmu = NULL, *cur;


	if (ast_strlen_zero(domain) || ast_strlen_zero(username)) {
		ast_log(LOG_NOTICE, "No username or domain? \n");
		return NULL;
	}
	ast_debug(3, "-_-_-_- Looking for voicemail user %s in domain %s\n", username, domain);

	AST_LIST_LOCK(&minivm_accounts);
	AST_LIST_TRAVERSE(&minivm_accounts, cur, list) {
		/* Is this the voicemail account we're looking for? */
		if (!strcasecmp(domain, cur->domain) && !strcasecmp(username, cur->username))
			break;
	}
	AST_LIST_UNLOCK(&minivm_accounts);

	if (cur) {
		ast_debug(3, "-_-_- Found account for %s@%s\n", username, domain);
		vmu = cur;

	} else
		vmu = find_user_realtime(domain, username);

	if (createtemp && !vmu) {
		/* Create a temporary user, send e-mail and be gone */
		vmu = mvm_user_alloc();
		ast_set2_flag(vmu, TRUE, MVM_ALLOCED);	
		if (vmu) {
			ast_copy_string(vmu->username, username, sizeof(vmu->username));
			ast_copy_string(vmu->domain, domain, sizeof(vmu->domain));
			ast_debug(1, "--- Created temporary account\n");
		}

	}
	return vmu;
}

/*! \brief Find user in realtime storage 
	Returns pointer to minivm_account structure
*/
static struct minivm_account *find_user_realtime(const char *domain, const char *username)
{
	struct ast_variable *var;
	struct minivm_account *retval;
	char name[MAXHOSTNAMELEN];

	retval = mvm_user_alloc();
	if (!retval)
		return NULL;

	if (username) 
		ast_copy_string(retval->username, username, sizeof(retval->username));

	populate_defaults(retval);
	var = ast_load_realtime("minivm", "username", username, "domain", domain, NULL);

	if (!var) {
		ast_free(retval);
		return NULL;
	}

	snprintf(name, sizeof(name), "%s@%s", username, domain);
	create_vmaccount(name, var, TRUE);

	ast_variables_destroy(var);
	return retval;
}

/*! \brief Send voicemail with audio file as an attachment */
static int sendmail(struct minivm_template *template, struct minivm_account *vmu, char *cidnum, char *cidname, const char *filename, char *format, int duration, int attach_user_voicemail, enum mvm_messagetype type, const char *counter)
{
	FILE *p = NULL;
	int pfd;
	char email[256] = "";
	char who[256] = "";
	char date[256];
	char bound[256];
	char fname[PATH_MAX];
	char dur[PATH_MAX];
	char tmp[80] = "/tmp/astmail-XXXXXX";
	char tmp2[PATH_MAX];
	struct timeval now;
	struct ast_tm tm;
	struct minivm_zone *the_zone = NULL;
	int len_passdata;
	struct ast_channel *ast;
	char *finalfilename;
	char *passdata = NULL;
	char *passdata2 = NULL;
	char *fromaddress;
	char *fromemail;

	if (type == MVM_MESSAGE_EMAIL) {
		if (vmu && !ast_strlen_zero(vmu->email)) {
			ast_copy_string(email, vmu->email, sizeof(email));	
		} else if (!ast_strlen_zero(vmu->username) && !ast_strlen_zero(vmu->domain))
			snprintf(email, sizeof(email), "%s@%s", vmu->username, vmu->domain);
	} else if (type == MVM_MESSAGE_PAGE) {
		ast_copy_string(email, vmu->pager, sizeof(email));
	}

	if (ast_strlen_zero(email)) {
		ast_log(LOG_WARNING, "No address to send message to.\n");
		return -1;	
	}

	ast_debug(3, "-_-_- Sending mail to %s@%s - Using template %s\n", vmu->username, vmu->domain, template->name);

	if (!strcmp(format, "wav49"))
		format = "WAV";


	/* If we have a gain option, process it now with sox */
	if (type == MVM_MESSAGE_EMAIL && (vmu->volgain < -.001 || vmu->volgain > .001) ) {
		char newtmp[PATH_MAX];
		char tmpcmd[PATH_MAX];
		int tmpfd;

		snprintf(newtmp, sizeof(newtmp), "/tmp/XXXXXX");
		ast_debug(3, "newtmp: %s\n", newtmp);
		tmpfd = mkstemp(newtmp);
		snprintf(tmpcmd, sizeof(tmpcmd), "sox -v %.4f %s.%s %s.%s", vmu->volgain, filename, format, newtmp, format);
		ast_safe_system(tmpcmd);
		finalfilename = newtmp;
		ast_debug(3, "-- VOLGAIN: Stored at: %s.%s - Level: %.4f - Mailbox: %s\n", filename, format, vmu->volgain, vmu->username);
	} else {
		finalfilename = ast_strdupa(filename);
	}

	/* Create file name */
	snprintf(fname, sizeof(fname), "%s.%s", finalfilename, format);

	if (template->attachment)
		ast_debug(1, "-- Attaching file '%s', format '%s', uservm is '%d'\n", finalfilename, format, attach_user_voicemail);

	/* Make a temporary file instead of piping directly to sendmail, in case the mail
	   command hangs */
	pfd = mkstemp(tmp);
	if (pfd > -1) {
		p = fdopen(pfd, "w");
		if (!p) {
			close(pfd);
			pfd = -1;
		}
		ast_debug(1, "-_-_- Opening temp file for e-mail: %s\n", tmp);
	}
	if (!p) {
		ast_log(LOG_WARNING, "Unable to open temporary file '%s'\n", tmp);
		return -1;
	}
	/* Allocate channel used for chanvar substitution */
	ast = ast_channel_alloc(0, AST_STATE_DOWN, 0, 0, "", "", "", 0, 0);


	snprintf(dur, sizeof(dur), "%d:%02d", duration / 60, duration % 60);

	/* Does this user have a timezone specified? */
	if (!ast_strlen_zero(vmu->zonetag)) {
		/* Find the zone in the list */
		struct minivm_zone *z;
		AST_LIST_LOCK(&minivm_zones);
		AST_LIST_TRAVERSE(&minivm_zones, z, list) {
			if (strcmp(z->name, vmu->zonetag)) 
				continue;
			the_zone = z;
		}
		AST_LIST_UNLOCK(&minivm_zones);
	}

	now = ast_tvnow();
	ast_localtime(&now, &tm, the_zone ? the_zone->timezone : NULL);
	ast_strftime(date, sizeof(date), "%a, %d %b %Y %H:%M:%S %z", &tm);

	/* Start printing the email to the temporary file */
	fprintf(p, "Date: %s\n", date);

	/* Set date format for voicemail mail */
	ast_strftime(date, sizeof(date), template->dateformat, &tm);


	/* Populate channel with channel variables for substitution */
	prep_email_sub_vars(ast, vmu, cidnum, cidname, dur, date, counter);

	/* Find email address to use */
	/* If there's a server e-mail adress in the account, user that, othterwise template */
	fromemail = ast_strlen_zero(vmu->serveremail) ?  template->serveremail : vmu->serveremail;

	/* Find name to user for server e-mail */
	fromaddress = ast_strlen_zero(template->fromaddress) ? "" : template->fromaddress;

	/* If needed, add hostname as domain */
	if (ast_strlen_zero(fromemail))
		fromemail = "asterisk";

	if (strchr(fromemail, '@'))
		ast_copy_string(who, fromemail, sizeof(who));
	else  {
		char host[MAXHOSTNAMELEN];
		gethostname(host, sizeof(host)-1);
		snprintf(who, sizeof(who), "%s@%s", fromemail, host);
	}

	if (ast_strlen_zero(fromaddress)) {
		fprintf(p, "From: Asterisk PBX <%s>\n", who);
	} else {
		/* Allocate a buffer big enough for variable substitution */
		int vmlen = strlen(fromaddress) * 3 + 200;

		ast_debug(4, "-_-_- Fromaddress template: %s\n", fromaddress);
		if ((passdata = alloca(vmlen))) {
			memset(passdata, 0, vmlen);
			pbx_substitute_variables_helper(ast, fromaddress, passdata, vmlen);
			len_passdata = strlen(passdata) * 2 + 3;
			passdata2 = alloca(len_passdata);
			fprintf(p, "From: %s <%s>\n", mailheader_quote(passdata, passdata2, len_passdata), who);
		} else  {
			ast_log(LOG_WARNING, "Cannot allocate workspace for variable substitution\n");
			fclose(p);
			return -1;	
		}
	} 
	ast_debug(4, "-_-_- Fromstring now: %s\n", ast_strlen_zero(passdata) ? "-default-" : passdata);

	fprintf(p, "Message-ID: <Asterisk-%d-%s-%d-%s>\n", (unsigned int)rand(), vmu->username, (int)getpid(), who);
	len_passdata = strlen(vmu->fullname) * 2 + 3;
	passdata2 = alloca(len_passdata);
	if (!ast_strlen_zero(vmu->email))
		fprintf(p, "To: %s <%s>\n", mailheader_quote(vmu->fullname, passdata2, len_passdata), vmu->email);
	else
		fprintf(p, "To: %s <%s@%s>\n", mailheader_quote(vmu->fullname, passdata2, len_passdata), vmu->username, vmu->domain);

	if (!ast_strlen_zero(template->subject)) {
		char *passdata;
		int vmlen = strlen(template->subject) * 3 + 200;
		if ((passdata = alloca(vmlen))) {
			memset(passdata, 0, vmlen);
			pbx_substitute_variables_helper(ast, template->subject, passdata, vmlen);
			fprintf(p, "Subject: %s\n", passdata);
		} else {
			ast_log(LOG_WARNING, "Cannot allocate workspace for variable substitution\n");
			fclose(p);
			return -1;	
		}

		ast_debug(4, "-_-_- Subject now: %s\n", passdata);

	} else  {
		fprintf(p, "Subject: New message in mailbox %s@%s\n", vmu->username, vmu->domain);
		ast_debug(1, "-_-_- Using default subject for this email \n");
	}


	if (option_debug > 2)
		fprintf(p, "X-Asterisk-debug: template %s user account %s@%s\n", template->name, vmu->username, vmu->domain);
	fprintf(p, "MIME-Version: 1.0\n");

	/* Something unique. */
	snprintf(bound, sizeof(bound), "voicemail_%s%d%d", vmu->username, (int)getpid(), (unsigned int)rand());

	fprintf(p, "Content-Type: multipart/mixed; boundary=\"%s\"\n\n\n", bound);

	fprintf(p, "--%s\n", bound);
	fprintf(p, "Content-Type: text/plain; charset=%s\nContent-Transfer-Encoding: 8bit\n\n", global_charset);
	if (!ast_strlen_zero(template->body)) {
		char *passdata;
		int vmlen = strlen(template->body)*3 + 200;
		if ((passdata = alloca(vmlen))) {
			memset(passdata, 0, vmlen);
			pbx_substitute_variables_helper(ast, template->body, passdata, vmlen);
			ast_debug(3, "Message now: %s\n-----\n", passdata);
			fprintf(p, "%s\n", passdata);
		} else
			ast_log(LOG_WARNING, "Cannot allocate workspace for variable substitution\n");
	} else {
		fprintf(p, "Dear %s:\n\n\tJust wanted to let you know you were just left a %s long message \n"

			"in mailbox %s from %s, on %s so you might\n"
			"want to check it when you get a chance.  Thanks!\n\n\t\t\t\t--Asterisk\n\n", vmu->fullname, 
			dur,  vmu->username, (cidname ? cidname : (cidnum ? cidnum : "an unknown caller")), date);
		ast_debug(3, "Using default message body (no template)\n-----\n");
	}
	/* Eww. We want formats to tell us their own MIME type */
	if (template->attachment) {
		char *ctype = "audio/x-";
		ast_debug(3, "-_-_- Attaching file to message: %s\n", fname);
		if (!strcasecmp(format, "ogg"))
			ctype = "application/";
	
		fprintf(p, "--%s\n", bound);
		fprintf(p, "Content-Type: %s%s; name=\"voicemailmsg.%s\"\n", ctype, format, format);
		fprintf(p, "Content-Transfer-Encoding: base64\n");
		fprintf(p, "Content-Description: Voicemail sound attachment.\n");
		fprintf(p, "Content-Disposition: attachment; filename=\"voicemail%s.%s\"\n\n", counter ? counter : "", format);

		base_encode(fname, p);
		fprintf(p, "\n\n--%s--\n.\n", bound);
	}
	fclose(p);
	snprintf(tmp2, sizeof(tmp2), "( %s < %s ; rm -f %s ) &", global_mailcmd, tmp, tmp);
	ast_safe_system(tmp2);
	ast_debug(1, "Sent message to %s with command '%s' - %s\n", vmu->email, global_mailcmd, template->attachment ? "(media attachment)" : "");
	ast_debug(3, "-_-_- Actual command used: %s\n", tmp2);
	if (ast)
		ast_channel_free(ast);
	return 0;
}

/*! \brief Create directory based on components */
static int make_dir(char *dest, int len, const char *domain, const char *username, const char *folder)
{
	return snprintf(dest, len, "%s%s/%s%s%s", MVM_SPOOL_DIR, domain, username, ast_strlen_zero(folder) ? "" : "/", folder ? folder : "");
}

/*! \brief Checks if directory exists. Does not create directory, but builds string in dest
 * \param dest    String. base directory.
 * \param len    Int. Length base directory string.
 * \param domain String. Ignored if is null or empty string.
 * \param username String. Ignored if is null or empty string. 
 * \param folder  String. Ignored if is null or empty string.
 * \return 0 on failure, 1 on success.
 */
static int check_dirpath(char *dest, int len, char *domain, char *username, char *folder)
{
	struct stat filestat;
	make_dir(dest, len, domain, username, folder ? folder : "");
	if (stat(dest, &filestat)== -1)
		return FALSE;
	else
		return TRUE;
}

/*! \brief basically mkdir -p $dest/$domain/$username/$folder
 * \param dest    String. base directory.
 * \param len     Length of directory string
 * \param domain  String. Ignored if is null or empty string.
 * \param folder  String. Ignored if is null or empty string. 
 * \param username  String. Ignored if is null or empty string.
 * \return -1 on failure, 0 on success.
 */
static int create_dirpath(char *dest, int len, char *domain, char *username, char *folder)
{
	int res;
	make_dir(dest, len, domain, username, folder);
	if ((res = ast_mkdir(dest, 0777))) {
		ast_log(LOG_WARNING, "ast_mkdir '%s' failed: %s\n", dest, strerror(res));
		return -1;
	}
	ast_debug(2, "Creating directory for %s@%s folder %s : %s\n", username, domain, folder, dest);
	return 0;
}


/*! \brief Play intro message before recording voicemail 
*/
static int invent_message(struct ast_channel *chan, char *domain, char *username, int busy, char *ecodes)
{
	int res;
	char fn[PATH_MAX];

	ast_debug(2, "-_-_- Still preparing to play message ...\n");

	snprintf(fn, sizeof(fn), "%s%s/%s/greet", MVM_SPOOL_DIR, domain, username);

	if (ast_fileexists(fn, NULL, NULL) > 0) {
		res = ast_streamfile(chan, fn, chan->language);
		if (res) 
			return -1;
		res = ast_waitstream(chan, ecodes);
		if (res) 
			return res;
	} else {
		int numericusername = 1;
		char *i = username;

		ast_debug(2, "-_-_- No personal prompts. Using default prompt set for language\n");
		
		while (*i)  {
			ast_debug(2, "-_-_- Numeric? Checking %c\n", *i);
			if (!isdigit(*i)) {
				numericusername = FALSE;
				break;
			}
			i++;
		}

		if (numericusername) {
			if(ast_streamfile(chan, "vm-theperson", chan->language))
				return -1;
			if ((res = ast_waitstream(chan, ecodes)))
				return res;
	
			res = ast_say_digit_str(chan, username, ecodes, chan->language);
			if (res)
				return res;
		} else {
			if(ast_streamfile(chan, "vm-theextensionis", chan->language))
				return -1;
			if ((res = ast_waitstream(chan, ecodes)))
				return res;
		}
	}

	res = ast_streamfile(chan, busy ? "vm-isonphone" : "vm-isunavail", chan->language);
	if (res)
		return -1;
	res = ast_waitstream(chan, ecodes);
	return res;
}

/*! \brief Delete media files and attribute file */
static int vm_delete(char *file)
{
	int res;

	ast_debug(1, "-_-_- Deleting voicemail file %s\n", file);

	res = unlink(file);	/* Remove the meta data file */
	res |=  ast_filedelete(file, NULL);	/* remove the media file */
	return res;
}


/*! \brief Record voicemail message & let caller review or re-record it, or set options if applicable */
static int play_record_review(struct ast_channel *chan, char *playfile, char *recordfile, int maxtime, char *fmt,
			      int outsidecaller, struct minivm_account *vmu, int *duration, const char *unlockdir,
			      signed char record_gain)
{
 	int cmd = 0;
 	int max_attempts = 3;
 	int attempts = 0;
 	int recorded = 0;
 	int message_exists = 0;
	signed char zero_gain = 0;
	char *acceptdtmf = "#";
	char *canceldtmf = "";

 	/* Note that urgent and private are for flagging messages as such in the future */
 
	/* barf if no pointer passed to store duration in */
	if (duration == NULL) {
		ast_log(LOG_WARNING, "Error play_record_review called without duration pointer\n");
		return -1;
	}

 	cmd = '3';	 /* Want to start by recording */
 
	while ((cmd >= 0) && (cmd != 't')) {
		switch (cmd) {
 		case '2':
 			/* Review */
 			if (option_verbose > 2)
				ast_verbose(VERBOSE_PREFIX_3 "Reviewing the message\n");
 			ast_streamfile(chan, recordfile, chan->language);
 			cmd = ast_waitstream(chan, AST_DIGIT_ANY);
 			break;
 		case '3':
 			message_exists = 0;
 			/* Record */
			if (option_verbose > 2) {
 				if (recorded == 1) 
					ast_verbose(VERBOSE_PREFIX_3 "Re-recording the message\n");
 				else
					ast_verbose(VERBOSE_PREFIX_3 "Recording the message\n");
			}
			if (recorded && outsidecaller) 
 				cmd = ast_play_and_wait(chan, "beep");
 			recorded = 1;
 			/* After an attempt has been made to record message, we have to take care of INTRO and beep for incoming messages, but not for greetings */
			if (record_gain)
				ast_channel_setoption(chan, AST_OPTION_RXGAIN, &record_gain, sizeof(record_gain), 0);
			if (ast_test_flag(vmu, MVM_OPERATOR))
				canceldtmf = "0";
			cmd = ast_play_and_record_full(chan, playfile, recordfile, maxtime, fmt, duration, global_silencethreshold, global_maxsilence, unlockdir, acceptdtmf, canceldtmf);
			if (record_gain)
				ast_channel_setoption(chan, AST_OPTION_RXGAIN, &zero_gain, sizeof(zero_gain), 0);
 			if (cmd == -1) /* User has hung up, no options to give */
 				return cmd;
 			if (cmd == '0')
 				break;
 			else if (cmd == '*')
 				break;
 			else {
 				/* If all is well, a message exists */
 				message_exists = 1;
				cmd = 0;
 			}
 			break;
 		case '4':
 		case '5':
 		case '6':
 		case '7':
 		case '8':
 		case '9':
		case '*':
		case '#':
 			cmd = ast_play_and_wait(chan, "vm-sorry");
 			break;
 		case '0':
			if(!ast_test_flag(vmu, MVM_OPERATOR)) {
 				cmd = ast_play_and_wait(chan, "vm-sorry");
 				break;
			}
			if (message_exists || recorded) {
				cmd = ast_play_and_wait(chan, "vm-saveoper");
				if (!cmd)
					cmd = ast_waitfordigit(chan, 3000);
				if (cmd == '1') {
					ast_play_and_wait(chan, "vm-msgsaved");
					cmd = '0';
				} else {
					ast_play_and_wait(chan, "vm-deleted");
					vm_delete(recordfile);
					cmd = '0';
				}
			}
			return cmd;
 		default:
			/* If the caller is an ouside caller, and the review option is enabled,
			   allow them to review the message, but let the owner of the box review
			   their OGM's */
			if (outsidecaller && !ast_test_flag(vmu, MVM_REVIEW))
				return cmd;
 			if (message_exists) {
 				cmd = ast_play_and_wait(chan, "vm-review");
 			} else {
 				cmd = ast_play_and_wait(chan, "vm-torerecord");
 				if (!cmd)
 					cmd = ast_waitfordigit(chan, 600);
 			}
 			
 			if (!cmd && outsidecaller && ast_test_flag(vmu, MVM_OPERATOR)) {
 				cmd = ast_play_and_wait(chan, "vm-reachoper");
 				if (!cmd)
 					cmd = ast_waitfordigit(chan, 600);
 			}
 			if (!cmd)
 				cmd = ast_waitfordigit(chan, 6000);
 			if (!cmd) {
 				attempts++;
 			}
 			if (attempts > max_attempts) {
 				cmd = 't';
 			}
 		}
 	}
 	if (outsidecaller)  
		ast_play_and_wait(chan, "vm-goodbye");
 	if (cmd == 't')
 		cmd = 0;
 	return cmd;
}

/*! \brief Run external notification for voicemail message */
static void run_externnotify(struct ast_channel *chan, struct minivm_account *vmu)
{
	char arguments[BUFSIZ];

	if (ast_strlen_zero(vmu->externnotify) && ast_strlen_zero(global_externnotify))
		return;

	snprintf(arguments, sizeof(arguments), "%s %s@%s %s %s&", 
		ast_strlen_zero(vmu->externnotify) ? global_externnotify : vmu->externnotify, 
		vmu->username, vmu->domain,
		chan->cid.cid_name, chan->cid.cid_num);

	ast_debug(1, "Executing: %s\n", arguments);
  	ast_safe_system(arguments);
}

/*! \brief Send message to voicemail account owner */
static int notify_new_message(struct ast_channel *chan, const char *templatename, struct minivm_account *vmu, const char *filename, long duration, const char *format, char *cidnum, char *cidname)
{
	char *stringp;
	struct minivm_template *etemplate;
	char *messageformat;
	int res = 0;
	char oldlocale[100];
	const char *counter;

	if (!ast_strlen_zero(vmu->attachfmt)) {
		if (strstr(format, vmu->attachfmt)) {
			format = vmu->attachfmt;
		} else 
			ast_log(LOG_WARNING, "Attachment format '%s' is not one of the recorded formats '%s'.  Falling back to default format for '%s@%s'.\n", vmu->attachfmt, format, vmu->username, vmu->domain);
	}

	etemplate = message_template_find(vmu->etemplate);
	if (!etemplate)
		etemplate = message_template_find(templatename);
	if (!etemplate)
		etemplate = message_template_find("email-default");

	/* Attach only the first format */
	stringp = messageformat = ast_strdupa(format);
	strsep(&stringp, "|");

	if (!ast_strlen_zero(etemplate->locale)) {
		char *newlocale;
		ast_copy_string(oldlocale, setlocale(LC_TIME, NULL), sizeof(oldlocale));
		ast_debug(2, "-_-_- Changing locale from %s to %s\n", oldlocale, etemplate->locale);
		newlocale = setlocale(LC_TIME, etemplate->locale);
		if (newlocale == NULL) {
			ast_log(LOG_WARNING, "-_-_- Changing to new locale did not work. Locale: %s\n", etemplate->locale);
		}
	}



	/* Read counter if available */
	counter = pbx_builtin_getvar_helper(chan, "MVM_COUNTER");
	if (ast_strlen_zero(counter)) {
		ast_debug(2, "-_-_- MVM_COUNTER not found\n");
	} else {
		ast_debug(2, "-_-_- MVM_COUNTER found - will use it with value %s\n", counter);
	}

	res = sendmail(etemplate, vmu, cidnum, cidname, filename, messageformat, duration, etemplate->attachment, MVM_MESSAGE_EMAIL, counter);

	if (res == 0 && !ast_strlen_zero(vmu->pager))  {
		/* Find template for paging */
		etemplate = message_template_find(vmu->ptemplate);
		if (!etemplate)
			etemplate = message_template_find("pager-default");
		if (etemplate->locale) {
			ast_copy_string(oldlocale, setlocale(LC_TIME, ""), sizeof(oldlocale));
			setlocale(LC_TIME, etemplate->locale);
		}

		res = sendmail(etemplate, vmu, cidnum, cidname, filename, messageformat, duration, etemplate->attachment, MVM_MESSAGE_PAGE, counter);
	}

	manager_event(EVENT_FLAG_CALL, "MiniVoiceMail", "Action: SentNotification\rn\nMailbox: %s@%s\r\nCounter: %s\r\n", vmu->username, vmu->domain, counter);

	run_externnotify(chan, vmu);		/* Run external notification */

	if (etemplate->locale) 
		setlocale(LC_TIME, oldlocale); /* Rest to old locale */
	return res;
}

 
/*! \brief Record voicemail message, store into file prepared for sending e-mail */
static int leave_voicemail(struct ast_channel *chan, char *username, struct leave_vm_options *options)
{
	char tmptxtfile[PATH_MAX];
	char callerid[256];
	FILE *txt;
	int res = 0, txtdes;
	int msgnum;
	int duration = 0;
	char date[256];
	char tmpdir[PATH_MAX];
	char ext_context[256] = "";
	char fmt[80];
	char *domain;
	char tmp[256] = "";
	struct minivm_account *vmu;
	int userdir;

	ast_copy_string(tmp, username, sizeof(tmp));
	username = tmp;
	domain = strchr(tmp, '@');
	if (domain) {
		*domain = '\0';
		domain++;
	}

	if (!(vmu = find_account(domain, username, TRUE))) {
		/* We could not find user, let's exit */
		ast_log(LOG_ERROR, "Can't allocate temporary account for '%s@%s'\n", username, domain);
		pbx_builtin_setvar_helper(chan, "MINIVM_RECORD_STATUS", "FAILED");
		return 0;
	}

	/* Setup pre-file if appropriate */
	if (strcmp(vmu->domain, "localhost"))
		snprintf(ext_context, sizeof(ext_context), "%s@%s", username, vmu->domain);
	else
		ast_copy_string(ext_context, vmu->domain, sizeof(ext_context));

	/* The meat of recording the message...  All the announcements and beeps have been played*/
	if (ast_strlen_zero(vmu->attachfmt))
		ast_copy_string(fmt, default_vmformat, sizeof(fmt));
	else
		ast_copy_string(fmt, vmu->attachfmt, sizeof(fmt));

	if (ast_strlen_zero(fmt)) {
		ast_log(LOG_WARNING, "No format for saving voicemail? Default %s\n", default_vmformat);
		pbx_builtin_setvar_helper(chan, "MINIVM_RECORD_STATUS", "FAILED");
		return res;
	}
	msgnum = 0;

	userdir = check_dirpath(tmpdir, sizeof(tmpdir), vmu->domain, username, "tmp");

	/* If we have no user directory, use generic temporary directory */
	if (!userdir) {
		create_dirpath(tmpdir, sizeof(tmpdir), "0000_minivm_temp", "mediafiles", "");
		ast_debug(3, "Creating temporary directory %s\n", tmpdir);
	}


	snprintf(tmptxtfile, sizeof(tmptxtfile), "%s/XXXXXX", tmpdir);
	

	/* XXX This file needs to be in temp directory */
	txtdes = mkstemp(tmptxtfile);
	if (txtdes < 0) {
		ast_log(LOG_ERROR, "Unable to create message file %s: %s\n", tmptxtfile, strerror(errno));
		res = ast_streamfile(chan, "vm-mailboxfull", chan->language);
		if (!res)
			res = ast_waitstream(chan, "");
		pbx_builtin_setvar_helper(chan, "MINIVM_RECORD_STATUS", "FAILED");
		return res;
	}

	if (res >= 0) {
		/* Unless we're *really* silent, try to send the beep */
		res = ast_streamfile(chan, "beep", chan->language);
		if (!res)
			res = ast_waitstream(chan, "");
	}

	/* OEJ XXX Maybe this can be turned into a log file? Hmm. */
	/* Store information */
	ast_debug(2, "Open file for metadata: %s\n", tmptxtfile);

	res = play_record_review(chan, NULL, tmptxtfile, global_vmmaxmessage, fmt, 1, vmu, &duration, NULL, options->record_gain);

	txt = fdopen(txtdes, "w+");
	if (!txt) {
		ast_log(LOG_WARNING, "Error opening text file for output\n");
	} else {
		struct ast_tm tm;
		struct timeval now = ast_tvnow();
		char timebuf[30];
		char logbuf[BUFSIZ];
		get_date(date, sizeof(date));
		ast_localtime(&now, &tm, NULL);
		ast_strftime(timebuf, sizeof(timebuf), "%H:%M:%S", &tm);
		
		snprintf(logbuf, sizeof(logbuf),
			/* "Mailbox:domain:macrocontext:exten:priority:callerchan:callerid:origdate:origtime:duration:durationstatus:accountcode" */
			"%s:%s:%s:%s:%d:%s:%s:%s:%s:%d:%s:%s\n",
			username,
			chan->context,
			chan->macrocontext, 
			chan->exten,
			chan->priority,
			chan->name,
			ast_callerid_merge(callerid, sizeof(callerid), chan->cid.cid_name, chan->cid.cid_num, "Unknown"),
			date, 
			timebuf,
			duration,
			duration < global_vmminmessage ? "IGNORED" : "OK",
			vmu->accountcode
		); 
		fprintf(txt, logbuf);
		if (minivmlogfile) {
			ast_mutex_lock(&minivmloglock);
			fprintf(minivmlogfile, logbuf);
			ast_mutex_unlock(&minivmloglock);
		}

		if (duration < global_vmminmessage) {
			if (option_verbose > 2) 
				ast_verbose( VERBOSE_PREFIX_3 "Recording was %d seconds long but needs to be at least %d - abandoning\n", duration, global_vmminmessage);
			fclose(txt);
			ast_filedelete(tmptxtfile, NULL);
			unlink(tmptxtfile);
			pbx_builtin_setvar_helper(chan, "MINIVM_RECORD_STATUS", "FAILED");
			return 0;
		} 
		fclose(txt); /* Close log file */
		if (ast_fileexists(tmptxtfile, NULL, NULL) <= 0) {
			ast_debug(1, "The recorded media file is gone, so we should remove the .txt file too!\n");
			unlink(tmptxtfile);
			pbx_builtin_setvar_helper(chan, "MINIVM_RECORD_STATUS", "FAILED");
			if(ast_test_flag(vmu, MVM_ALLOCED))
				free_user(vmu);
			return 0;
		}

		/* Set channel variables for the notify application */
		pbx_builtin_setvar_helper(chan, "MVM_FILENAME", tmptxtfile);
		snprintf(timebuf, sizeof(timebuf), "%d", duration);
		pbx_builtin_setvar_helper(chan, "MVM_DURATION", timebuf);
		pbx_builtin_setvar_helper(chan, "MVM_FORMAT", fmt);

	}
	global_stats.lastreceived = ast_tvnow();
	global_stats.receivedmessages++;
//	/* Go ahead and delete audio files from system, they're not needed any more */
//	if (ast_fileexists(tmptxtfile, NULL, NULL) <= 0) {
//		ast_filedelete(tmptxtfile, NULL);
//		 /* Even not being used at the moment, it's better to convert ast_log to ast_debug anyway */
//		ast_debug(2, "-_-_- Deleted audio file after notification :: %s \n", tmptxtfile);
//	}

	if (res > 0)
		res = 0;

	if(ast_test_flag(vmu, MVM_ALLOCED))
		free_user(vmu);

	pbx_builtin_setvar_helper(chan, "MINIVM_RECORD_STATUS", "SUCCESS");
	return res;
}

/*! \brief Notify voicemail account owners - either generic template or user specific */
static int minivm_notify_exec(struct ast_channel *chan, void *data)
{
	int argc;
	char *argv[2];
	int res = 0;
	char tmp[PATH_MAX];
	char *domain;
	char *tmpptr;
	struct minivm_account *vmu;
	char *username = argv[0];
	const char *template = "";
	const char *filename;
	const char *format;
	const char *duration_string;
	
	if (ast_strlen_zero(data))  {
		ast_log(LOG_ERROR, "Minivm needs at least an account argument \n");
		return -1;
	}
	tmpptr = ast_strdupa((char *)data);
	if (!tmpptr) {
		ast_log(LOG_ERROR, "Out of memory\n");
		return -1;
	}
	argc = ast_app_separate_args(tmpptr, ',', argv, sizeof(argv) / sizeof(argv[0]));

	if (argc == 2 && !ast_strlen_zero(argv[1]))
		template = argv[1];

	ast_copy_string(tmp, argv[0], sizeof(tmp));
	username = tmp;
	domain = strchr(tmp, '@');
	if (domain) {
		*domain = '\0';
		domain++;
	} 
	if (ast_strlen_zero(domain) || ast_strlen_zero(username)) {
		ast_log(LOG_ERROR, "Need username@domain as argument. Sorry. Argument 0 %s\n", argv[0]);
		return -1;
	}

	if(!(vmu = find_account(domain, username, TRUE))) {
		/* We could not find user, let's exit */
		ast_log(LOG_WARNING, "Could not allocate temporary memory for '%s@%s'\n", username, domain);
		pbx_builtin_setvar_helper(chan, "MINIVM_NOTIFY_STATUS", "FAILED");
		return -1;
	}
	
	filename = pbx_builtin_getvar_helper(chan, "MVM_FILENAME");
	format = pbx_builtin_getvar_helper(chan, "MVM_FORMAT");
	duration_string = pbx_builtin_getvar_helper(chan, "MVM_DURATION");
	/* Notify of new message to e-mail and pager */
	if (!ast_strlen_zero(filename)) {
		res = notify_new_message(chan, template, vmu, filename, atoi(duration_string), format, chan->cid.cid_num, chan->cid.cid_name);
	};

	pbx_builtin_setvar_helper(chan, "MINIVM_NOTIFY_STATUS", res == 0 ? "SUCCESS" : "FAILED");


	if(ast_test_flag(vmu, MVM_ALLOCED))
		free_user(vmu);

	/* Ok, we're ready to rock and roll. Return to dialplan */

	return res;

}

/*! \brief Dialplan function to record voicemail */
static int minivm_record_exec(struct ast_channel *chan, void *data)
{
	int res = 0;
	char *tmp;
	struct leave_vm_options leave_options;
	int argc;
	char *argv[2];
	struct ast_flags flags = { 0 };
	char *opts[OPT_ARG_ARRAY_SIZE];
		
	memset(&leave_options, 0, sizeof(leave_options));

	/* Answer channel if it's not already answered */
	if (chan->_state != AST_STATE_UP)
		ast_answer(chan);

	if (ast_strlen_zero(data))  {
		ast_log(LOG_ERROR, "Minivm needs at least an account argument \n");
		return -1;
	}
	tmp = ast_strdupa((char *)data);
	if (!tmp) {
		ast_log(LOG_ERROR, "Out of memory\n");
		return -1;
	}
	argc = ast_app_separate_args(tmp, ',', argv, sizeof(argv) / sizeof(argv[0]));
	if (argc == 2) {
		if (ast_app_parse_options(minivm_app_options, &flags, opts, argv[1])) {
			return -1;
		}
		ast_copy_flags(&leave_options, &flags, OPT_SILENT | OPT_BUSY_GREETING | OPT_UNAVAIL_GREETING );
		if (ast_test_flag(&flags, OPT_RECORDGAIN)) {
			int gain;

			if (sscanf(opts[OPT_ARG_RECORDGAIN], "%d", &gain) != 1) {
				ast_log(LOG_WARNING, "Invalid value '%s' provided for record gain option\n", opts[OPT_ARG_RECORDGAIN]);
				return -1;
			} else 
				leave_options.record_gain = (signed char) gain;
		}
	} 

	/* Now run the appliation and good luck to you! */
	res = leave_voicemail(chan, argv[0], &leave_options);

	if (res == ERROR_LOCK_PATH) {
		ast_log(LOG_ERROR, "Could not leave voicemail. The path is already locked.\n");
		pbx_builtin_setvar_helper(chan, "MINIVM_RECORD_STATUS", "FAILED");
		res = 0;
	}
	pbx_builtin_setvar_helper(chan, "MINIVM_RECORD_STATUS", "SUCCESS");

	return res;
}

/*! \brief Play voicemail prompts - either generic or user specific */
static int minivm_greet_exec(struct ast_channel *chan, void *data)
{
	struct leave_vm_options leave_options = { 0, '\0'};
	int argc;
	char *argv[2];
	struct ast_flags flags = { 0 };
	char *opts[OPT_ARG_ARRAY_SIZE];
	int res = 0;
	int ausemacro = 0;
	int ousemacro = 0;
	int ouseexten = 0;
	char tmp[PATH_MAX];
	char dest[PATH_MAX];
	char prefile[PATH_MAX];
	char tempfile[PATH_MAX] = "";
	char ext_context[256] = "";
	char *domain;
	char ecodes[16] = "#";
	char *tmpptr;
	struct minivm_account *vmu;
	char *username = argv[0];

	if (ast_strlen_zero(data))  {
		ast_log(LOG_ERROR, "Minivm needs at least an account argument \n");
		return -1;
	}
	tmpptr = ast_strdupa((char *)data);
	if (!tmpptr) {
		ast_log(LOG_ERROR, "Out of memory\n");
		return -1;
	}
	argc = ast_app_separate_args(tmpptr, ',', argv, sizeof(argv) / sizeof(argv[0]));

	if (argc == 2) {
		if (ast_app_parse_options(minivm_app_options, &flags, opts, argv[1]))
			return -1;
		ast_copy_flags(&leave_options, &flags, OPT_SILENT | OPT_BUSY_GREETING | OPT_UNAVAIL_GREETING );
	}

	ast_copy_string(tmp, argv[0], sizeof(tmp));
	username = tmp;
	domain = strchr(tmp, '@');
	if (domain) {
		*domain = '\0';
		domain++;
	} 
	if (ast_strlen_zero(domain) || ast_strlen_zero(username)) {
		ast_log(LOG_ERROR, "Need username@domain as argument. Sorry. Argument:  %s\n", argv[0]);
		return -1;
	}
	ast_debug(1, "-_-_- Trying to find configuration for user %s in domain %s\n", username, domain);

	if (!(vmu = find_account(domain, username, TRUE))) {
		ast_log(LOG_ERROR, "Could not allocate memory. \n");
		return -1;
	}

	/* Answer channel if it's not already answered */
	if (chan->_state != AST_STATE_UP)
		ast_answer(chan);

	/* Setup pre-file if appropriate */
	if (strcmp(vmu->domain, "localhost"))
		snprintf(ext_context, sizeof(ext_context), "%s@%s", username, vmu->domain);
	else
		ast_copy_string(ext_context, vmu->domain, sizeof(ext_context));

	if (ast_test_flag(&leave_options, OPT_BUSY_GREETING)) {
		res = check_dirpath(dest, sizeof(dest), vmu->domain, username, "busy");
		if (res)
			snprintf(prefile, sizeof(prefile), "%s%s/%s/busy", MVM_SPOOL_DIR, vmu->domain, username);
	} else if (ast_test_flag(&leave_options, OPT_UNAVAIL_GREETING)) {
		res = check_dirpath(dest, sizeof(dest), vmu->domain, username, "unavail");
		if (res)
			snprintf(prefile, sizeof(prefile), "%s%s/%s/unavail", MVM_SPOOL_DIR, vmu->domain, username);
	}
	/* Check for temporary greeting - it overrides busy and unavail */
	snprintf(tempfile, sizeof(tempfile), "%s%s/%s/temp", MVM_SPOOL_DIR, vmu->domain, username);
	if (!(res = check_dirpath(dest, sizeof(dest), vmu->domain, username, "temp"))) {
		ast_debug(2, "Temporary message directory does not exist, using default (%s)\n", tempfile);
		ast_copy_string(prefile, tempfile, sizeof(prefile));
	}
	ast_debug(2, "-_-_- Preparing to play message ...\n");

	/* Check current or macro-calling context for special extensions */
	if (ast_test_flag(vmu, MVM_OPERATOR)) {
		if (!ast_strlen_zero(vmu->exit)) {
			if (ast_exists_extension(chan, vmu->exit, "o", 1, chan->cid.cid_num)) {
				strncat(ecodes, "0", sizeof(ecodes) - strlen(ecodes) - 1);
				ouseexten = 1;
			}
		} else if (ast_exists_extension(chan, chan->context, "o", 1, chan->cid.cid_num)) {
			strncat(ecodes, "0", sizeof(ecodes) - strlen(ecodes) - 1);
			ouseexten = 1;
		}
		else if (!ast_strlen_zero(chan->macrocontext) && ast_exists_extension(chan, chan->macrocontext, "o", 1, chan->cid.cid_num)) {
			strncat(ecodes, "0", sizeof(ecodes) - strlen(ecodes) - 1);
			ousemacro = 1;
		}
	}

	if (!ast_strlen_zero(vmu->exit)) {
		if (ast_exists_extension(chan, vmu->exit, "a", 1, chan->cid.cid_num))
			strncat(ecodes, "*", sizeof(ecodes) -  strlen(ecodes) - 1);
	} else if (ast_exists_extension(chan, chan->context, "a", 1, chan->cid.cid_num))
		strncat(ecodes, "*", sizeof(ecodes) -  strlen(ecodes) - 1);
	else if (!ast_strlen_zero(chan->macrocontext) && ast_exists_extension(chan, chan->macrocontext, "a", 1, chan->cid.cid_num)) {
		strncat(ecodes, "*", sizeof(ecodes) -  strlen(ecodes) - 1);
		ausemacro = 1;
	}

	res = 0;	/* Reset */
	/* Play the beginning intro if desired */
	if (!ast_strlen_zero(prefile)) {
		if (ast_streamfile(chan, prefile, chan->language) > -1) 
			res = ast_waitstream(chan, ecodes);
	} else {
		ast_debug(2, "%s doesn't exist, doing what we can\n", prefile);
		res = invent_message(chan, vmu->domain, username, ast_test_flag(&leave_options, OPT_BUSY_GREETING), ecodes);
	}
	if (res < 0) {
		ast_debug(2, "Hang up during prefile playback\n");
		pbx_builtin_setvar_helper(chan, "MINIVM_GREET_STATUS", "FAILED");
		if(ast_test_flag(vmu, MVM_ALLOCED))
			free_user(vmu);
		return -1;
	}
	if (res == '#') {
		/* On a '#' we skip the instructions */
		ast_set_flag(&leave_options, OPT_SILENT);
		res = 0;
	}
	if (!res && !ast_test_flag(&leave_options, OPT_SILENT)) {
		res = ast_streamfile(chan, SOUND_INTRO, chan->language);
		if (!res)
			res = ast_waitstream(chan, ecodes);
		if (res == '#') {
			ast_set_flag(&leave_options, OPT_SILENT);
			res = 0;
		}
	}
	if (res > 0)
		ast_stopstream(chan);
	/* Check for a '*' here in case the caller wants to escape from voicemail to something
	   other than the operator -- an automated attendant or mailbox login for example */
	if (res == '*') {
		chan->exten[0] = 'a';
		chan->exten[1] = '\0';
		if (!ast_strlen_zero(vmu->exit)) {
			ast_copy_string(chan->context, vmu->exit, sizeof(chan->context));
		} else if (ausemacro && !ast_strlen_zero(chan->macrocontext)) {
			ast_copy_string(chan->context, chan->macrocontext, sizeof(chan->context));
		}
		chan->priority = 0;
		pbx_builtin_setvar_helper(chan, "MINIVM_GREET_STATUS", "USEREXIT");
		res = 0;
	} else if (res == '0') { /* Check for a '0' here */
		if(ouseexten || ousemacro) {
			chan->exten[0] = 'o';
			chan->exten[1] = '\0';
			if (!ast_strlen_zero(vmu->exit)) {
				ast_copy_string(chan->context, vmu->exit, sizeof(chan->context));
			} else if (ousemacro && !ast_strlen_zero(chan->macrocontext)) {
				ast_copy_string(chan->context, chan->macrocontext, sizeof(chan->context));
			}
			ast_play_and_wait(chan, "transfer");
			chan->priority = 0;
			pbx_builtin_setvar_helper(chan, "MINIVM_GREET_STATUS", "USEREXIT");
		}
		res =  0;
	} else if (res < 0) {
		pbx_builtin_setvar_helper(chan, "MINIVM_GREET_STATUS", "FAILED");
		res = -1;
	} else
		pbx_builtin_setvar_helper(chan, "MINIVM_GREET_STATUS", "SUCCESS");

	if(ast_test_flag(vmu, MVM_ALLOCED))
		free_user(vmu);


	/* Ok, we're ready to rock and roll. Return to dialplan */
	return res;

}

/*! \brief Dialplan application to delete voicemail */
static int minivm_delete_exec(struct ast_channel *chan, void *data)
{
	int res = 0;
	char filename[BUFSIZ];
		
	if (!ast_strlen_zero(data))
		ast_copy_string(filename, (char *) data, sizeof(filename));
	else
		ast_copy_string(filename, pbx_builtin_getvar_helper(chan, "MVM_FILENAME"), sizeof(filename));

	if (ast_strlen_zero(filename)) {
		ast_log(LOG_ERROR, "No filename given in application arguments or channel variable MVM_FILENAME\n");
		return res;
	} 

	/* Go ahead and delete audio files from system, they're not needed any more */
	/* We should look for both audio and text files here */
	if (ast_fileexists(filename, NULL, NULL) > 0) {
		res = vm_delete(filename);
		if (res) {
			ast_debug(2, "-_-_- Can't delete file: %s\n", filename);
			pbx_builtin_setvar_helper(chan, "MINIVM_DELETE_STATUS", "FAILED");
		} else {
			ast_debug(2, "-_-_- Deleted voicemail file :: %s \n", filename);
			pbx_builtin_setvar_helper(chan, "MINIVM_DELETE_STATUS", "SUCCESS");
		}
	} else {
		ast_debug(2, "-_-_- Filename does not exist: %s\n", filename);
		pbx_builtin_setvar_helper(chan, "MINIVM_DELETE_STATUS", "FAILED");
	}

	return res;
}

/*! \brief Record specific messages for voicemail account */
static int minivm_accmess_exec(struct ast_channel *chan, void *data)
{
	int argc = 0;
	char *argv[2];
	int res = 0;
	char filename[PATH_MAX];
	char tmp[PATH_MAX];
	char *domain;
	char *tmpptr = NULL;
	struct minivm_account *vmu;
	char *username = argv[0];
	struct ast_flags flags = { 0 };
	char *opts[OPT_ARG_ARRAY_SIZE];
	int error = FALSE;
	char *message = NULL;
	char *prompt = NULL;
	int duration;
	int cmd;

	if (ast_strlen_zero(data))  {
		ast_log(LOG_ERROR, "MinivmAccmess needs at least two arguments: account and option\n");
		error = TRUE;
	} else 
		tmpptr = ast_strdupa((char *)data);
	if (!error) {
		if (!tmpptr) {
			ast_log(LOG_ERROR, "Out of memory\n");
			error = TRUE;
		} else
			argc = ast_app_separate_args(tmpptr, ',', argv, sizeof(argv) / sizeof(argv[0]));
	}

	if (argc <=1) {
		ast_log(LOG_ERROR, "MinivmAccmess needs at least two arguments: account and option\n");
		error = TRUE;
	}
	if (!error && strlen(argv[1]) > 1) {
		ast_log(LOG_ERROR, "MinivmAccmess can only handle one option at a time. Bad option string: %s\n", argv[1]);
		error = TRUE;
	}

	if (!error && ast_app_parse_options(minivm_accmess_options, &flags, opts, argv[1])) {
		ast_log(LOG_ERROR, "Can't parse option %s\n", argv[1]);
		error = TRUE;
	}

	if (error)
		return -1;

	ast_copy_string(tmp, argv[0], sizeof(tmp));
	username = tmp;
	domain = strchr(tmp, '@');
	if (domain) {
		*domain = '\0';
		domain++;
	} 
	if (ast_strlen_zero(domain) || ast_strlen_zero(username)) {
		ast_log(LOG_ERROR, "Need username@domain as argument. Sorry. Argument 0 %s\n", argv[0]);
		return -1;
	}

	if(!(vmu = find_account(domain, username, TRUE))) {
		/* We could not find user, let's exit */
		ast_log(LOG_WARNING, "Could not allocate temporary memory for '%s@%s'\n", username, domain);
		pbx_builtin_setvar_helper(chan, "MINIVM_NOTIFY_STATUS", "FAILED");
		return -1;
	}

	/* Answer channel if it's not already answered */
	if (chan->_state != AST_STATE_UP)
		ast_answer(chan);
	
	/* Here's where the action is */
	if (ast_test_flag(&flags, OPT_BUSY_GREETING)) {
		message = "busy";
		prompt = "vm-rec-busy";
	} else if (ast_test_flag(&flags, OPT_UNAVAIL_GREETING)) {
		message = "unavailable";
		prompt = "vm-rec-unavail";
	} else if (ast_test_flag(&flags, OPT_TEMP_GREETING)) {
		message = "temp";
		prompt = "vm-temp-greeting";
	} else if (ast_test_flag(&flags, OPT_NAME_GREETING)) {
		message = "greet";
		prompt = "vm-rec-name";
	}
	snprintf(filename,sizeof(filename), "%s%s/%s/%s", MVM_SPOOL_DIR, vmu->domain, vmu->username, message);
	/* Maybe we should check the result of play_record_review ? */
	cmd = play_record_review(chan, prompt, filename, global_maxgreet, default_vmformat, 0, vmu, &duration, NULL, FALSE);

	ast_debug(1, "Recorded new %s message in %s (duration %d)\n", message, filename, duration);

	if(ast_test_flag(vmu, MVM_ALLOCED))
		free_user(vmu);


	/* Ok, we're ready to rock and roll. Return to dialplan */
	return res;

}

/*! \brief Append new mailbox to mailbox list from configuration file */
static int create_vmaccount(char *name, struct ast_variable *var, int realtime)
{
	struct minivm_account *vmu;
	char *domain;
	char *username;
	char accbuf[BUFSIZ];

	ast_debug(3, "Creating %s account for [%s]\n", realtime ? "realtime" : "static", name);

	ast_copy_string(accbuf, name, sizeof(accbuf));
	username = accbuf;
	domain = strchr(accbuf, '@');
	if (domain) {
		*domain = '\0';
		domain++;
	}
	if (ast_strlen_zero(domain)) {
		ast_log(LOG_ERROR, "No domain given for mini-voicemail account %s. Not configured.\n", name);
		return 0;
	}

	ast_debug(3, "Creating static account for user %s domain %s\n", username, domain);

	/* Allocate user account */
	vmu = ast_calloc(1, sizeof(*vmu));
	if (!vmu)
		return 0;
	
	ast_copy_string(vmu->domain, domain, sizeof(vmu->domain));
	ast_copy_string(vmu->username, username, sizeof(vmu->username));

	populate_defaults(vmu);

	ast_debug(3, "...Configuring account %s\n", name);

	while (var) {
		ast_debug(3, "---- Configuring %s = \"%s\" for account %s\n", var->name, var->value, name);
		if (!strcasecmp(var->name, "serveremail")) {
			ast_copy_string(vmu->serveremail, var->value, sizeof(vmu->serveremail));
		} else if (!strcasecmp(var->name, "email")) {
			ast_copy_string(vmu->email, var->value, sizeof(vmu->email));
		} else if (!strcasecmp(var->name, "accountcode")) {
			ast_copy_string(vmu->accountcode, var->value, sizeof(vmu->accountcode));
		} else if (!strcasecmp(var->name, "pincode")) {
			ast_copy_string(vmu->pincode, var->value, sizeof(vmu->pincode));
		} else if (!strcasecmp(var->name, "domain")) {
			ast_copy_string(vmu->domain, var->value, sizeof(vmu->domain));
		} else if (!strcasecmp(var->name, "language")) {
			ast_copy_string(vmu->language, var->value, sizeof(vmu->language));
		} else if (!strcasecmp(var->name, "timezone")) {
			ast_copy_string(vmu->zonetag, var->value, sizeof(vmu->zonetag));
		} else if (!strcasecmp(var->name, "externnotify")) {
			ast_copy_string(vmu->externnotify, var->value, sizeof(vmu->externnotify));
		} else if (!strcasecmp(var->name, "etemplate")) {
			ast_copy_string(vmu->etemplate, var->value, sizeof(vmu->etemplate));
		} else if (!strcasecmp(var->name, "ptemplate")) {
			ast_copy_string(vmu->ptemplate, var->value, sizeof(vmu->ptemplate));
		} else if (!strcasecmp(var->name, "fullname")) {
			ast_copy_string(vmu->fullname, var->value, sizeof(vmu->fullname));
		} else if (!strcasecmp(var->name, "setvar")) {
			char *varval;
			char *varname = ast_strdupa(var->value);
			struct ast_variable *tmpvar;

			if (varname && (varval = strchr(varname, '='))) {
				*varval = '\0';
				varval++;
				if ((tmpvar = ast_variable_new(varname, varval))) {
					tmpvar->next = vmu->chanvars;
					vmu->chanvars = tmpvar;
				}
			}
		} else if (!strcasecmp(var->name, "pager")) {
			ast_copy_string(vmu->pager, var->value, sizeof(vmu->pager));
		} else if (!strcasecmp(var->name, "volgain")) {
			sscanf(var->value, "%lf", &vmu->volgain);
		} else {
			ast_log(LOG_ERROR, "Unknown configuration option for minivm account %s : %s\n", name, var->name);
		}
		var = var->next;
	}
	ast_debug(3, "...Linking account %s\n", name);
	
	AST_LIST_LOCK(&minivm_accounts);
	AST_LIST_INSERT_TAIL(&minivm_accounts, vmu, list);
	AST_LIST_UNLOCK(&minivm_accounts);

	global_stats.voicemailaccounts++;

	ast_debug(2, "MINIVM :: Created account %s@%s - tz %s etemplate %s %s\n", username, domain, ast_strlen_zero(vmu->zonetag) ? "" : vmu->zonetag, ast_strlen_zero(vmu->etemplate) ? "" : vmu->etemplate, realtime ? "(realtime)" : "");
	return 0;
}

/*! \brief Free Mini Voicemail timezone */
static void free_zone(struct minivm_zone *z)
{
	ast_free(z);
}

/*! \brief Clear list of timezones */
static void timezone_destroy_list(void)
{
	struct minivm_zone *this;

	AST_LIST_LOCK(&minivm_zones);
	while ((this = AST_LIST_REMOVE_HEAD(&minivm_zones, list))) 
		free_zone(this);
		
	AST_LIST_UNLOCK(&minivm_zones);
}

/*! \brief Add time zone to memory list */
static int timezone_add(char *zonename, char *config)
{

	struct minivm_zone *newzone;
	char *msg_format, *timezone;

	newzone = ast_calloc(1, sizeof(*newzone));
	if (newzone == NULL)
		return 0;

	msg_format = ast_strdupa(config);
	if (msg_format == NULL) {
		ast_log(LOG_WARNING, "Out of memory.\n");
		ast_free(newzone);
		return 0;
	}

	timezone = strsep(&msg_format, "|");
	if (!msg_format) {
		ast_log(LOG_WARNING, "Invalid timezone definition : %s\n", zonename);
		ast_free(newzone);
		return 0;
	}
			
	ast_copy_string(newzone->name, zonename, sizeof(newzone->name));
	ast_copy_string(newzone->timezone, timezone, sizeof(newzone->timezone));
	ast_copy_string(newzone->msg_format, msg_format, sizeof(newzone->msg_format));

	AST_LIST_LOCK(&minivm_zones);
	AST_LIST_INSERT_TAIL(&minivm_zones, newzone, list);
	AST_LIST_UNLOCK(&minivm_zones);

	global_stats.timezones++;

	return 0;
}

/*! \brief Read message template from file */
static char *message_template_parse_filebody(char *filename) {
	char buf[BUFSIZ * 6];
	char readbuf[BUFSIZ];
	char filenamebuf[BUFSIZ];
	char *writepos;
	char *messagebody;
	FILE *fi;
	int lines = 0;

	if (ast_strlen_zero(filename))
		return NULL;
	if (*filename == '/') 
		ast_copy_string(filenamebuf, filename, sizeof(filenamebuf));
	else 
		snprintf(filenamebuf, sizeof(filenamebuf), "%s/%s", ast_config_AST_CONFIG_DIR, filename);

	if (!(fi = fopen(filenamebuf, "r"))) {
		ast_log(LOG_ERROR, "Can't read message template from file: %s\n", filenamebuf);
		return NULL;
	}
	writepos = buf;
	while (fgets(readbuf, sizeof(readbuf), fi)) {
		lines ++;
		if (writepos != buf) {
			*writepos = '\n';		/* Replace EOL with new line */
			writepos++;
		}
		ast_copy_string(writepos, readbuf, sizeof(buf) - (writepos - buf));
		writepos += strlen(readbuf) - 1;
	}
	fclose(fi);
	messagebody = ast_calloc(1, strlen(buf + 1));
	ast_copy_string(messagebody, buf, strlen(buf) + 1);
	ast_debug(4, "---> Size of allocation %d\n", (int) strlen(buf + 1) );
	ast_debug(4, "---> Done reading message template : \n%s\n---- END message template--- \n", messagebody);

	return messagebody;
}

/*! \brief Parse emailbody template from configuration file */
static char *message_template_parse_emailbody(const char *configuration)
{
	char *tmpread, *tmpwrite;
	char *emailbody = ast_strdup(configuration);

	/* substitute strings \t and \n into the apropriate characters */
	tmpread = tmpwrite = emailbody;
	while ((tmpwrite = strchr(tmpread,'\\'))) {
	       int len = strlen("\n");
	       switch (tmpwrite[1]) {
	       case 'n':
		      memmove(tmpwrite + len, tmpwrite + 2, strlen(tmpwrite + 2) + 1);
		      strncpy(tmpwrite, "\n", len);
		      break;
	       case 't':
		      memmove(tmpwrite + len, tmpwrite + 2, strlen(tmpwrite + 2) + 1);
		      strncpy(tmpwrite, "\t", len);
		      break;
	       default:
		      ast_log(LOG_NOTICE, "Substitution routine does not support this character: %c\n", tmpwrite[1]);
	       }
	       tmpread = tmpwrite + len;
	}
	return emailbody;	
}

/*! \brief Apply general configuration options */
static int apply_general_options(struct ast_variable *var)
{
	int error = 0;

	while (var) {
		/* Mail command */
		if (!strcmp(var->name, "mailcmd")) {
			ast_copy_string(global_mailcmd, var->value, sizeof(global_mailcmd)); /* User setting */
		} else if (!strcmp(var->name, "maxgreet")) {
			global_maxgreet = atoi(var->value);
		} else if (!strcmp(var->name, "maxsilence")) {
			global_maxsilence = atoi(var->value);
			if (global_maxsilence > 0)
				global_maxsilence *= 1000;
		} else if (!strcmp(var->name, "logfile")) {
			if (!ast_strlen_zero(var->value) ) {
				if(*(var->value) == '/')
					ast_copy_string(global_logfile, var->value, sizeof(global_logfile));
				else
					snprintf(global_logfile, sizeof(global_logfile), "%s/%s", ast_config_AST_LOG_DIR, var->value);
			}
		} else if (!strcmp(var->name, "externnotify")) {
			/* External voicemail notify application */
			ast_copy_string(global_externnotify, var->value, sizeof(global_externnotify));
		} else if (!strcmp(var->name, "silencetreshold")) {
			/* Silence treshold */
			global_silencethreshold = atoi(var->value);
		} else if (!strcmp(var->name, "maxmessage")) {
			int x;
			if (sscanf(var->value, "%d", &x) == 1) {
				global_vmmaxmessage = x;
			} else {
				error ++;
				ast_log(LOG_WARNING, "Invalid max message time length\n");
			}
		} else if (!strcmp(var->name, "minmessage")) {
			int x;
			if (sscanf(var->value, "%d", &x) == 1) {
				global_vmminmessage = x;
				if (global_maxsilence <= global_vmminmessage)
					ast_log(LOG_WARNING, "maxsilence should be less than minmessage or you may get empty messages\n");
			} else {
				error ++;
				ast_log(LOG_WARNING, "Invalid min message time length\n");
			}
		} else if (!strcmp(var->name, "format")) {
			ast_copy_string(default_vmformat, var->value, sizeof(default_vmformat));
		} else if (!strcmp(var->name, "review")) {
			ast_set2_flag((&globalflags), ast_true(var->value), MVM_REVIEW);	
		} else if (!strcmp(var->name, "operator")) {
			ast_set2_flag((&globalflags), ast_true(var->value), MVM_OPERATOR);	
		}
		var = var->next;
	}
	return error;
}

/*! \brief Load minivoicemail configuration */
static int load_config(int reload)
{
	struct ast_config *cfg;
	struct ast_variable *var;
	char *cat;
	const char *chanvar;
	int error = 0;
	struct minivm_template *template;
	struct ast_flags config_flags = { reload ? CONFIG_FLAG_FILEUNCHANGED : 0 };

	cfg = ast_config_load(VOICEMAIL_CONFIG, config_flags);
	if (cfg == CONFIG_STATUS_FILEUNCHANGED)
		return 0;

	ast_mutex_lock(&minivmlock);

	/* Destroy lists to reconfigure */
	message_destroy_list();		/* Destroy list of voicemail message templates */
	timezone_destroy_list();	/* Destroy list of timezones */
	vmaccounts_destroy_list();	/* Destroy list of voicemail accounts */
	ast_debug(2, "Destroyed memory objects...\n");

	/* First, set some default settings */
	global_externnotify[0] = '\0';
	global_logfile[0] = '\0';
	global_silencethreshold = 256;
	global_vmmaxmessage = 2000;
	global_maxgreet = 2000;
	global_vmminmessage = 0;
	strcpy(global_mailcmd, SENDMAIL);
	global_maxsilence = 0;
	global_saydurationminfo = 2;
	ast_copy_string(default_vmformat, "wav", sizeof(default_vmformat));
	ast_set2_flag((&globalflags), FALSE, MVM_REVIEW);	
	ast_set2_flag((&globalflags), FALSE, MVM_OPERATOR);	
	strcpy(global_charset, "ISO-8859-1");
	/* Reset statistics */
	memset(&global_stats, 0, sizeof(global_stats));
	global_stats.reset = ast_tvnow();

	/* Make sure we could load configuration file */
	if (!cfg) {
		ast_log(LOG_WARNING, "Failed to load configuration file. Module activated with default settings.\n");
		ast_mutex_unlock(&minivmlock);
		return 0;
	}

	ast_debug(2, "-_-_- Loaded configuration file, now parsing\n");

	/* General settings */

	cat = ast_category_browse(cfg, NULL);
	while (cat) {
		ast_debug(3, "-_-_- Found configuration section [%s]\n", cat);
		if (!strcasecmp(cat, "general")) {
			/* Nothing right now */
			error += apply_general_options(ast_variable_browse(cfg, cat));
		} else if (!strncasecmp(cat, "template-", 9))  {
			/* Template */
			char *name = cat + 9;

			/* Now build and link template to list */
			error += message_template_build(name, ast_variable_browse(cfg, cat));
		} else {
			var = ast_variable_browse(cfg, cat);
			if (!strcasecmp(cat, "zonemessages")) {
				/* Timezones in this context */
				while (var) {
					timezone_add(var->name, var->value);
					var = var->next;
				}
			} else {
				/* Create mailbox from this */
				error += create_vmaccount(cat, var, FALSE);
			}
		}
		/* Find next section in configuration file */
		cat = ast_category_browse(cfg, cat);
	}

	/* Configure the default email template */
	message_template_build("email-default", NULL);
	template = message_template_find("email-default");

	/* Load date format config for voicemail mail */
	if ((chanvar = ast_variable_retrieve(cfg, "general", "emaildateformat"))) 
		ast_copy_string(template->dateformat, chanvar, sizeof(template->dateformat));
	if ((chanvar = ast_variable_retrieve(cfg, "general", "emailfromstring")))
		ast_copy_string(template->fromaddress, chanvar, sizeof(template->fromaddress));
	if ((chanvar = ast_variable_retrieve(cfg, "general", "emailaaddress")))
		ast_copy_string(template->serveremail, chanvar, sizeof(template->serveremail));
	if ((chanvar = ast_variable_retrieve(cfg, "general", "emailcharset")))
		ast_copy_string(template->charset, chanvar, sizeof(template->charset));
	if ((chanvar = ast_variable_retrieve(cfg, "general", "emailsubject"))) 
		ast_copy_string(template->subject, chanvar, sizeof(template->subject));
	if ((chanvar = ast_variable_retrieve(cfg, "general", "emailbody"))) 
		template->body = message_template_parse_emailbody(chanvar);
	template->attachment = TRUE;

	message_template_build("pager-default", NULL);
	template = message_template_find("pager-default");
	if ((chanvar = ast_variable_retrieve(cfg, "general", "pagerfromstring")))
		ast_copy_string(template->fromaddress, chanvar, sizeof(template->fromaddress));
	if ((chanvar = ast_variable_retrieve(cfg, "general", "pageraddress")))
		ast_copy_string(template->serveremail, chanvar, sizeof(template->serveremail));
	if ((chanvar = ast_variable_retrieve(cfg, "general", "pagercharset")))
		ast_copy_string(template->charset, chanvar, sizeof(template->charset));
	if ((chanvar = ast_variable_retrieve(cfg, "general", "pagersubject")))
		ast_copy_string(template->subject, chanvar,sizeof(template->subject));
	if ((chanvar = ast_variable_retrieve(cfg, "general", "pagerbody"))) 
		template->body = message_template_parse_emailbody(chanvar);
	template->attachment = FALSE;

	if (error)
		ast_log(LOG_ERROR, "--- A total of %d errors found in mini-voicemail configuration\n", error);

	ast_mutex_unlock(&minivmlock);
	ast_config_destroy(cfg);

	/* Close log file if it's open and disabled */
	if(minivmlogfile)
		fclose(minivmlogfile);

	/* Open log file if it's enabled */
	if(!ast_strlen_zero(global_logfile)) {
		minivmlogfile = fopen(global_logfile, "a");
		if(!minivmlogfile)
			ast_log(LOG_ERROR, "Failed to open minivm log file %s : %s\n", global_logfile, strerror(errno));
		if (minivmlogfile)
			ast_debug(3, "-_-_- Opened log file %s \n", global_logfile);
	}

	return 0;
}

static const char minivm_show_users_help[] =
"Usage: minivm list accounts\n"
"       Lists all mailboxes currently set up\n";

static const char minivm_show_zones_help[] =
"Usage: minivm list zones\n"
"       Lists zone message formats\n";

static const char minivm_list_templates_help[] =
"Usage: minivm list templates\n"
"       Lists message templates for e-mail, paging and IM\n";

static const char minivm_show_stats_help[] =
"Usage: minivm show stats\n"
"       Display Mini-Voicemail counters\n";

static const char minivm_show_settings_help[] =
"Usage: minivm show settings\n"
"       Display Mini-Voicemail general settings\n";

static const char minivm_reload_help[] =
"Usage: minivm reload\n"
"       Reload mini-voicemail configuration and reset statistics\n";

/*! \brief CLI routine for listing templates */
static int handle_minivm_list_templates(int fd, int argc, char *argv[])
{
	struct minivm_template *this;
	char *output_format = "%-15s %-10s %-10s %-15.15s %-50s\n";
	int count = 0;

	if (argc > 3)
		return RESULT_SHOWUSAGE;

	AST_LIST_LOCK(&message_templates);
	if (AST_LIST_EMPTY(&message_templates)) {
		ast_cli(fd, "There are no message templates defined\n");
		AST_LIST_UNLOCK(&message_templates);
		return RESULT_FAILURE;
	}
	ast_cli(fd, output_format, "Template name", "Charset", "Locale", "Attach media", "Subject");
	ast_cli(fd, output_format, "-------------", "-------", "------", "------------", "-------");
	AST_LIST_TRAVERSE(&message_templates, this, list) {
		ast_cli(fd, output_format, this->name, 
			this->charset ? this->charset : "-", 
			this->locale ? this->locale : "-",
			this->attachment ? "Yes" : "No",
			this->subject ? this->subject : "-");
		count++;
	}
	AST_LIST_UNLOCK(&message_templates);
	ast_cli(fd, "\n * Total: %d minivoicemail message templates\n", count);
	return RESULT_SUCCESS;
}

/*! \brief CLI command to list voicemail accounts */
static int handle_minivm_show_users(int fd, int argc, char *argv[])
{
	struct minivm_account *vmu;
	char *output_format = "%-23s %-15s %-15s %-10s %-10s %-50s\n";
	int count = 0;

	if ((argc < 3) || (argc > 5) || (argc == 4))
		return RESULT_SHOWUSAGE;
	if ((argc == 5) && strcmp(argv[3],"for"))
		return RESULT_SHOWUSAGE;

	AST_LIST_LOCK(&minivm_accounts);
	if (AST_LIST_EMPTY(&minivm_accounts)) {
		ast_cli(fd, "There are no voicemail users currently defined\n");
		AST_LIST_UNLOCK(&minivm_accounts);
		return RESULT_FAILURE;
	}
	ast_cli(fd, output_format, "User", "E-Template", "P-template", "Zone", "Format", "Full name");
	ast_cli(fd, output_format, "----", "----------", "----------", "----", "------", "---------");
	AST_LIST_TRAVERSE(&minivm_accounts, vmu, list) {
		char tmp[256] = "";


		if ((argc == 3) || ((argc == 5) && !strcmp(argv[4], vmu->domain))) {
			count++;
			snprintf(tmp, sizeof(tmp), "%s@%s", vmu->username, vmu->domain);
			ast_cli(fd, output_format, tmp, vmu->etemplate ? vmu->etemplate : "-", 
				vmu->ptemplate ? vmu->ptemplate : "-",
				vmu->zonetag ? vmu->zonetag : "-", 
				vmu->attachfmt ? vmu->attachfmt : "-",
				vmu->fullname);
		}
	}
	AST_LIST_UNLOCK(&minivm_accounts);
	ast_cli(fd, "\n * Total: %d minivoicemail accounts\n", count);
	return RESULT_SUCCESS;
}

/*! \brief Show a list of voicemail zones in the CLI */
static int handle_minivm_show_zones(int fd, int argc, char *argv[])
{
	struct minivm_zone *zone;
	char *output_format = "%-15s %-20s %-45s\n";
	int res = RESULT_SUCCESS;

	if (argc != 3)
		return RESULT_SHOWUSAGE;

	AST_LIST_LOCK(&minivm_zones);
	if (!AST_LIST_EMPTY(&minivm_zones)) {
		ast_cli(fd, output_format, "Zone", "Timezone", "Message Format");
		ast_cli(fd, output_format, "----", "--------", "--------------");
		AST_LIST_TRAVERSE(&minivm_zones, zone, list) {
			ast_cli(fd, output_format, zone->name, zone->timezone, zone->msg_format);
		}
	} else {
		ast_cli(fd, "There are no voicemail zones currently defined\n");
		res = RESULT_FAILURE;
	}
	AST_LIST_UNLOCK(&minivm_zones);

	return res;
}


static char *complete_minivm_show_users(const char *line, const char *word, int pos, int state)
{
	int which = 0;
	int wordlen;
	struct minivm_account *vmu;
	const char *domain = "";

	/* 0 - show; 1 - voicemail; 2 - users; 3 - for; 4 - <domain> */
	if (pos > 4)
		return NULL;
	if (pos == 3)
		return (state == 0) ? ast_strdup("for") : NULL;
	wordlen = strlen(word);
	AST_LIST_TRAVERSE(&minivm_accounts, vmu, list) {
		if (!strncasecmp(word, vmu->domain, wordlen)) {
			if (domain && strcmp(domain, vmu->domain) && ++which > state)
				return ast_strdup(vmu->domain);
			/* ignore repeated domains ? */
			domain = vmu->domain;
		}
	}
	return NULL;
}

/*! \brief CLI Show settings */
static int handle_minivm_show_settings(int fd, int argc, char *argv[])
{
	ast_cli(fd, "* Mini-Voicemail general settings\n");
	ast_cli(fd, "  -------------------------------\n");
	ast_cli(fd, "\n");
	ast_cli(fd, "  Mail command (shell):               %s\n", global_mailcmd);
	ast_cli(fd, "  Max silence:                        %d\n", global_maxsilence);
	ast_cli(fd, "  Silence treshold:                   %d\n", global_silencethreshold);
	ast_cli(fd, "  Max message length (secs):          %d\n", global_vmmaxmessage);
	ast_cli(fd, "  Min message length (secs):          %d\n", global_vmminmessage);
	ast_cli(fd, "  Default format:                     %s\n", default_vmformat);
	ast_cli(fd, "  Extern notify (shell):              %s\n", global_externnotify);
	ast_cli(fd, "  Logfile:                            %s\n", global_logfile[0] ? global_logfile : "<disabled>");
	ast_cli(fd, "  Operator exit:                      %s\n", ast_test_flag(&globalflags, MVM_OPERATOR) ? "Yes" : "No");
	ast_cli(fd, "  Message review:                     %s\n", ast_test_flag(&globalflags, MVM_REVIEW) ? "Yes" : "No");

	ast_cli(fd, "\n");
	return RESULT_SUCCESS;
}

/*! \brief Show stats */
static int handle_minivm_show_stats(int fd, int argc, char *argv[])
{
	struct ast_tm time;
	char buf[BUFSIZ];

	ast_cli(fd, "* Mini-Voicemail statistics\n");
	ast_cli(fd, "  -------------------------\n");
	ast_cli(fd, "\n");
	ast_cli(fd, "  Voicemail accounts:                  %-5.5d\n", global_stats.voicemailaccounts);
	ast_cli(fd, "  Templates:                           %-5.5d\n", global_stats.templates);
	ast_cli(fd, "  Timezones:                           %-5.5d\n", global_stats.timezones);
	if (global_stats.receivedmessages == 0) {
		ast_cli(fd, "  Received messages since last reset:  <none>\n");
	} else {
		ast_cli(fd, "  Received messages since last reset:  %d\n", global_stats.receivedmessages);
		ast_localtime(&global_stats.lastreceived, &time, NULL);
		ast_strftime(buf, sizeof(buf), "%a %b %e %r %Z %Y", &time);
		ast_cli(fd, "  Last received voicemail:             %s\n", buf);
	}
	ast_localtime(&global_stats.reset, &time, NULL);
	ast_strftime(buf, sizeof(buf), "%a %b %e %r %Z %Y", &time);
	ast_cli(fd, "  Last reset:                          %s\n", buf);

	ast_cli(fd, "\n");
	return RESULT_SUCCESS;
}

/*! \brief  ${MINIVMACCOUNT()} Dialplan function - reads account data */
static int minivm_account_func_read(struct ast_channel *chan, const char *cmd, char *data, char *buf, size_t len)
{
	struct minivm_account *vmu;
	char *username, *domain, *colname;

	if (!(username = ast_strdupa(data))) {
		ast_log(LOG_ERROR, "Memory Error!\n");
		return -1;
	}

	if ((colname = strchr(username, ':'))) {
		*colname = '\0';
		colname++;
	} else {
		colname = "path";
	}
	if ((domain = strchr(username, '@'))) {
		*domain = '\0';
		domain++;
	}
	if (ast_strlen_zero(username) || ast_strlen_zero(domain)) {
		ast_log(LOG_ERROR, "This function needs a username and a domain: username@domain\n");
		return 0;
	}

	if (!(vmu = find_account(domain, username, TRUE)))
		return 0;

	if (!strcasecmp(colname, "hasaccount")) {
		ast_copy_string(buf, (ast_test_flag(vmu, MVM_ALLOCED) ? "0" : "1"), len);
	} else  if (!strcasecmp(colname, "fullname")) { 
		ast_copy_string(buf, vmu->fullname, len);
	} else  if (!strcasecmp(colname, "email")) { 
		if (!ast_strlen_zero(vmu->email))
			ast_copy_string(buf, vmu->email, len);
		else
			snprintf(buf, len, "%s@%s", vmu->username, vmu->domain);
	} else  if (!strcasecmp(colname, "pager")) { 
		ast_copy_string(buf, vmu->pager, len);
	} else  if (!strcasecmp(colname, "etemplate")) { 
		if (!ast_strlen_zero(vmu->etemplate))
			ast_copy_string(buf, vmu->etemplate, len);
		else
			ast_copy_string(buf, "email-default", len);
	} else  if (!strcasecmp(colname, "language")) { 
		ast_copy_string(buf, vmu->language, len);
	} else  if (!strcasecmp(colname, "timezone")) { 
		ast_copy_string(buf, vmu->zonetag, len);
	} else  if (!strcasecmp(colname, "ptemplate")) { 
		if (!ast_strlen_zero(vmu->ptemplate))
			ast_copy_string(buf, vmu->ptemplate, len);
		else
			ast_copy_string(buf, "email-default", len);
	} else  if (!strcasecmp(colname, "accountcode")) {
		ast_copy_string(buf, vmu->accountcode, len);
	} else  if (!strcasecmp(colname, "pincode")) {
		ast_copy_string(buf, vmu->pincode, len);
	} else  if (!strcasecmp(colname, "path")) {
		check_dirpath(buf, len, vmu->domain, vmu->username, NULL);
	} else {	/* Look in channel variables */
		struct ast_variable *var;
		int found = 0;

		for (var = vmu->chanvars ; var ; var = var->next)
			if (!strcmp(var->name, colname)) {
				ast_copy_string(buf, var->value, len);
				found = 1;
				break;
			}
	}

	if(ast_test_flag(vmu, MVM_ALLOCED))
		free_user(vmu);

	return 0;
}

/*! \brief lock directory

   only return failure if ast_lock_path returns 'timeout',
   not if the path does not exist or any other reason
*/
static int vm_lock_path(const char *path)
{
        switch (ast_lock_path(path)) {
        case AST_LOCK_TIMEOUT:
                return -1;
        default:
                return 0;
        }
}

/*! \brief Access counter file, lock directory, read and possibly write it again changed 
	\param directory	Directory to crate file in
	\param countername	filename 
	\param value		If set to zero, we only read the variable
	\param operand		0 to read, 1 to set new value, 2 to change 
	\return -1 on error, otherwise counter value
*/
static int access_counter_file(char *directory, char *countername, int value, int operand)
{
	char filename[BUFSIZ];
	char readbuf[BUFSIZ];
	FILE *counterfile;
	int old = 0, counter = 0;

	/* Lock directory */
	if (vm_lock_path(directory)) {
		return -1;	/* Could not lock directory */
	}
	snprintf(filename, sizeof(filename), "%s/%s.counter", directory, countername);
	if (operand != 1) {
		counterfile = fopen(filename, "r");
		if (counterfile) {
			if(fgets(readbuf, sizeof(readbuf), counterfile)) {
				ast_debug(3, "Read this string from counter file: %s\n", readbuf);
				old = counter = atoi(readbuf);
			}
			fclose(counterfile);
		}
	}
	switch (operand) {
	case 0:	/* Read only */
		ast_unlock_path(directory);
		ast_debug(2, "MINIVM Counter %s/%s: Value %d\n", directory, countername, counter);
		return counter;
		break;
	case 1: /* Set new value */
		counter = value;
		break;
	case 2: /* Change value */
		counter += value;
		if (counter < 0)	/* Don't allow counters to fall below zero */
			counter = 0;
		break;
	}
	
	/* Now, write the new value to the file */
	counterfile = fopen(filename, "w");
	if (!counterfile) {
		ast_log(LOG_ERROR, "Could not open counter file for writing : %s - %s\n", filename, strerror(errno));
		ast_unlock_path(directory);
		return -1;	/* Could not open file for writing */
	}
	fprintf(counterfile, "%d\n\n", counter);
	fclose(counterfile);
	ast_unlock_path(directory);
	ast_debug(2, "MINIVM Counter %s/%s: Old value %d New value %d\n", directory, countername, old, counter);
	return counter;
}

/*! \brief  ${MINIVMCOUNTER()} Dialplan function - read counters */
static int minivm_counter_func_read(struct ast_channel *chan, const char *cmd, char *data, char *buf, size_t len)
{
	char *username, *domain, *countername;
	struct minivm_account *vmu = NULL;
	char userpath[BUFSIZ];
	int res;

	*buf = '\0';

	if (!(username = ast_strdupa(data))) {	/* Copy indata to local buffer */
		ast_log(LOG_WARNING, "Memory error!\n");
		return -1;
	}
	if ((countername = strchr(username, ':'))) {
		*countername = '\0';
		countername++;
	} 

	if ((domain = strchr(username, '@'))) {
		*domain = '\0';
		domain++;
	}

	/* If we have neither username nor domain now, let's give up */
	if (ast_strlen_zero(username) && ast_strlen_zero(domain)) {
		ast_log(LOG_ERROR, "No account given\n");
		return -1;
	}

	if (ast_strlen_zero(countername)) {
		ast_log(LOG_ERROR, "This function needs two arguments: Account:countername\n");
		return -1;
	}

	/* We only have a domain, no username */
	if (!ast_strlen_zero(username) && ast_strlen_zero(domain)) {
		domain = username;
		username = NULL;
	}

	/* If we can't find account or if the account is temporary, return. */
	if (!ast_strlen_zero(username) && !(vmu = find_account(domain, username, FALSE))) {
		ast_log(LOG_ERROR, "Minivm account does not exist: %s@%s\n", username, domain);
		return 0;
	}

	create_dirpath(userpath, sizeof(userpath), domain, username, NULL);

	/* We have the path, now read the counter file */
	res = access_counter_file(userpath, countername, 0, 0);
	if (res >= 0)
		snprintf(buf, len, "%d", res);
	return 0;
}

/*! \brief  ${MINIVMCOUNTER()} Dialplan function - changes counter data */
static int minivm_counter_func_write(struct ast_channel *chan, const char *cmd, char *data, const char *value)
{
	char *username, *domain, *countername, *operand;
	char userpath[BUFSIZ];
	struct minivm_account *vmu;
	int change = 0;
	int operation = 0;

	if(!value)
		return -1;
	change = atoi(value);

	if (!(username = ast_strdupa(data))) {	/* Copy indata to local buffer */
		ast_log(LOG_WARNING, "Memory error!\n");
		return -1;
	}

	if ((countername = strchr(username, ':'))) {
		*countername = '\0';
		countername++;
	} 
	if ((operand = strchr(countername, ':'))) {
		*operand = '\0';
		operand++;
	} 

	if ((domain = strchr(username, '@'))) {
		*domain = '\0';
		domain++;
	}

	/* If we have neither username nor domain now, let's give up */
	if (ast_strlen_zero(username) && ast_strlen_zero(domain)) {
		ast_log(LOG_ERROR, "No account given\n");
		return -1;
	}

	/* We only have a domain, no username */
	if (!ast_strlen_zero(username) && ast_strlen_zero(domain)) {
		domain = username;
		username = NULL;
	}

	if (ast_strlen_zero(operand) || ast_strlen_zero(countername)) {
		ast_log(LOG_ERROR, "Writing to this function requires three arguments: Account:countername:operand\n");
		return -1;
	}

	/* If we can't find account or if the account is temporary, return. */
	if (!ast_strlen_zero(username) && !(vmu = find_account(domain, username, FALSE))) {
		ast_log(LOG_ERROR, "Minivm account does not exist: %s@%s\n", username, domain);
		return 0;
	}

	create_dirpath(userpath, sizeof(userpath), domain, username, NULL);
	/* Now, find out our operator */
	if (*operand == 'i') /* Increment */
		operation = 2;
	else if (*operand == 'd') {
		change = change * -1;
		operation = 2;
	} else if (*operand == 's')
		operation = 1;
	else {
		ast_log(LOG_ERROR, "Unknown operator: %s\n", operand);
		return -1;
	}

	/* We have the path, now read the counter file */
	access_counter_file(userpath, countername, change, operation);
	return 0;
}


/*! \brief CLI commands for Mini-voicemail */
static struct ast_cli_entry cli_minivm[] = {
	{ { "minivm", "list", "accounts", NULL },
	handle_minivm_show_users, "List defined mini-voicemail boxes",
	minivm_show_users_help, complete_minivm_show_users, NULL },

	{ { "minivm", "list", "zones", NULL },
	handle_minivm_show_zones, "List zone message formats",
	minivm_show_zones_help, NULL, NULL },

	{ { "minivm", "list", "templates", NULL },
	handle_minivm_list_templates, "List message templates",
	minivm_list_templates_help, NULL, NULL },

	{ { "minivm", "reload", NULL, NULL },
	handle_minivm_reload, "Reload Mini-voicemail configuration",
	minivm_reload_help, NULL, NULL },

	{ { "minivm", "show", "stats", NULL },
	handle_minivm_show_stats, "Show some mini-voicemail statistics",
	minivm_show_stats_help, NULL, NULL },

	{ { "minivm", "show", "settings", NULL },
	handle_minivm_show_settings, "Show mini-voicemail general settings",
	minivm_show_settings_help, NULL, NULL },
};

static struct ast_custom_function minivm_counter_function = {
	.name = "MINIVMCOUNTER",
	.synopsis = "Reads or sets counters for MiniVoicemail message",
	.syntax = "MINIVMCOUNTER(<account>:name[:operand])",
	.read = minivm_counter_func_read,
	.write = minivm_counter_func_write,
	.desc = "Valid operands for changing the value of a counter when assigning a value are:\n"
	"- i   Increment by value\n"
	"- d   Decrement by value\n"
	"- s   Set to value\n"
	"\nThe counters never goes below zero.\n"
	"- The name of the counter is a string, up to 10 characters\n"
	"- If account is given and it exists, the counter is specific for the account\n"
	"- If account is a domain and the domain directory exists, counters are specific for a domain\n"
	"The operation is atomic and the counter is locked while changing the value\n"
	"\nThe counters are stored as text files in the minivm account directories. It might be better to use\n"
	"realtime functions if you are using a database to operate your Asterisk\n",
};

static struct ast_custom_function minivm_account_function = {
	.name = "MINIVMACCOUNT",
	.synopsis = "Gets MiniVoicemail account information",
	.syntax = "MINIVMACCOUNT(<account>:item)",
	.read = minivm_account_func_read,
	.desc = "Valid items are:\n"
	"- path           Path to account mailbox (if account exists, otherwise temporary mailbox)\n"
	"- hasaccount     1 if static Minivm account exists, 0 otherwise\n"
	"- fullname       Full name of account owner\n"
	"- email          Email address used for account\n"
	"- etemplate      E-mail template for account (default template if none is configured)\n"   
	"- ptemplate      Pager template for account (default template if none is configured)\n"   
	"- accountcode    Account code for voicemail account\n"
	"- pincode        Pin code for voicemail account\n"
	"- timezone       Time zone for voicemail account\n"
	"- language       Language for voicemail account\n"
	"- <channel variable name> Channel variable value (set in configuration for account)\n"
	"\n",
};

/*! \brief Load mini voicemail module */
static int load_module(void)
{
	int res;

	res = ast_register_application(app_minivm_record, minivm_record_exec, synopsis_minivm_record, descrip_minivm_record);
	res = ast_register_application(app_minivm_greet, minivm_greet_exec, synopsis_minivm_greet, descrip_minivm_greet);
	res = ast_register_application(app_minivm_notify, minivm_notify_exec, synopsis_minivm_notify, descrip_minivm_notify);
	res = ast_register_application(app_minivm_delete, minivm_delete_exec, synopsis_minivm_delete, descrip_minivm_delete);
	res = ast_register_application(app_minivm_accmess, minivm_accmess_exec, synopsis_minivm_accmess, descrip_minivm_accmess);

	ast_custom_function_register(&minivm_account_function);
	ast_custom_function_register(&minivm_counter_function);
	if (res)
		return(res);

	if ((res = load_config(0)))
		return(res);

	ast_cli_register_multiple(cli_minivm, sizeof(cli_minivm)/sizeof(cli_minivm[0]));

	/* compute the location of the voicemail spool directory */
	snprintf(MVM_SPOOL_DIR, sizeof(MVM_SPOOL_DIR), "%s/voicemail/", ast_config_AST_SPOOL_DIR);

	return res;
}

/*! \brief Reload mini voicemail module */
static int reload(void)
{
	return(load_config(1));
}

/*! \brief Reload cofiguration */
static int handle_minivm_reload(int fd, int argc, char *argv[])
{
	reload();
	ast_cli(fd, "\n-- Mini voicemail re-configured \n");
	return RESULT_SUCCESS;
}

/*! \brief Unload mini voicemail module */
static int unload_module(void)
{
	int res;
	
	res = ast_unregister_application(app_minivm_record);
	res |= ast_unregister_application(app_minivm_greet);
	res |= ast_unregister_application(app_minivm_notify);
	res |= ast_unregister_application(app_minivm_delete);
	res |= ast_unregister_application(app_minivm_accmess);
	ast_cli_unregister_multiple(cli_minivm, sizeof(cli_minivm)/sizeof(cli_minivm[0]));
	ast_custom_function_unregister(&minivm_account_function);
	ast_custom_function_unregister(&minivm_counter_function);

	message_destroy_list();		/* Destroy list of voicemail message templates */
	timezone_destroy_list();	/* Destroy list of timezones */
	vmaccounts_destroy_list();	/* Destroy list of voicemail accounts */

	return res;
}


AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_DEFAULT, "Mini VoiceMail (A minimal Voicemail e-mail System)",
		.load = load_module,
		.unload = unload_module,
		.reload = reload,
		);
