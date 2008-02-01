/* GKrellM
|  Copyright (C) 1999-2007 Bill Wilson
|
|  Author:  Bill Wilson    billw@gkrellm.net
|  Latest versions might be found at:  http://gkrellm.net
|
|
|  GKrellM is free software: you can redistribute it and/or modify it
|  under the terms of the GNU General Public License as published by
|  the Free Software Foundation, either version 3 of the License, or
|  (at your option) any later version.
|
|  GKrellM is distributed in the hope that it will be useful, but WITHOUT
|  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
|  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public
|  License for more details.
|
|  You should have received a copy of the GNU General Public License
|  along with this program. If not, see http://www.gnu.org/licenses/
*/

#include "gkrellm.h"
#include "gkrellm-private.h"

#include <utime.h>
#include <sys/time.h>
#include <errno.h>

#include	"pixmaps/mail/decal_mail.xpm"

#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
#define HAVE_MD5_H
#endif

#if defined(HAVE_GNUTLS)
#include <gnutls/openssl.h>
#include <gcrypt.h>
#include <pthread.h>
#define MD5Init		MD5_Init
#define MD5Update	MD5_Update
#define MD5Final	MD5_Final
GCRY_THREAD_OPTION_PTHREAD_IMPL;
#else
#if defined(HAVE_SSL)
#include <openssl/ssl.h>
#include <openssl/md5.h>
#define MD5Init		MD5_Init
#define MD5Update	MD5_Update
#define MD5Final	MD5_Final
#else
#if defined(HAVE_MD5_H)
#include <md5.h>
#else
#include "md5.h"
#endif
#endif
#endif

#include "ntlm.h"

#define MUTE_FLAG	-1

/* msg_count_mode has 3 states
*/
#define	MSG_NEW_TOTAL_COUNT	0
#define	MSG_NEW_COUNT		1
#define	MSG_NO_COUNT		2

/* animation_mode states
*/
#define	ANIMATION_NONE			0
#define	ANIMATION_ENVELOPE		1
#define	ANIMATION_PENGUIN		2
#define	ANIMATION_BOTH			3

/* # of seconds to wait for a response from a POP3 or IMAP server
*/
#define TCP_TIMEOUT			30
#define	DEFAULT_POP3_PORT	"110"
#define	DEFAULT_IMAP_PORT	"143"
#define	DEFAULT_IMAPS_PORT	"993"
#define	DEFAULT_POP3S_PORT	"995"


  /* A mailbox type has bits encoding how to check the mailbox (inline code
  |  check or threaded check).
  |  Threaded checks and the fetch program are remote checks
  */
#define	MBOX_CHECK_FETCH		0x1000
#define	MBOX_CHECK_INLINE		0x2000
#define	MBOX_CHECK_THREADED		0x4000
#define MBOX_CHECK_TYPE_MASK	0xf000

  /* Counts for mailboxes created and checked in other plugins can be shown */
#define	MBOX_EXTERNAL		0x10

  /* Mailboxes internally checked and created via the Mail->Mailboxes config */
#define MBOX_INTERNAL		0x20


  /* Here's the list of all the mailbox types the Mail monitor knows about.
  |  The MBOX_FETCH is a pseudo internal mailbox where the counts read from
  |  the fetch program are kept.  Additionally MBOX_FETCH_TOOLTIP types
  |  are constructed just so the fetch programs output lines can be 
  |  reported in a tooltip.  Real mailboxes that GKrellM creates in its
  |  config and knows how to check have MBOX_INTERNAL set.  And
  |  finally there can be external (plugin) mailboxes created which
  |  can have their check function called at the update intervals.  If the
  |  plugin reports back the count results, the animation/sound can be
  |  triggered for the plugin.  (Don't know if EXTERNAL guys will ever be used)
  |  Internal mailboxes can be remote or local.  Remote mailboxes have an
  |  authorization protocol that subdivides them into types.  Local mailboxes
  |  currently have separate mboxtype values but I may later group them
  |  into a MBOX_LOCAL type with a subdivision protocol like is currently
  |  done for remote mailboxes.
  */
#define	MBOX_FETCH		(MBOX_CHECK_FETCH)
#define	MBOX_MBOX		(MBOX_CHECK_INLINE   | MBOX_INTERNAL | 0)
#define	MBOX_MAILDIR	(MBOX_CHECK_INLINE   | MBOX_INTERNAL | 1)
#define	MBOX_MH_DIR		(MBOX_CHECK_INLINE   | MBOX_INTERNAL | 2)
#define	MBOX_REMOTE		(MBOX_CHECK_THREADED | MBOX_INTERNAL | 3)
#define	MBOX_FETCH_TOOLTIP	(6)

#define	MBOX_LOCAL_PLUGIN	(MBOX_CHECK_INLINE   | MBOX_EXTERNAL)
#define	MBOX_REMOTE_PLUGIN	(MBOX_CHECK_THREADED | MBOX_EXTERNAL)

#define	PROTO_POP3		0
#define	PROTO_IMAP		1

#define	AUTH_PLAINTEXT		0
#define	AUTH_USER		AUTH_PLAINTEXT		/* POP3 only */
#define	AUTH_APOP		1			/* POP3 only */
#define	AUTH_LOGIN		AUTH_PLAINTEXT		/* IMAP4 only */
#define	AUTH_CRAM_MD5		2
#define	AUTH_NTLM		3

#define	SSL_NONE		0
#define	SSL_TRANSPORT		1
#define	SSL_STARTTLS		2


  /* Authorization protocol strings to write into the config for remote
  |  mailboxes.
  */
typedef struct
	{
	gchar	*string;
	gint	protocol;
	gint	authmech;
	}
	AuthType;

static AuthType	auth_strings[] =
	{
	{ "POP3",		PROTO_POP3,	AUTH_USER },
	{ "POP3_(APOP)",	PROTO_POP3,	AUTH_APOP },
	{ "POP3_(CRAM-MD5)",	PROTO_POP3,	AUTH_CRAM_MD5 },
	{ "POP3_(NTLM)",	PROTO_POP3,	AUTH_NTLM },
	{ "IMAP",		PROTO_IMAP,	AUTH_LOGIN },
	{ "IMAP_(CRAM-MD5)",	PROTO_IMAP,	AUTH_CRAM_MD5 },
	{ "IMAP_(NTLM)",	PROTO_IMAP,	AUTH_NTLM },
	{ NULL,			-1,		-1 }
	};


  /* Save local mailbox type strings in the config in case I later change
  |  to an option_menu selection for subdividing a MBOX_LOCAL type.
  |  Currently local mailbox types are determined in get_local_mboxtype().
  */
static gchar	*mbox_strings[3] =
	{
	"mbox",
	"Maildir",
	"MH_mail"
	};

static GkrellmMonitor	*mon_mail;

typedef struct
	{
	gchar		*path,
				*homedir_path;
	gchar		*server;
	gchar		*username;
	gchar		*password;
	gchar		*imapfolder;
	gint		mboxtype;
	gint		protocol;
	gint		authmech;
	gint		port;
	gint		use_ssl;		/* Always SSL_NONE if !HAVE_SSL */
	}
	MailAccount;

typedef struct
	{
	MailAccount	*account;
	gboolean	busy;
	GString		*tcp_in;
	gboolean	(*check_func)();
	gpointer	data;			/* For external mailboxes (in plugins) */
	GThread*	thread;
	gint		mail_count;
	gint		new_mail_count;
	gint		old_mail_count;
	gint		prev_mail_count,
				prev_new_mail_count;
	time_t		last_mtime;
	off_t		last_size;
	gboolean	is_internal;	/* Internal mail message (ie: localmachine) */
	gboolean	need_animation,
				prev_need_animation;
	gchar		*warn_msg;
	gchar		*uidl;
	gboolean	warned;
	void		*private;
	}
	Mailbox;

static GList	*mailbox_list;

typedef struct
	{
	gchar	*command;
	GString	*read_gstring;			/* Bytes read from pipe stored here */
	gint	pipe;
	}
	Mailproc;

typedef struct
	{
	gint	fd;
#ifdef HAVE_SSL
	SSL	*ssl;
	SSL_CTX	*ssl_ctx;
#endif
	}
	ConnInfo;

Mailbox			*mail_fetch;		/* Internal mailbox: fetch command */

static Mailproc	mail_user_agent;
static gchar	*mail_notify;		/* Sound		*/

static GkrellmPiximage *decal_mail_piximage;

static gint		run_animation,
				decal_frame;

static gint		remote_check_timeout = 5;			/* Minutes */
static gint		local_check_timeout = 4;			/* Seconds */
static gboolean	fetch_check_is_local;

static GkrellmPanel	*mail;

static GtkTooltips	*tooltip;

static GkrellmDecalbutton	*mua_button;

static gboolean	enable_mail,
				mute_mode,
				super_mute_mode,
				cont_animation_mode,
				mua_inhibit_mode,		/* Inhibit checking if MUA launched */
				enable_multimua,		/* allow multiple MUA instances */
				count_mode,
				fetch_check_only_mode,
				reset_remote_mode,
				unseen_is_new,			/* Accessed but unread */
				local_supported = TRUE;

static gboolean	mh_seq_ignore,
				have_mh_sequences,
				checking_mh_mail;

static gint		animation_mode	= ANIMATION_BOTH;

static gboolean	force_mail_check;
static gint		new_mail_count, total_mail_count;
static gint		check_timeout;
static gint		show_tooltip = FALSE;

static gint		anim_frame,
				anim_dir,
				anim_pause;

static gint		style_id;

  /* This may be called from gkrellm_sys_main_init()
  */
void
gkrellm_mail_local_unsupported(void)
	{
	local_supported = FALSE;
	}

GThread *
gkrellm_mail_get_active_thread(void)
	{
	GList	*list;
	Mailbox	*mbox;
	GThread	*thread;

	for (list = mailbox_list; list; list = list->next)
		{
		mbox = (Mailbox *) list->data;
		thread = mbox->thread;
		if (thread)
			return thread;
		}
	return NULL;
	}

static void
free_account(MailAccount *account)
	{
	if (!account)
		return;
	g_free(account->path);
	g_free(account->homedir_path);
	g_free(account->server);
	g_free(account->username);
	g_free(account->password);
	g_free(account->imapfolder);
	g_free(account);
	}

static void
free_mailbox(Mailbox *mbox)
	{
	/* If user changes mailbox config list while a mailbox thread is busy,
	|  freeing the mbox can cause a segfault.   Rare, so allow the leak.
	*/
	if (mbox->busy)
		return;
	free_account(mbox->account);
	g_free(mbox->warn_msg);
	g_free(mbox);
	}


static gboolean
format_remote_mbox_name(Mailbox *mbox, gchar *buf, size_t len)
	{
	MailAccount	*account = mbox->account;

	if (account->imapfolder && *account->imapfolder)
		snprintf(buf, len, "%s-%s@%s", account->username,
			account->imapfolder, account->server);
	else if (account->server)
		snprintf(buf, len, "%s@%s", account->username, account->server);
	else if (account->username)
		snprintf(buf, len, "%s", account->username);
	else
		{
		snprintf(buf, len, "??");
		return FALSE;
		}
	return TRUE;
	}

  /* Make tooltip visible/invisible and fill it with mailbox names
  |  containing new mail.
  */
static void
update_tooltip(void)
	{
	GList		*list;
	Mailbox		*mbox;
	MailAccount	*account;
	GString		*mboxes = NULL;
	gchar		buf[128];
   
	if (show_tooltip)
		{
		mboxes = g_string_sized_new(512);
		for (list = mailbox_list; list; list = list->next)
			{
			mbox = (Mailbox *) list->data;
			account = mbox->account;
			if (mbox->new_mail_count > 0)
				{
				if ((    account->mboxtype == MBOX_MBOX
					  || account->mboxtype == MBOX_MAILDIR
					  || account->mboxtype == MBOX_MH_DIR
					  || account->mboxtype == MBOX_LOCAL_PLUGIN
					  || account->mboxtype == MBOX_REMOTE_PLUGIN
					) && account->path
				   )
					snprintf(buf, sizeof(buf), "%s", account->homedir_path ?
								account->homedir_path : account->path);
				else if (! format_remote_mbox_name(mbox, buf, sizeof(buf)))
					continue;	/* Can't get a name, so no tooltip for you! */

				if (mboxes->len > 0) 
					g_string_append_c(mboxes, '\n');
				g_string_append(mboxes, buf);
							
				if (count_mode == MSG_NEW_TOTAL_COUNT)
					snprintf(buf, sizeof(buf), "(%d/%d)",
						mbox->new_mail_count, mbox->mail_count);
				else
					snprintf(buf, sizeof(buf), "(%d)", mbox->new_mail_count);
				g_string_append(mboxes, buf);
 				}
			}
		}
	if (show_tooltip && mboxes && mboxes->len > 0)
		{
		gtk_tooltips_set_tip(tooltip, mail->drawing_area, mboxes->str, "");
		gtk_tooltips_enable(tooltip);
		}
	else
		gtk_tooltips_disable(tooltip);
	if (mboxes)
		g_string_free(mboxes, TRUE);
	}
	
	
  /* Look at a From line to see if it is valid, lines look like:
  |  From sending_address dayofweek month dayofmonth timeofday year
  |  eg: From billw@gkrellm.net Fri Oct 22 13:52:49 2010
  */
static gint
is_From_line(Mailbox *mbox, gchar *buf)
	{
	gchar	sender[512];
	gint	dayofmonth = 0;

	if (strncmp(buf, "From ", 5))
		return FALSE;

	/* In case sending address missing, look for a day of month
	|  number in field 3 or 4 (0 based).
	*/
	sender[0] = '\0';
	if (sscanf(buf, "%*s %*s %*s %d", &dayofmonth) != 1)
		{
		if (sscanf(buf, "%*s %511s %*s %*s %d", sender, &dayofmonth) != 2)
			return FALSE;
		}
	if (dayofmonth < 1 || dayofmonth > 31)
		return FALSE;
	if (strcmp(sender, "MAILER-DAEMON") == 0)
		mbox->is_internal = TRUE;
	return TRUE;
	}


  /* Check if this is a Content-Type-line. If it contains a boundary
  |  field, copy boundary string to buffer (including two leading and
  |  trailing dashes marking the end of a multipart mail) and return
  |  true. Otherwise, return false.
  */
static gint
is_multipart_mail(gchar *buf, gchar *separator)
	{
	gchar *fieldstart;
	gchar *sepstart;
	gint  seplen;
	
	if (strncmp(buf, "Content-Type: ", 14) != 0)
		return FALSE;
	if (strncmp(&buf[14], "multipart/", 10) != 0)
		return FALSE;
	fieldstart = &buf[14];
	while (*fieldstart!=0)
		{
		while (*fieldstart!=0 && *fieldstart!=';')
			fieldstart++;
		if (*fieldstart==';') fieldstart++;
		while (*fieldstart!=0 && *fieldstart==' ')
			fieldstart++;
		if (strncmp(fieldstart, "boundary=", 9) == 0)
			{
			sepstart = fieldstart + 9;
			if (sepstart[0]=='"')
				{
				sepstart++;
				seplen = 0;
				while (sepstart[seplen]!='"' && sepstart[seplen]>=32)
					seplen++;
				}
			else
				{
				seplen = 0;
				while (sepstart[seplen]!=';' && sepstart[seplen]>32)
					seplen++;
				}
			strcpy(separator,"--");
			strncpy(&separator[2],sepstart,seplen);
			strcpy(&separator[seplen+2],"--");
			return TRUE;
			}
		}
	return FALSE;
	}

  /* Hide a password that is embedded in a string.
  */
static void
hide_password(Mailbox *mbox, gchar *line, gint offset)
	{
	gint    n;

	n = strlen(mbox->account->password);
	while (n--)
		line[offset + n] = '*';
	}

static gint
read_select(gint fd, gchar *buf, size_t size, time_t timeout)
	{
	fd_set			readfds;
	struct timeval	tv;
	gint			n	= 0;
	gint			s;

	do
	{
	    FD_ZERO(&readfds);
	    FD_SET(fd, &readfds);
	    tv.tv_sec = timeout;
	    tv.tv_usec = 0;

	    if ((s = select(fd+1, &readfds, NULL, NULL, &tv)) > 0)
#if defined(WIN32)
		n = recv(fd, buf, size, 0);
#else
		n = read(fd, buf, size);
#endif
	} while (s < 0 && errno == EINTR);

	return n;
	}

  /* Read \r\n terminated lines from a remote IMAP or POP3 mail server,
  */
static void
tcp_getline(ConnInfo *conn, Mailbox *mbox)
	{
	gchar	buf[256];
	gint	n;
	gchar	*s;

	if (mbox->tcp_in)
		mbox->tcp_in = g_string_truncate(mbox->tcp_in, 0);
	else
		mbox->tcp_in = g_string_new("");
	s = buf;;
	for (;;)
		{
#ifdef HAVE_SSL
		if (conn->ssl)
			n = SSL_read(conn->ssl, s, 1);
		else
#endif
			n = read_select(conn->fd, s, 1, TCP_TIMEOUT);
		if (n <= 0)
			break;
		*(s+1) = '\0';
		if (*s++ == '\n')
			break;
		if (s >= buf + sizeof(buf) - 2)
			{
			g_string_append(mbox->tcp_in, buf);
			s = buf;
			}
		}
	if (s > buf)
		g_string_append(mbox->tcp_in, buf);

	if (_GK.debug_level & DEBUG_MAIL)
		{
		if (n < 0)
			perror("tcp_getline: ");
		format_remote_mbox_name(mbox, buf, sizeof(buf));
		printf("server_response( %s )<%d>:%s\n", buf,
			   (gint) mbox->tcp_in->len, mbox->tcp_in->str);
		}
	}

static void
tcp_putline(ConnInfo *conn, gchar *line)
	{
#ifdef HAVE_SSL
	if (conn->ssl)
		SSL_write(conn->ssl, line, strlen(line));
	else
#endif
		{
#if defined(WIN32)
		send(conn->fd, line, strlen(line), 0);
#else
		write(conn->fd, line, strlen (line));
#endif
		}
	}

  /* Get a server response line and verify the beginning of the line
  |  matches a string.
  */
static gboolean
server_response(ConnInfo *conn, Mailbox *mbox, gchar *match)
	{
	tcp_getline(conn, mbox);
	return (!strncmp(match, mbox->tcp_in->str, strlen(match)) ? TRUE : FALSE);
	}

  /* Get a imap server completion result response for a tagged command.
  |  Skip over any untagged responses the server may send.
  */
static gboolean
imap_completion_result(ConnInfo *conn, Mailbox *mbox, gchar *tag)
	{
	while (1)
		{
		tcp_getline(conn, mbox);
		if (*(mbox->tcp_in->str) == '*')	/* untagged response */
			continue;
		return (!strncmp(tag, mbox->tcp_in->str, strlen(tag)) ? TRUE : FALSE);
		}
	}

static void
server_command(ConnInfo *conn, Mailbox *mbox, gchar *line)
	{
	gchar	buf[128];

	tcp_putline(conn, line);

	if (_GK.debug_level & DEBUG_MAIL)
		{
		format_remote_mbox_name(mbox, buf, sizeof(buf));
		printf("server_command( %s ):%s", buf, line);
		}
	}

static gchar	*tcp_error_message[]	=
	{
	N_("Unable to connect."),
	N_("Bad response after connect."),
	N_("Bad response after username."),
	N_("Bad response after password."),
	N_("Bad response after STAT or STATUS."),
	N_("Bad response after UIDL."),
	N_("Bad APOP response after connect."),
	N_("Bad CRAM_MD5 response after connect."),
	};

static void
tcp_close(ConnInfo *conn)
	{
#ifdef HAVE_SSL
#ifndef HAVE_GNUTLS
	SSL_SESSION *session;
#endif
#endif

	if (conn->fd != -1)
		{
#ifdef WIN32
		closesocket(conn->fd);
#else
		close(conn->fd);
#endif
		conn->fd = -1;
		}
#ifdef HAVE_SSL
	if (conn->ssl)
		{
#ifndef HAVE_GNUTLS
		session = SSL_get_session(conn->ssl);
		if (session)
			SSL_CTX_remove_session(conn->ssl_ctx, session);
#endif
		SSL_free(conn->ssl);
		conn->ssl = NULL;
		}
	if (conn->ssl_ctx)
		{
		SSL_CTX_free(conn->ssl_ctx);
		conn->ssl_ctx = NULL;
		}
#endif
}

static gboolean
tcp_warn(Mailbox *mbox, gchar *message, gboolean warn)
	{
	gchar	buf[128];

  	if (_GK.debug_level & DEBUG_MAIL)
		{
		format_remote_mbox_name(mbox, buf, sizeof(buf));
		g_print(_("Mail TCP Error: %s - %s\n"), buf, _(message));
		}
	if (warn && !mbox->warned)
		{
		g_free(mbox->warn_msg);
		format_remote_mbox_name(mbox, buf, sizeof(buf));
		mbox->warn_msg = g_strdup_printf("%s\n%s\n%s\n", buf,
				_(message), mbox->tcp_in->str);
		}
	return FALSE;
	}

static gboolean
tcp_shutdown(ConnInfo *conn, Mailbox *mbox, gchar *message, gboolean warn)
	{
	tcp_close(conn);
	return tcp_warn(mbox, message, warn);
	}

#ifdef HAVE_SSL
static gboolean
ssl_negotiate(ConnInfo *conn, Mailbox *mbox)
	{
	SSL_METHOD	*ssl_method;

	SSLeay_add_ssl_algorithms();
	SSL_load_error_strings();

	if (mbox->account->use_ssl == SSL_TRANSPORT)
		ssl_method = SSLv23_client_method();
	else
		ssl_method = TLSv1_client_method();
	if (ssl_method == NULL)
		return tcp_shutdown(conn, mbox,
				    N_("Cannot initialize SSL method."),
				    FALSE);
	if ((conn->ssl_ctx = SSL_CTX_new(ssl_method)) == NULL)
		return tcp_shutdown(conn, mbox,
				    N_("Cannot initialize SSL server certificate handler."),
				    FALSE);
	SSL_CTX_set_options(conn->ssl_ctx, SSL_OP_ALL);
	SSL_CTX_set_verify(conn->ssl_ctx, SSL_VERIFY_NONE, NULL);
	if ((conn->ssl = SSL_new(conn->ssl_ctx)) == NULL)
		return tcp_shutdown(conn, mbox,
				    N_("Cannot initialize SSL handler."),
				    FALSE);
#ifndef HAVE_GNUTLS
	SSL_clear(conn->ssl);
#endif

	SSL_set_fd(conn->ssl, conn->fd);
	SSL_set_connect_state(conn->ssl);
	if (SSL_connect(conn->ssl) < 0)
		return tcp_shutdown(conn, mbox, tcp_error_message[0], FALSE);

	return TRUE;
	}
#endif

static gboolean
tcp_connect(ConnInfo *conn, Mailbox *mbox)
	{
	MailAccount		*account = mbox->account;
	gchar			buf[128];

	memset(conn, 0, sizeof(*conn));
	if (_GK.debug_level & DEBUG_MAIL)
		{
		format_remote_mbox_name(mbox, buf, sizeof(buf));
		printf("tcp_connect: connecting to %s\n", buf);
		}
	conn->fd = gkrellm_connect_to(account->server, account->port);
	if (conn->fd < 0)
		return tcp_warn(mbox, tcp_error_message[0], FALSE);
#ifdef HAVE_SSL
	if (account->use_ssl == SSL_TRANSPORT && !ssl_negotiate(conn, mbox))
		return FALSE;
#endif
	return TRUE;
	}

extern void to64frombits(unsigned char *, const unsigned char *, int);
extern int from64tobits(char *, const char *, int);

static void
hmac_md5(unsigned char *password,  size_t pass_len,
	 unsigned char *challenge, size_t chal_len,
	 unsigned char *response,  size_t resp_len)
	{
	int i;
	unsigned char ipad[64];
	unsigned char opad[64];
	unsigned char hash_passwd[16];

	MD5_CTX ctx;

	if (resp_len != 16)
		return;

	if (pass_len > sizeof(ipad))
		{
		MD5Init(&ctx);
		MD5Update(&ctx, password, pass_len);
		MD5Final(hash_passwd, &ctx);
		password = hash_passwd;
		pass_len = sizeof(hash_passwd);
		}

	memset(ipad, 0, sizeof(ipad));
	memset(opad, 0, sizeof(opad));
	memcpy(ipad, password, pass_len);
	memcpy(opad, password, pass_len);

	for (i = 0; i < 64; i++)
		{
		ipad[i] ^= 0x36;
		opad[i] ^= 0x5c;
		}

	MD5Init(&ctx);
	MD5Update(&ctx, ipad, sizeof(ipad));
	MD5Update(&ctx, challenge, chal_len);
	MD5Final(response, &ctx);

	MD5Init(&ctx);
	MD5Update(&ctx, opad, sizeof(opad));
	MD5Update(&ctx, response, resp_len);
	MD5Final(response, &ctx);
	}

/* authenticate as per RFC2195 */
static int
do_cram_md5(ConnInfo *conn, char *command, Mailbox  *mbox, char *strip)
	{
	MailAccount	*account = mbox->account;
	gint		len;
	gchar		buf1[1024];
	gchar		msg_id[768];
	gchar		reply[1024];
	gchar		*respdata;
	guchar		response[16];

	snprintf(buf1, sizeof(buf1), "%s CRAM-MD5\r\n", command);
	server_command(conn, mbox, buf1);

	/* From RFC2195:
	 * The data encoded in the first ready response contains an
	 * presumptively arbitrary string of random digits, a
	 * timestamp, and the * fully-qualified primary host name of
	 * the server.  The syntax of the * unencoded form must
	 * correspond to that of an RFC 822 'msg-id' * [RFC822] as
	 * described in [POP3].
	 */

	if (!server_response(conn, mbox, "+ "))
		return FALSE;

	/* caller may specify a response prefix we should strip if present */
	respdata = mbox->tcp_in->str;
	len = strlen(respdata);
	if (respdata[len - 1] == '\n')
		respdata[--len] = '\0';
	if (respdata[len - 1] == '\r')
		respdata[--len] = '\0';
	if (strip && strncmp(respdata, strip, strlen(strip)) == 0)
		respdata += strlen(strip);
	len = from64tobits(msg_id, respdata, sizeof(msg_id));

	if (len < 0)
		{
		if (_GK.debug_level & DEBUG_MAIL)
			g_print(_("could not decode BASE64 challenge\n"));
		return FALSE;
		}
	else if (len < sizeof(msg_id))
		msg_id[len] = 0;
	else
		msg_id[sizeof(msg_id) - 1] = 0;
	if (_GK.debug_level & DEBUG_MAIL)
		g_print(_("decoded as %s\n"), msg_id);

	/* The client makes note of the data and then responds with a string
	 * consisting of the user name, a space, and a 'digest'.  The latter is
	 * computed by applying the keyed MD5 algorithm from [KEYED-MD5] where
	 * the key is a shared secret and the digested text is the timestamp
	 * (including angle-brackets).
	 */

	hmac_md5((guchar *) account->password, strlen(account->password),
		 (guchar *) msg_id, strlen(msg_id), response, sizeof(response));

	snprintf(reply, sizeof(reply),
		"%s %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
		account->username,
		response[0], response[1], response[2], response[3],
		response[4], response[5], response[6], response[7],
		response[8], response[9], response[10], response[11],
		response[12], response[13], response[14], response[15]);

	to64frombits((guchar *) buf1, (guchar *) reply, strlen(reply));

	len = strlen(buf1);
	if (len + 3 > sizeof(buf1))
		return FALSE;
	strcpy(buf1 + len, "\r\n");
	server_command(conn, mbox, buf1);
	return TRUE;
	}

/* NTLM authentication */
static int
do_ntlm(ConnInfo *conn, char *command, Mailbox *mbox)
	{
	gint			len;
	gchar			msgbuf[2048];
	tSmbNtlmAuthRequest	request;
	tSmbNtlmAuthChallenge	challenge;
	tSmbNtlmAuthResponse	response;

	snprintf(msgbuf, sizeof(msgbuf), "%s NTLM\r\n", command);
	server_command(conn, mbox, msgbuf);

	if (!server_response(conn, mbox, "+ "))
		return FALSE;

	buildSmbNtlmAuthRequest(&request, mbox->account->username, NULL);
	if (_GK.debug_level & DEBUG_MAIL)
		dumpSmbNtlmAuthRequest(stdout, &request);
	memset(msgbuf, 0, sizeof(msgbuf));
	to64frombits((guchar *) msgbuf, (guchar *) &request, SmbLength(&request));
	len = strlen(msgbuf);
	if (len + 3 > sizeof(msgbuf))
		return FALSE;
	strcpy(msgbuf + len, "\r\n");
	server_command(conn, mbox, msgbuf);

	if (!server_response(conn, mbox, "+ "))
		return FALSE;

	len = from64tobits((char *)&challenge, mbox->tcp_in->str,
			   sizeof(challenge));
	if (len < 0)
		{
		if (_GK.debug_level & DEBUG_MAIL)
			g_print(_("could not decode BASE64 challenge\n"));
		return FALSE;
		}
	if (_GK.debug_level & DEBUG_MAIL)
		dumpSmbNtlmAuthChallenge(stdout, &challenge);

	buildSmbNtlmAuthResponse(&challenge, &response,
				 mbox->account->username,
				 mbox->account->password);
	if (_GK.debug_level & DEBUG_MAIL)
		dumpSmbNtlmAuthResponse(stdout, &response);
	memset(msgbuf, 0, sizeof msgbuf);
	to64frombits((guchar *)msgbuf, (guchar *) &response, SmbLength(&response));
	len = strlen(msgbuf);
	if (len + 3 > sizeof(msgbuf))
		return FALSE;
	strcpy(msgbuf + len, "\r\n");
	server_command(conn, mbox, msgbuf);
	return TRUE;
	}

static gboolean
check_pop3(Mailbox *mbox)
	{
	MailAccount		*account = mbox->account;
	ConnInfo		conn;
	gchar			line[128], buf[128];
	gchar			*challenge = NULL;

	if (!tcp_connect(&conn, mbox))
		return FALSE;

	/* Is the machine we are connected to really a POP3 server?
	*/
	if (! server_response(&conn, mbox, "+OK"))
		return tcp_shutdown(&conn, mbox, tcp_error_message[1], FALSE);

	if (account->authmech == AUTH_APOP &&
	    (strlen(mbox->tcp_in->str) < 3 ||
	     (challenge = g_strdup(mbox->tcp_in->str + 3)) == NULL))
		return tcp_shutdown(&conn, mbox, tcp_error_message[1], FALSE);

#ifdef HAVE_SSL
	if (account->use_ssl == SSL_STARTTLS)
		{
		server_command(&conn, mbox, "STLS\r\n");
		if (!server_response(&conn, mbox, "+OK"))
			{
			if (challenge)
				g_free(challenge);
			return tcp_shutdown(&conn, mbox,
					    N_("Bad response after STLS."),
					    FALSE);
			}
		if (!ssl_negotiate(&conn, mbox))
			{
			if (challenge)
				g_free(challenge);
			return FALSE;
			}
		}
#endif

	if (account->authmech == AUTH_APOP)
		{
		static const gchar hex[] = "0123456789abcdef";
		MD5_CTX ctx;
		gint	i;
		gchar	*key, *p;
		guchar	digest[16];
		gchar	ascii_digest[33];

		if ((key = strchr(challenge, '<')) == NULL)
			{
			g_free(challenge);
			return tcp_shutdown(&conn, mbox, tcp_error_message[6],
					    TRUE);
			}
		if ((p = strchr(key, '>')) == NULL)
			{
			g_free(challenge);
			return tcp_shutdown(&conn, mbox, tcp_error_message[6],
					    TRUE);
			}
		*(p + 1) = '\0';
		snprintf(line, sizeof(line), "%s%s", key, account->password);
		g_free(challenge);
		MD5Init(&ctx);
		MD5Update(&ctx, line, strlen(line));
		MD5Final(digest, &ctx);
		for (i = 0;  i < 16;  i++)
			{
			ascii_digest[i + i] = hex[digest[i] >> 4];
			ascii_digest[i + i + 1] = hex[digest[i] & 0x0f];
			}
		ascii_digest[i + i] = '\0';
		snprintf(line, sizeof(line),
			 "APOP %s %s\r\n", account->username, ascii_digest);
		server_command(&conn, mbox, line);
		}
	else if (account->authmech == AUTH_CRAM_MD5)
		{
		if (!do_cram_md5(&conn, "AUTH", mbox, NULL))
			{
			/* SASL cancellation of authentication */
			server_command(&conn, mbox, "*\r\n");
			return tcp_shutdown(&conn, mbox, tcp_error_message[7], TRUE);
			}
		}
	else if (account->authmech == AUTH_NTLM)
		{
		if (!do_ntlm(&conn, "AUTH", mbox))
			{
			/* SASL cancellation of authentication */
			server_command(&conn, mbox, "*\r\n");
			return tcp_shutdown(&conn, mbox, tcp_error_message[7], TRUE);
			}
		}
	else	/* AUTH_USER */
		{
		snprintf (line, sizeof (line), "USER %s\r\n", account->username);
		server_command(&conn, mbox, line);
		if (! server_response(&conn, mbox, "+OK"))
			return tcp_shutdown(&conn, mbox, tcp_error_message[2], TRUE);

		snprintf (line, sizeof (line), "PASS %s\r\n", account->password);
		tcp_putline(&conn, line);

		if (_GK.debug_level & DEBUG_MAIL)
			{
			hide_password(mbox, line, 5);
			format_remote_mbox_name(mbox, buf, sizeof(buf));
			printf("server_command( %s ):%s", buf, line);
			}
		}
	if (! server_response(&conn, mbox, "+OK"))
		return tcp_shutdown(&conn, mbox, tcp_error_message[3], TRUE);

	server_command(&conn, mbox, "STAT\r\n");
	if (! server_response(&conn, mbox, "+OK"))
		return tcp_shutdown(&conn, mbox, tcp_error_message[4], FALSE);

	sscanf(mbox->tcp_in->str, "+OK %d", &mbox->mail_count);
   	snprintf (line, sizeof (line), "UIDL %d\r\n", mbox->mail_count);
	server_command(&conn, mbox, line);

	if (! server_response(&conn, mbox, "+OK"))
		mbox->new_mail_count = mbox->mail_count;
	else
		/* Set the new_mail_count only if the UIDL is changed to avoid
		|  re-reporting mail is new after MUA button has been clicked.
		*/
		if (   sscanf(mbox->tcp_in->str, "+OK %*d %127s", line) == 1
			&& (   gkrellm_dup_string(&mbox->uidl, line)
				|| unseen_is_new
			   )
		   )
			mbox->new_mail_count = mbox->mail_count;

	server_command(&conn, mbox, "QUIT\r\n");
	tcp_close(&conn);
	return TRUE;
	}

static gboolean
check_imap(Mailbox *mbox)
	{
	MailAccount		*account = mbox->account;
	ConnInfo		conn;
	gint			messages = 0;
	gint			unseen = 0;
	gint			seq = 0;
	gchar			line[128], *ss;
	gchar			buf[128];

	if (!tcp_connect(&conn, mbox))
		return FALSE;

	/* Is the machine we are connected to really a IMAP server?
	*/
	if (! server_response(&conn, mbox, "* OK"))
		return tcp_shutdown(&conn, mbox, tcp_error_message[1], FALSE);

#ifdef HAVE_SSL
	if (account->use_ssl == SSL_STARTTLS)
		{
		snprintf(line, sizeof(line), "a%03d STARTTLS\r\n", ++seq);
		server_command(&conn, mbox, line);
		snprintf(line, sizeof(line), "a%03d OK", seq);
		if (!imap_completion_result(&conn, mbox, line))
			return tcp_shutdown(&conn, mbox,
					    N_("Bad response after STARTTLS."),
					    TRUE);
		if (!ssl_negotiate(&conn, mbox))
			return FALSE;
		}
#endif

	if (account->authmech == AUTH_CRAM_MD5)
		{
		snprintf(line, sizeof(line), "a%03d AUTHENTICATE", ++seq);
		if (!do_cram_md5(&conn, line, mbox, NULL))
			{
			/* SASL cancellation of authentication */
			server_command(&conn, mbox, "*\r\n");
			return tcp_shutdown(&conn, mbox, tcp_error_message[7], TRUE);
			}
		}
	else if (account->authmech == AUTH_NTLM)
		{
		snprintf(line, sizeof(line), "a%03d AUTHENTICATE", ++seq);
		if (!do_ntlm(&conn, line, mbox))
			{
			/* SASL cancellation of authentication */
			server_command(&conn, mbox, "*\r\n");
			return tcp_shutdown(&conn, mbox, tcp_error_message[7], TRUE);
			}
		}
	else	/* AUTH_LOGIN */
		{
		snprintf(line, sizeof(line), "a%03d LOGIN \"%s\" \"%s\"\r\n",
			 ++seq, account->username, account->password);
		tcp_putline(&conn, line);

		if (_GK.debug_level & DEBUG_MAIL)
			{
			line[10 + 2 + strlen(account->username)] = '\0';
			hide_password(mbox, line, 11 + 2 + strlen(account->username));
			format_remote_mbox_name(mbox, buf, sizeof(buf));
			printf("server_command( %s ):%s", buf, line);
			}
		}
	snprintf(line, sizeof(line), "a%03d OK", seq);
	if (! imap_completion_result(&conn, mbox, line))
		return tcp_shutdown(&conn, mbox, tcp_error_message[2], TRUE);

	/* I expect the only untagged response to STATUS will be "* STATUS ..."
	*/
	snprintf(line, sizeof(line),
		 "a%03d STATUS \"%s\" (MESSAGES UNSEEN)\r\n",
		 ++seq, account->imapfolder);
	server_command(&conn, mbox, line);
	if (! server_response(&conn, mbox, "*"))
		return tcp_shutdown(&conn, mbox, tcp_error_message[4], FALSE);

	if ((ss = strstr(mbox->tcp_in->str, "MESSAGES")) == NULL)
		{
		if (strrchr(mbox->tcp_in->str, '}'))
			{
			if (! server_response(&conn, mbox, ""))
				return tcp_shutdown(&conn, mbox, tcp_error_message[4], FALSE);
			if ((ss = strstr(mbox->tcp_in->str, "MESSAGES")) == NULL)
				return tcp_shutdown(&conn, mbox, tcp_error_message[4], FALSE);
			} 
		else
			return tcp_shutdown(&conn, mbox, tcp_error_message[4], FALSE);
		}
	if (sscanf(ss, "MESSAGES %d", &messages) == 1)
		{
		if ((ss = strstr(mbox->tcp_in->str, "UNSEEN")) != NULL)
			sscanf(ss, "UNSEEN %d", &unseen);
		if (_GK.debug_level & DEBUG_MAIL)
			g_print(_("Messages: %d\nUnseen: %d\n"), messages, unseen);
		}
	mbox->mail_count = messages;
	mbox->new_mail_count = unseen;
	snprintf(line, sizeof(line), "a%03d OK", seq);
	imap_completion_result(&conn, mbox, line);

	snprintf(line, sizeof(line), "a%03d LOGOUT\r\n", ++seq);
	server_command(&conn, mbox, line);
	snprintf(line, sizeof(line), "a%03d OK", seq);
	imap_completion_result(&conn, mbox, line);

	tcp_close(&conn);
	return TRUE;
	}

static gboolean
mh_sequences_new_count(Mailbox *mbox)
	{
	FILE	*f;
	gchar	buf[1024];
	gchar	*path, *tok;
	gint	n0, n1;

	path = g_strconcat(mbox->account->path, G_DIR_SEPARATOR_S,
				".mh_sequences", NULL);
	f = fopen(path, "r");
	g_free(path);
	if (!f)
		return FALSE;
	have_mh_sequences = TRUE;
	if (mh_seq_ignore)
		{
		fclose(f);
		return FALSE;
		}
	while (fgets(buf, sizeof(buf), f))
		{
		/* Look for unseen sequence like "unseen: 4 7-9 23"
		*/
		if (strncmp(buf, "unseen:", 7))
			continue;
		tok = strtok(buf, " \t\n");
		while ((tok = strtok(NULL, " \t\n")) != NULL)
			{
			if (sscanf(tok, "%d-%d", &n0, &n1) == 2)
				mbox->new_mail_count += n1 - n0 + 1;
			else
				mbox->new_mail_count++;
			}
		break;
		}
	fclose(f);
	return TRUE;
	}

  /* Sylpheed procmsg.h enums MSG_NEW as (1 << 0) and MSG_UNREAD as (1 << 1)
  |  And procmsg_write_flags() in Sylpheeds procmsg.c writes a mail record as
  |  a pair of ints with msgnum first followed by flags.
  */
#define SYLPHEED_MSG_NEW		1
#define SYLPHEED_MSG_UNREAD		2
#define SYLPHEED_MARK_VERSION	2

static gboolean
sylpheed_mark_new_count(Mailbox *mbox)
	{
	FILE	*f = NULL;
	gchar	*path;
	gint	msgnum, flags, ver, mark_files = 0;
	static const gchar *mark_file_names[] = {
	    ".sylpheed_mark", ".claws_mark", NULL
	};
	const gchar **fname;

	for (fname = mark_file_names; *fname; fname++) {
	    path = g_strconcat(mbox->account->path, G_DIR_SEPARATOR_S,
			       *fname, NULL);
	    f = fopen(path, "rb");
	    g_free(path);
	    if (f)
		break;
	}

	if (!f)
	    return FALSE;
	
	if (   fread(&ver, sizeof(ver), 1, f) == 1
		&& SYLPHEED_MARK_VERSION == ver
	   )
		{
		while (   fread(&msgnum, sizeof(msgnum), 1, f) == 1
			   && fread(&flags, sizeof(flags), 1, f) == 1
			  )
			{
			if (   (flags & SYLPHEED_MSG_NEW)
				|| ((flags & SYLPHEED_MSG_UNREAD) && unseen_is_new)
			   )
				mbox->new_mail_count += 1;
			++mark_files;
			}
		if (mark_files < mbox->mail_count)
			mbox->new_mail_count += mbox->mail_count - mark_files;
		}
	fclose(f);
	return TRUE;
	}

  /* Check a mh directory for mail. The way that messages are marked as new
  |  depends on the MUA being using.  Only .mh_sequences and .sylpheed_mark
  |  are currently checked, otherwise all mail found is considered new mail.
  */
static gboolean
check_mh_dir(Mailbox *mbox)
	{
	GDir	*dir;
	gchar	*name;

	mbox->mail_count = mbox->new_mail_count = 0;

	if ((dir = g_dir_open(mbox->account->path, 0, NULL)) == NULL)
		return FALSE;
	while ((name = (gchar *) g_dir_read_name(dir)) != NULL)
		{
		/* Files starting with a digit are messages. */
		if (isdigit((unsigned char)name[0]))
			mbox->mail_count++;
		}
	g_dir_close(dir);

	/* Some MH dir clients use .mh_sequences, others such as mutt or gnus
	|  do not.  For mixed cases, it's a user option to ignore .mh_sequences.
	|  Sylpheed uses .sylpheed_mark.
	*/
	if (   !mh_sequences_new_count(mbox)
		&& !sylpheed_mark_new_count(mbox)
	   )
		mbox->new_mail_count = mbox->mail_count;

	return TRUE;
	}


  /* A maildir has new, cur, and tmp subdirectories.  Any file in new
  |  or cur that does not begin with a '.' is a mail message.  It is
  |  suggested that messages begin with the output of time() (9 digits)
  |  but while mutt and qmail use this standard, procmail does not.
  |  maildir(5) says:
  |      It is a good idea for readers to skip all filenames in
  |      new and cur starting with a dot.  Other than this,
  |      readers should not attempt to parse filenames.
  |  So check_maildir() simply looks for files in new and cur.
  |  But if unseen_is_new flag is set, look for ":2,*S" file suffix where
  |  the 'S' indicates the mail is seen.
  |  See http://cr.yp.to/proto/maildir.html
  */
static gboolean
check_maildir(Mailbox *mbox)
	{
	gchar	path[256], *s;
	gchar	*name;
	GDir	*dir;

	mbox->new_mail_count = 0;
	snprintf(path, sizeof(path), "%s%cnew", mbox->account->path,
				G_DIR_SEPARATOR);
	if ((dir = g_dir_open(path, 0, NULL)) != NULL)
		{
		while ((name = (gchar *) g_dir_read_name(dir)) != NULL)
			mbox->new_mail_count++;
		g_dir_close(dir);
		}
	mbox->mail_count = mbox->new_mail_count;
	snprintf(path, sizeof(path), "%s%ccur", mbox->account->path,
				G_DIR_SEPARATOR);
	if ((dir = g_dir_open(path, 0, NULL)) != NULL)
		{
		while ((name = (gchar *) g_dir_read_name(dir)) != NULL)
			{
			mbox->mail_count++;
			if (   unseen_is_new
				&& (   (s = strchr(name, ':')) == NULL
					|| !strchr(s, 'S')
				   )
			   )
				mbox->new_mail_count++;
			}
		g_dir_close(dir);
		}

	if (_GK.debug_level & DEBUG_MAIL)
		g_print("check_maildir %s: total=%d old=%d new=%d\n",
				mbox->account->path, mbox->mail_count,
				mbox->old_mail_count, mbox->new_mail_count);
    return TRUE;
	}


  /* Count total mail and old mail in a mailbox.  Old mail can be read
  |  with a Status: R0, or can be accessed and not read with Status: O
  |  So, new mail will be the diff - note that unread mail is not
  |  necessarily new mail.  According to stat() man page:
  |  st_atime is changed by mknod(), utime(), read(), write(), truncate()
  |  st_mtime is changed by mknod(), utime(), write()
  |  But, new mail arriving (writing mailbox) sets st_mtime while reading
  |  the mailbox (mail program reading) sets st_atime.  So the test
  |  st_atime > st_mtime is testing if mbox has been read since last new mail.
  |  Mail readers may restore st_mtime after writting status.
  |  And Netscape mail does status with X-Mozilla-Status: xxxS
  |    where S is bitwise or of status flags:
  |    1: read  2: replied  4: marked  8: deleted
  |  
  |  Evolution uses status with X-Evolution: 00000000-xxxx where xxxx status is
  |  a bitfield in hexadecimal (see enum _CamelMessageFlags in evolution/camel
  |  source) and most importantly CAMEL_MESSAGE_SEEN = 1<<4.
  */
  /* test if buf is a status for standard mail, mozilla or evolution 
  */
static gboolean
is_status(gchar *buf)
	{
	if (buf[0] != 'S' && buf[0] != 'X')
		return FALSE;

	if ( !strncmp(buf, "Status:", 7)  /* Standard mail clients */
	     || !strncmp(buf, "X-Mozilla-Status:", 17) /* Netscape */
	     || !strncmp(buf, "X-Evolution:", 12) )     /* Mozilla */
	    return TRUE;
	else
	    return FALSE;
	}

static gboolean
status_is_old(gchar *buf)
	{
	gchar	c;
	int tmp;

	/* Standard mail clients
	*/
	if (   !strncmp(buf, "Status:", 7)
		&& (strchr(buf, 'R') || (!unseen_is_new && strchr(buf, 'O')))
	   )
		return TRUE;

	/* Netscape
	*/
	if (!strncmp(buf, "X-Mozilla-Status:", 17))
		{
		c = buf[21];
		if (c < '8')	/* Not deleted */
			c -= '0';
		if (c >= '8' || (c & 0x1))
			return TRUE;
		}

	/* Evolution
	*/
	if (!strncmp(buf, "X-Evolution:", 12))
		{
		sscanf(buf+22, "%04x", &tmp);
		if (tmp & (1<<4))
			return TRUE;
		}

	return FALSE;
	}

  /* test if a mail is marked as deleted  
  |  Evolution uses status with X-Evolution: 00000000-xxxx where xxxx status is
  |  a bitfield in hexadecimal (see enum _CamelMessageFlags in evolution/camel source)
  |  and most importantly CAMEL_MESSAGE_DELETED = 1<<1.
  */
static gboolean
status_is_deleted(gchar *buf)
	{
	gint	tmp;

	/* Standard mail clients
	if (   !strncmp(buf, "Status:", 7) )
	*/
	/* Netscape
	if (!strncmp(buf, "X-Mozilla-Status:", 17))
	*/
	/* Evolution
	*/
	if (!strncmp(buf, "X-Evolution:", 12))
		{
		sscanf(buf+22, "%04x", &tmp);
		if (tmp & (1<<1))
			return TRUE;
		/* Junk is not explicitly marked as deleted but is shown as if
		|  where in evolution
		*/
		if (tmp & (1<<7))
			return TRUE;
		}

	return FALSE;
	}

static gboolean
check_mbox(Mailbox *mbox)
	{
	FILE			*f;
	struct utimbuf	ut;
	struct stat		s;
	gchar			buf[1024];
	gchar			mpart_sep[1024];
	gint			in_header	= FALSE;
	gint			marked_read = FALSE;
	gint			is_multipart = FALSE;
	gint			content_len = 0;

	if (stat(mbox->account->path, &s) != 0)
		{
		mbox->mail_count = mbox->old_mail_count = mbox->new_mail_count = 0;
		mbox->last_mtime = 0;
		mbox->last_size = 0;
		if (_GK.debug_level & DEBUG_MAIL)
			printf("check_mbox can't stat(%s): %s\n", mbox->account->path,
					g_strerror(errno));
		return FALSE;
		}

	/* Message no-counting mode reports new mail based on mailbox
	|  modified time and size.
	*/
	if (count_mode == MSG_NO_COUNT)
		{
		if (   s.st_size > 0
			&& s.st_size >= mbox->last_size
			&& s.st_mtime >= s.st_atime
		   )
			mbox->new_mail_count = TRUE;
		else
			mbox->new_mail_count = FALSE;
		mbox->mail_count = (s.st_size > 0) ? 1 : 0;	/* boolean, not used */
		mbox->old_mail_count = 0;			/* not used */
		mbox->last_size = s.st_size;
		mbox->last_mtime = s.st_mtime;
		return TRUE;
		}

	/* If the mailboxes have been modified since last check, count
	|  the new/total messages.
	*/
	if (   s.st_mtime != mbox->last_mtime
		|| s.st_size  != mbox->last_size
		|| force_mail_check
	   )
		{
		if ((f = fopen(mbox->account->path, "r")) == NULL)
			{
			if (_GK.debug_level & DEBUG_MAIL)
				printf("check_mbox can't fopen(%s): %s\n", mbox->account->path,
						g_strerror(errno));
			return FALSE;
			}
		mbox->mail_count = 0;
		mbox->old_mail_count = 0;
		while(fgets(buf, sizeof(buf), f))
			{
			if (is_multipart && !in_header)
				{
				/* Skip to last line of multipart mail */
				if (strncmp(buf,mpart_sep,strlen(mpart_sep))==0)
					is_multipart = FALSE;
				}
			else if (buf[0] == '\n')
				{
				in_header = FALSE;
				mbox->is_internal = FALSE;
				if (content_len > 0) { /* Then see if we can jump to the next message in sequence */
					long this_pos, jump_to;
					this_pos = ftell(f);
					jump_to = content_len + this_pos + 1;
					if (fseek(f, jump_to, SEEK_SET) != 0 ||
					    fgets(buf, sizeof(buf), f) == NULL ||
					    strncmp("From ", buf, 5))
						{
						fseek(f, this_pos, SEEK_SET);
						}
					else /* We have an apparently valid From line */
						{
						if (is_From_line(mbox, buf))
							{
							mbox->mail_count += 1;
							in_header = TRUE;
							marked_read = FALSE;
							content_len = 0;
							is_multipart = FALSE;
							}
						else
							{
							fseek(f, this_pos, SEEK_SET);
							}
						}
					}
				}
			else if (is_From_line(mbox, buf))
				{
				mbox->mail_count += 1;
				in_header = TRUE;
				marked_read = FALSE;
				content_len = 0;
				}
			else if (in_header && !strncmp(buf, "Content-Length: ", 16))
				{
				content_len = atoi(buf + 16);
				}
			else if (in_header && is_status(buf))
				{
				if (status_is_old(buf) && !marked_read)
					{
					mbox->old_mail_count += 1;
					marked_read = TRUE;
				    }
				if (status_is_deleted(buf))
					{
					if (marked_read)
						mbox->old_mail_count -= 1;
					mbox->mail_count -= 1;
				    }
				}
			else if (in_header && mbox->is_internal)
				{
				if (strncmp(buf, "From: Mail System Internal Data", 31) == 0)
					{
					in_header = FALSE;
					mbox->mail_count -= 1;
					mbox->is_internal = FALSE;
					}
				}
			else if (in_header && is_multipart_mail(buf,mpart_sep))
				{
				is_multipart = TRUE;
				}
			}
		fclose(f);

		/* Restore the mbox stat times for other mail checking programs and
		|  so the (st_atime > st_mtime) animation override below will work.
		*/
		ut.actime = s.st_atime;
		ut.modtime = s.st_mtime;
		utime(mbox->account->path, &ut);

		mbox->last_mtime = s.st_mtime;
		mbox->last_size = s.st_size;
		if (_GK.debug_level & DEBUG_MAIL)
			g_print("check_mbox %s: total=%d old=%d\n",
					mbox->account->path,
					mbox->mail_count, mbox->old_mail_count);
		}
	/* Set the animation state when new mail count changes, and override
	|  the animation to false if mbox has been accessed since last modify
	|  (A MUA has probably read the mbox).
	*/
	mbox->new_mail_count = mbox->mail_count - mbox->old_mail_count;
	if (s.st_atime > s.st_mtime)
		{
		mbox->need_animation = FALSE;
		mbox->prev_new_mail_count = mbox->new_mail_count;
		}
    return TRUE;
	}

static GkrellmDecal	*mail_text_decal;
static GkrellmDecal	*mail_icon_decal;

static void
draw_mail_text_decal(gint new_mail_count, gint mail_count)
	{
	GkrellmTextstyle	ts, ts_save;
	gint				x, w;
	GkrellmPanel		*p;
	GkrellmDecal		*d;
	GkrellmStyle		*style;
	GkrellmMargin		*m;
	gchar				buf[32], nbuf[16], tbuf[16];

	p = mail;
	d = mail_text_decal;

	ts_save = d->text_style;
	if (new_mail_count == MUTE_FLAG)
		{
		snprintf(buf, sizeof(buf), _("mute"));
		ts = *gkrellm_meter_alt_textstyle(style_id);	/* Use the alt color */
		ts.font = d->text_style.font;
		}
	else
		{
		ts = d->text_style;
		if (count_mode == MSG_NO_COUNT)
			buf[0] = '\0';
		else if (count_mode == MSG_NEW_COUNT)
			{
			if (new_mail_count == 0)
				strcpy(buf, "-");
			else
				snprintf(buf, sizeof(buf), "%d", new_mail_count);
			}
		else /* MSG_NEW_TOTAL_COUNT */
			{
			if (new_mail_count == 0)
				strcpy(nbuf, "-");
			else
				snprintf(nbuf, sizeof(nbuf), "%d", new_mail_count);
			if (mail_count == 0)
				strcpy(tbuf, "-");
			else
				snprintf(tbuf, sizeof(tbuf), "%d", mail_count);
			snprintf(buf, sizeof(buf), "%s/%s", nbuf, tbuf); 
			}
		}
	w = gkrellm_gdk_string_width(ts.font, buf);
	if (w > d->w)
		{
		ts.font = gkrellm_meter_alt_textstyle(style_id)->font;
		w = gkrellm_gdk_string_width(ts.font, buf);
		}
	style = gkrellm_meter_style(style_id);
	m = gkrellm_get_style_margins(style);
	x = gkrellm_chart_width() * p->label->position / GKRELLM_LABEL_MAX;
	x -= m->right + w / 2;
	if (p->label->position >= 50)
		x -= mail_icon_decal->w;
	if (x > d->w - w)
		x = d->w - w;
	if (x < 0)
		x = 0;
	d->text_style = ts;
	d->x_off = x;
	gkrellm_draw_decal_text(p, d, buf, 0);
	d->text_style = ts_save;
	}

static void
mbox_set_animation_state(Mailbox *mbox)
	{
	if (mbox->new_mail_count != mbox->prev_new_mail_count)
		mbox->need_animation =
				(mbox->new_mail_count > mbox->prev_new_mail_count);
	}

static void
update_krell_animation_frame(void)
	{
	/* Run the animation.  Just go back and forth with a pause on
	|  frames 1 and full_scale - 1 (frames 0 and full_scale are cut off).
	|  Frame 0 is blank anyway, and frame full_scale is just not used.
	*/
	if (anim_pause-- <= 0)
		{
		if (anim_frame <= 1)
			anim_dir = 1;
		if (anim_frame >= KRELL(mail)->full_scale - 1)
			anim_dir = -1;
		anim_frame += anim_dir;
		anim_frame %= KRELL(mail)->full_scale;
		if (anim_frame == 1 || anim_frame == KRELL(mail)->full_scale - 1)
			anim_pause = 4;
		}
	}

  /* I popen the mail_user_agent so I can know if it is running and
  |  the mail_fetch so I can process fetchmail/flist/fetcho/...  output.
  |  Reading the pipes need to not block so GKrellM won't freeze.
  |  So, I need a special non-blocking pipe line reading routine.
  */
static void
pipe_command(Mailproc *mp)
	{
	gchar		**argv;
	GError		*err = NULL;
	gboolean	res;

	if (mp->pipe >= 0)	/* Still running?  */
		{
		if (_GK.debug_level & DEBUG_MAIL)
			g_print("mail pipe_command: <%s> still running.\n", mp->command);
		return;
		}
	if (!mp->command || *mp->command == '\0')
		return;

	g_shell_parse_argv(mp->command, NULL, &argv, NULL);

	if (_GK.debug_level & DEBUG_MAIL)
		g_print("mail pipe_command <%s>\n", mp->command);

	mp->pipe = -1;
	res = g_spawn_async_with_pipes(NULL, argv, NULL,
				G_SPAWN_SEARCH_PATH | G_SPAWN_STDERR_TO_DEV_NULL,
				NULL, NULL, NULL, NULL, &mp->pipe, NULL, &err);

	if (!res && err)
		{
		gkrellm_message_dialog(NULL, err->message);
		g_error_free(err);
		}

	if (mp->read_gstring)
		mp->read_gstring = g_string_truncate(mp->read_gstring, 0);
	else
		mp->read_gstring = g_string_new("");
#ifndef WIN32
	fcntl(mp->pipe, F_SETFL, O_NONBLOCK);
#endif
	g_strfreev(argv);
	}


  /* Accumulate non-blocking reads from a pipe into a gstring.  Then
  |  try to read a single line from the gstring.  If a line is read,
  |  return the count of chars in the line.  But if no line can be read
  |  and the other end of the pipe has exited (pipe eof), return -1.
  |  Otherwise return 0 to indicate no line is available but the pipe
  |  producer is still alive.
  |  Why this trouble?: the stdio fgets() reading from a non-blocking
  |  stream is not guaranteed to return complete '\n' delimited lines.
  */
static gint
fgets_pipe(gchar *line, gint len, Mailproc *mp)
	{
	gchar	buf[512];
	gint	n;

	if (mp->pipe >= 0)
		{
#if defined(WIN32)
		DWORD bytesAvailable  = 0;
		HANDLE hPipe = (HANDLE)_get_osfhandle(mp->pipe);
		// pipe is blocking on windows so check before ending up in a
		// blocking call to read()
		if (!PeekNamedPipe(hPipe, NULL, 0, NULL, &bytesAvailable, NULL))
			{
			DWORD err = GetLastError();
			// this is no real error if the started mua was closed in the meantime
			if (err == ERROR_BROKEN_PIPE)
				mp->pipe = -1;
			else if (_GK.debug_level & DEBUG_MAIL)
				g_print("fgets_pipe: PeekNamedPipe() FAILED, error was %lu\n", err);
			return 0;
			}
		if (bytesAvailable == 0)
			return 0;
		n = read(mp->pipe, buf, min(sizeof(buf) - 1, bytesAvailable));
#else
		n = read(mp->pipe, buf, sizeof(buf) - 1);
#endif
		if (n <= 0)
			{
			if (errno != EINTR && errno != EAGAIN)
				{
				if (close(mp->pipe) < 0 && errno == EINTR)
					close(mp->pipe);
				mp->pipe = -1;
				}
			}
		else
			{
			buf[n] = '\0';
			mp->read_gstring = g_string_append(mp->read_gstring, buf);
			}
		}
	if (gkrellm_getline_from_gstring(&mp->read_gstring, line, len))
		return strlen(line);
	if (mp->pipe < 0)
		{
		if (mp->read_gstring)
			g_string_free(mp->read_gstring, TRUE);
		mp->read_gstring = NULL;
		return -1;
		}
	return 0;
	}

static gboolean
mua_is_launched(void)
	{
	gchar	buf[128];
	gint	n;

	if (mail_user_agent.pipe == -1)
		return FALSE;
	while ((n = fgets_pipe(buf, sizeof(buf), &mail_user_agent)) > 0)
		;
	return (n < 0) ? FALSE : TRUE;
	}

  /* Read mail_fetch pipe and if fetch_check_only_mode, look for output
  |  of fetchmail -c or flist so I can report mail from these programs.
  |
  | eg. for MH mail, the nmh program flist -all (from the man page):
  |   /work/Mail  has  5 in sequence unseen (private); out of  46
  |   inbox+      has 10 in sequence unseen          ; out of 153
  |   junklist    has  0 in sequence unseen          ; out of  63
  |
  |  For fetchmail, if no remote mail, I get:
  |		fetchmail: No mail for billw at mail.wt.net
  |  If remote mail i will get lines like:
  |		1 message for billw at mail.wt.net (32743 octets).
  |	  or, as reported by a user, there could be:
  |		26 messages (25 seen) for billw at mail.wt.net
  |  If the remote mail is fetched, I get additional lines like:
  |		reading message 1 of 1 (32743 octets) .................... flushed
  |  Note: above 26 messages (25 seen) should show as 1/26 on panel.
  |
  |  And fetchmail can't make up its mind.  A user gets this with 5.2.0:
  |  fetchmail: 5.2.0 querying xx.yy.net (protocol POP3) at Thu, 20 Jan 2000...
  |  fetchmail: 2 messages for uuu at xx.yy.net (20509 octets).
  */

  /* Since there is now internal POP3 and IMAP checking, this will not be
  |  used as much, but there is still flist, using fetchmail for ssl, etc.
  |  Anyway, here's the code to grok the above mess!
  */
static gboolean
parse_fetch_line(gchar *line, gint *msg, gint *seen)
	{
	gchar	*s, *p, *buf;
	gint	n, n1, n2;
	gint	tok_count	= 0;
	gint	state		= 0,
			seen_flag	= FALSE,
			unseen_flag	= FALSE;

	*msg = 0;
	*seen = 0;
	n1 = n2 = 0;
	buf = g_strdup(line);
	s = strtok(buf, " \t()\n");

	/* Trap out badly set Fetch/check option.
	*/
	if (!s || !strcmp(s, "reading") || !strcmp(s, "skipping"))
		{
		g_free(buf);
		return FALSE;
		}

	if (_GK.debug_level & DEBUG_MAIL)
		printf("  parse[");
	while (s)
		{
		if (++tok_count > 3 && state == 0)	/* need a int within 3 tokens */
			break;
		n = strtol(s, &p, 0);
		if (*p == ' ' || *p == '\t' || *p == '\0' || *p == '\n')
			{		/* Have an integer, and not a x.y version number */
			if (_GK.debug_level & DEBUG_MAIL)
				printf("*<%s>,st=%d,", s, state);
			if (state == 0)
				n1 = n;
			else if (state == 1)
				n2 = n;
			if (_GK.debug_level & DEBUG_MAIL)
				printf("n1=%d,n2=%d state=%d*", n1, n2, state);
			++state;
			}
		else if (!strcmp(s, "seen") || !strcmp(s, _("seen")))
			seen_flag = TRUE;
		else if (!strcmp(s, "unseen"))
			unseen_flag = TRUE;
		s = strtok(NULL, " \t()\n");
		}
	if (state > 1 && seen_flag)	/* 26 messages (25 seen) ... */
		{
		*msg = n1;
		*seen = n2;
		}
	else if (state > 1 && unseen_flag)	/* /xxx has 5 in sequence unseen ... */
		{
		*msg = n2;
		*seen = n2 - n1;
		}
	else if (state > 0)			/*     1 message for billw at ... */
		*msg = n1;				/* or  Fetchmail: 1 message for .... */
	if (_GK.debug_level & DEBUG_MAIL)
		printf("]snf=%d sunf=%d msg=%d seen=%d STATE=%d\n",
				seen_flag, unseen_flag, *msg, *seen, state);
	g_free(buf);
	return TRUE;
	}


static gint
compare_mailboxes(gconstpointer a, gconstpointer b)
	{
	gchar a_name[128];
	gchar b_name[128];

	format_remote_mbox_name((Mailbox *)a, a_name, sizeof(a_name));
	format_remote_mbox_name((Mailbox *)b, b_name, sizeof(b_name));

	return strcmp(a_name, b_name);
	}

static void
make_fetch_tooltip(gchar *line, gint msg, gint seen)
	{
	Mailbox		*mbox;
	MailAccount	*account;
	GList		*old_mbox_pointer;
	gchar		buf[64], *s;

	mbox = g_new0(Mailbox, 1);
	account = g_new0(MailAccount, 1);
	mbox->account = account;
	account->mboxtype = MBOX_FETCH_TOOLTIP;

	if ((s = strstr(line, "sequence unseen")) != NULL)	/* flist */
		{
		sscanf(line, "%63s", buf);
		account->username = g_strdup(buf);
		}
	else if ((s = strstr(line, " for ")) != NULL)		/* fetchmail */
		{
		sscanf(s + 5, "%63s", buf);
		account->username = g_strdup(buf);
		if ((s = strstr(line, " at ")) != NULL)
			{
			sscanf(s + 4, "%63s", buf);
			account->server = g_strdup(buf);
			}
		if ((s = strstr(line, "(folder ")) != NULL)
			{
			sscanf(s + 8, "%63[^)]", buf);
			account->imapfolder = g_strdup(buf);
			}
		}
	else
		{
		free_mailbox(mbox);
		return;
		}
	old_mbox_pointer = g_list_find_custom(mailbox_list, mbox,
				(GCompareFunc) compare_mailboxes);
	if (old_mbox_pointer)
		{
		free_mailbox(mbox);
		mbox = (Mailbox *) old_mbox_pointer->data;
		if (mbox->account->mboxtype == MBOX_FETCH_TOOLTIP)
			{
			mbox->mail_count = msg;
			mbox->new_mail_count = msg - seen;
			}
		}
	else
		{
		mbox->mail_count = msg;
		mbox->new_mail_count = msg - seen;
		mailbox_list = g_list_insert_sorted(mailbox_list, mbox,
								(GCompareFunc)(compare_mailboxes));
		}
	}

  /* Read output lines from the fetch/check program.  If fetch_check_only_mode
  |  is set, parse the lines to try to read message counts so they can be
  |  reported ("fetchmail -c" or equiv should be configured).  However, if
  |  the mode is not set, then just read lines and waste them because mail
  |  is presumably being downloaded into local mailboxes.
  */
static void
read_mail_fetch(void)
	{
	Mailproc	*mp		= (Mailproc *) mail_fetch->private;
	gchar		buf[128];
	gint		n, msg, seen;

	while ((n = fgets_pipe(buf, sizeof(buf), mp)) > 0)	/* non-blocking */
		{
		if (_GK.debug_level & DEBUG_MAIL)
			printf("read_mail_fetch(%d): %s\n", fetch_check_only_mode, buf);
		if (fetch_check_only_mode)
			{
			if (parse_fetch_line(buf, &msg, &seen))
				make_fetch_tooltip(buf, msg, seen);
			if (msg > 0)
				{
				mail_fetch->mail_count += msg;
				mail_fetch->old_mail_count += seen;
				}
			}
		}

	/* When fetch program is done, flag not busy so new counts will be used.
	*/
	if (n < 0)
		{
		if (fetch_check_only_mode)
			{
			mail_fetch->new_mail_count =
					mail_fetch->mail_count - mail_fetch->old_mail_count;
			mbox_set_animation_state(mail_fetch);
			}
		mail_fetch->busy = FALSE;
		}
	}

static void
reset_mail_fetch(void)
	{
	Mailproc	*mp	= (Mailproc *) mail_fetch->private;

	if (mp->pipe >= 0)
		{
		if (close(mp->pipe) < 0 && errno == EINTR)
			close(mp->pipe);
		}
	mp->pipe = -1;
	if (mp->read_gstring)
		g_string_free(mp->read_gstring, TRUE);
	mp->read_gstring = NULL;
	mail_fetch->mail_count = 0;
	mail_fetch->new_mail_count = 0;
	mail_fetch->old_mail_count = 0;
	mail_fetch->need_animation = FALSE;
	mail_fetch->busy = FALSE;
	force_mail_check = TRUE;
	}

static gboolean
run_fetch_program(void)
	{
	Mailbox		*mbox;
	Mailproc	*mp	= (Mailproc *) mail_fetch->private;
	GList		*list;

	if (   !mail_fetch->busy && !mp->read_gstring
		&& mp->command && *(mp->command) != '\0'
	   )
		{
		mail_fetch->busy = TRUE;
		for (list = mailbox_list; list; list = list->next)
			{
			mbox = (Mailbox *) list->data;
			if (   mbox->account->mboxtype == MBOX_FETCH
				|| mbox->account->mboxtype == MBOX_FETCH_TOOLTIP
			   )
				{
				mbox->mail_count = 0;
				mbox->new_mail_count = 0;
				mbox->old_mail_count = 0;
				}
			}
		pipe_command(mp);
		if (mp->pipe >= 0)
			return TRUE;
		}
	return FALSE;
	}

static gpointer
mail_check_thread(void *data)
	{
	Mailbox	*mbox	= (Mailbox *) data;
	gchar	buf[256];
	gint	external;

	buf[0] = '\0';
	if (_GK.debug_level & DEBUG_MAIL)
		{
		format_remote_mbox_name(mbox, buf, sizeof(buf));
		printf("Start mail_check_thread: %s at %d:%d\n", buf,
				gkrellm_get_current_time()->tm_min,
				gkrellm_get_current_time()->tm_sec);
		}
	external = mbox->account->mboxtype & MBOX_EXTERNAL;
	if ( (*(mbox->check_func))(external ? mbox->data : mbox) == FALSE)
		{
		mbox->mail_count = 0;
		mbox->new_mail_count = 0;
		mbox->need_animation = FALSE;
		}
	else
		mbox_set_animation_state(mbox);

	if (_GK.debug_level & DEBUG_MAIL)
		printf("Stop mail_check_thread: %s at %d:%d\n", buf,
				gkrellm_get_current_time()->tm_min,
				gkrellm_get_current_time()->tm_sec);

	mbox->busy = FALSE;
	mbox->thread = NULL;
	return NULL;
	}

static void
update_mail(void)
	{
	Mailbox		*mbox;
	GList		*list;
	gint		external, prev_new_mail_count, any_busy;
	gint		local_check  = FALSE,
				remote_check = FALSE,
				fetch_check;
	static gint	second_count,
				minute_count,
				sound_inhibit;

	if (!enable_mail || !mailbox_list)
		return;
	if (GK.minute_tick && (++minute_count % remote_check_timeout) == 0)
		remote_check = TRUE;
	if (GK.second_tick && (++second_count % local_check_timeout) == 0)
		local_check = TRUE;
	fetch_check = fetch_check_is_local ? local_check : remote_check;
		
	if (remote_check || local_check)
		mua_is_launched();		/* update pipe, avoid lingering zombie */

	if (   force_mail_check
		|| (   GK.second_tick
			&& !(mute_mode && super_mute_mode)
			&& !(mua_inhibit_mode && mail_user_agent.pipe >= 0)
		   )
	   )
		{
		prev_new_mail_count = new_mail_count;
		total_mail_count = 0;
		new_mail_count = 0;
		run_animation = FALSE;
		remote_check |= force_mail_check;
		local_check |= force_mail_check;
		fetch_check |= force_mail_check;
		if (force_mail_check)
			minute_count = second_count = 0;
		any_busy = FALSE;

		for (list = mailbox_list; list; list = list->next)
			{
			mbox = (Mailbox *) list->data;
			external = mbox->account->mboxtype & MBOX_EXTERNAL;
			switch (mbox->account->mboxtype & MBOX_CHECK_TYPE_MASK)
				{
				case MBOX_CHECK_FETCH:
					if (((Mailproc *)(mail_fetch->private))->read_gstring)
						read_mail_fetch();
					if (fetch_check)
						(*mbox->check_func)(mbox);
					break;
					
				case MBOX_CHECK_INLINE:	/* Local mailbox or maildir check */
					if (local_check)
						{
						if (mbox->check_func)
							(*mbox->check_func)(external ? mbox->data : mbox);
						mbox_set_animation_state(mbox);
						}
					break;
				case MBOX_CHECK_THREADED:
					if (remote_check && !mbox->busy && mbox->check_func)
						{
						mbox->busy = TRUE;
						mbox->thread = g_thread_create(mail_check_thread,
								mbox, FALSE, NULL);
						}
					else if (   (_GK.debug_level & DEBUG_MAIL)
							 && remote_check && mbox->busy
							)
						printf("    %s thread busy\n", mbox->account->server);
					break;
				default:	/* Unknown or pseudo mail box type */
					continue;
				}
			/* If a mailbox check is busy (thread or remote fetch program
			|  running), use previous counts until the check is done.
			*/
			if (mbox->busy)
				{
				total_mail_count += mbox->prev_mail_count;
				new_mail_count += mbox->prev_new_mail_count;
				run_animation |= mbox->prev_need_animation;
				any_busy = TRUE;
				}
			else
				{
				total_mail_count += mbox->mail_count;
				new_mail_count += mbox->new_mail_count;
				if (mbox->new_mail_count && cont_animation_mode)
					mbox->need_animation = TRUE;
				run_animation |= mbox->need_animation;
				mbox->prev_mail_count = mbox->mail_count;
				mbox->prev_new_mail_count = mbox->new_mail_count;
				mbox->prev_need_animation = mbox->need_animation;
				if (mbox->warn_msg && !mbox->warned)
					{
					gkrellm_message_dialog(NULL, mbox->warn_msg);
					mbox->warned = TRUE;
					}
				}
			}
		force_mail_check = FALSE;

		if ((_GK.debug_level & DEBUG_MAIL) && (local_check || remote_check))
			g_print("Mail check totals: total=%d new=%d anim=%d [%d,%d,%d]\n",
				total_mail_count, new_mail_count, run_animation,
				local_check, remote_check, fetch_check);

		if (   prev_new_mail_count != new_mail_count
			|| tooltip->active_tips_data == NULL
		   )
			update_tooltip();

		/* Run the notify (sound) command if the new mail count goes up,
		|  and if the various modes permit.  Ensure a sound command is
		|  issued only once per remote check interval by locking it out
		|  if a new mail count triggers the sound and there are other
		|  remote threads still busy.
		*/
		if (   !sound_inhibit && !mute_mode
			&& new_mail_count > prev_new_mail_count
			&& mail_notify && mail_notify[0] != '\0'
		   )
			{
			g_spawn_command_line_async(mail_notify, NULL /* GError */);
			if (any_busy)
				sound_inhibit = TRUE;
			}
		if (!any_busy)
			sound_inhibit = FALSE;
		}

	if (mute_mode && (GK.timer_ticks % 15) < 3)	/* Asymmetric blink */
		draw_mail_text_decal(MUTE_FLAG, MUTE_FLAG);
	else
		draw_mail_text_decal(new_mail_count, total_mail_count);

	if (run_animation && (animation_mode & ANIMATION_PENGUIN))
		update_krell_animation_frame();
	else
		anim_frame = 0;

	if ((GK.timer_ticks % _GK.decal_mail_delay) == 0)
		{
		if (run_animation)
			{
			if (animation_mode & ANIMATION_ENVELOPE)
				{
				++decal_frame;
				if (decal_frame >= _GK.decal_mail_frames)
					decal_frame = 1;
				}
			else
				decal_frame = 0;
			}
		else
			decal_frame = 1;
		}
	gkrellm_draw_decal_pixmap(mail, DECAL(mail), decal_frame);

	/* All the animation frame drawing is done with the general krell code.
	*/
	KRELL(mail)->previous = 0;
	gkrellm_update_krell(mail, KRELL(mail), anim_frame);
	gkrellm_draw_panel_layers(mail);
	}


static gint
mail_expose_event(GtkWidget *widget, GdkEventExpose *ev)
	{
	if (widget == mail->drawing_area)
		gdk_draw_drawable(widget->window, gkrellm_draw_GC(1), mail->pixmap,
			ev->area.x, ev->area.y, ev->area.x, ev->area.y,
			ev->area.width, ev->area.height);
	return FALSE;
	}

  /* The mua launch button also optionally stops animations and resets
  |  remote mail counts.  So this routine decides if it should be enabled.
  */
static void
set_mua_button_sensitivity(void)
	{
	if (   *(mail_user_agent.command) || reset_remote_mode
		|| (animation_mode && !cont_animation_mode)
	   )
		gkrellm_set_button_sensitive(mua_button, TRUE);
	else
		gkrellm_set_button_sensitive(mua_button, FALSE);
	}


  /* Callback for the message count decal button.
  */
static void
cb_mail_button(GkrellmDecalbutton *button)
	{
	GList	*list;
	Mailbox	*mbox;

	if (reset_remote_mode)
		{
		for (list = mailbox_list; list; list = list->next)
			{
			mbox = (Mailbox *) list->data;
			if (   (mbox->account->mboxtype & MBOX_CHECK_THREADED)
				|| (mbox->account->mboxtype & MBOX_CHECK_FETCH)
				|| mbox->account->mboxtype == MBOX_FETCH_TOOLTIP
			   )
				{
				GDK_THREADS_ENTER();
				mbox->mail_count = 0;
				mbox->new_mail_count = 0;
				mbox->old_mail_count = 0;
				mbox->need_animation = FALSE;
				GDK_THREADS_LEAVE();
				}
			}
		}
	if (!cont_animation_mode)
		{
		for (list = mailbox_list; list; list = list->next)
			{
			mbox = (Mailbox *) list->data;
			mbox->need_animation = FALSE;
			}
		run_animation = FALSE;
		}
	if (enable_multimua)
		{
		if (mail_user_agent.command && *mail_user_agent.command != '\0')
			g_spawn_command_line_async(mail_user_agent.command, NULL);
		}
	else if (!mua_is_launched()) 
		pipe_command(&mail_user_agent);
	else
		{
		check_timeout = 0;
		if (_GK.debug_level & DEBUG_MAIL)
			g_print("Mail user agent is already running.\n");
		}
	}

  /* Callback for any button clicks on the Mail panel.  Must exclude
  |  button 1 clicks on the message decal button.
  */
static gint
cb_panel_press(GtkWidget *widget, GdkEventButton *ev)
	{
	GkrellmDecal	*d 	= mail_icon_decal;

	if (ev->button == 3)
		gkrellm_open_config_window(mon_mail);

	/* button 2 anywhere in the panel toggles the mute mode.
	*/
	if (ev->button == 2)
		{
		mute_mode = !mute_mode;

		/* If coming out of mute mode and mail checking was inhibited,
		|  force a check.
		*/
		if (   ! mute_mode && super_mute_mode
			&& (! mua_is_launched() || ! mua_inhibit_mode)
		   )
			force_mail_check = TRUE;
		}
	/* Button 1 press on the envelope - must exclude the message count button.
	*/
	if (ev->button == 1 && ev->x >= d->x && ev->x < d->x + d->w)
		force_mail_check = TRUE;
	return FALSE;
	}	

static void
dup_account(MailAccount *dst, MailAccount *src)
	{
	dst->path = g_strdup(src->path);
	dst->homedir_path = g_strdup(src->homedir_path);
	dst->server = g_strdup(src->server);
	dst->username = g_strdup(src->username);
	dst->password = g_strdup(src->password);
	dst->imapfolder = g_strdup(src->imapfolder);
	dst->mboxtype = src->mboxtype;
	dst->protocol = src->protocol;
	dst->authmech = src->authmech;
	dst->port = src->port;
	dst->use_ssl = src->use_ssl;
	}

static gboolean
get_local_mboxtype(MailAccount *account)
	{
	gchar	*path;

	if (!account->path)
		return FALSE;
	if (*(account->path) == '~')
		{
		account->homedir_path = account->path;
		account->path = g_strdup_printf("%s%s", gkrellm_homedir(),
						account->homedir_path + 1);
		}
	if (g_file_test(account->path, G_FILE_TEST_IS_DIR))
		{
		path = g_build_path(G_DIR_SEPARATOR_S, account->path, "new", NULL);
		if (g_file_test(path, G_FILE_TEST_IS_DIR))
			account->mboxtype = MBOX_MAILDIR;
		else
			account->mboxtype = MBOX_MH_DIR;
		g_free(path);
		}
	else
		account->mboxtype = MBOX_MBOX;
	return TRUE;
	}

static Mailbox *
add_mailbox(MailAccount *account)
	{
	Mailbox	*mbox;

	if (!account)
		return NULL;
	mbox = g_new0(Mailbox, 1);
	mbox->account = account;
	if (account->path)
		{
		if (*(account->path) == '~')
			{
			account->homedir_path = account->path;
			account->path = g_strdup_printf("%s%s", gkrellm_homedir(),
							account->homedir_path + 1);
			}
		if (account->mboxtype == MBOX_MAILDIR)
			mbox->check_func = check_maildir;
		else if (account->mboxtype == MBOX_MH_DIR)
			{
			mbox->check_func = check_mh_dir;
			checking_mh_mail = TRUE;
			}
		else
			mbox->check_func = check_mbox;
		}
	else if (account->mboxtype == MBOX_REMOTE)
		{
		switch (account->protocol)
			{
			case PROTO_POP3:
				mbox->check_func = check_pop3;
				break;
			case PROTO_IMAP:
				mbox->check_func = check_imap;
				break;
			default:
				g_free(mbox);
				mbox = NULL;
			}
		}
	else
		{
		g_free(mbox);
		mbox = NULL;
		}
	if (mbox)
		mailbox_list = g_list_append(mailbox_list, mbox);
	return mbox;
	}

  /* Pre 1.2.7  mailbox config parsing.
  */
static Mailbox *
old_add_mailbox(gchar *arg)
	{
	Mailbox		*mbox;
	MailAccount	*account;
	gchar		parms[4][CFG_BUFSIZE];
	gint		n;
	gchar		*p;

	account = g_new0(MailAccount, 1);
	n = sscanf(arg, "%s %s %s %s", parms[0], parms[1], parms[2], parms[3]);
	if (n == 1)
		{
		account->path = g_strdup(parms[0]);
		get_local_mboxtype(account);
		mbox = add_mailbox(account);
		return mbox;
		}
	mbox = g_new0(Mailbox, 1);
	mbox->account = account;
	switch (n)
		{
		case 3:
			account->mboxtype = MBOX_REMOTE;
			if ((p = strchr(parms[0], ':')) != NULL)
				*p++ = '\0';
			account->server = g_strdup(parms[0]);
			account->port = (p && *p) ? atoi(p) : atoi(DEFAULT_POP3_PORT);
			account->protocol = PROTO_POP3;
			if ((p = strchr(parms[1], '/')) != NULL)
				{
				*p++ = '\0';
				if (strcasecmp(p, "user") == 0)
					account->authmech = AUTH_USER;
				else if (strcasecmp(p, "apop") == 0)
					account->authmech = AUTH_APOP;
				else
					{
					g_free(account);
					g_free(mbox);
					return NULL;
					}
				}
			else
				account->authmech = AUTH_USER;
			account->username = g_strdup(parms[1]);
			account->password = g_strdup(parms[2]);
			account->use_ssl = SSL_NONE;
			mbox->check_func = check_pop3;
			break;

		case 4:
			account->mboxtype = MBOX_REMOTE;
			if ((p = strchr(parms[0], ':')) != NULL)
				*p++ = '\0';
			account->server = g_strdup(parms[0]);
			account->port = (p && *p) ? atoi(p) : atoi(DEFAULT_IMAP_PORT);
			account->imapfolder = g_strdup(parms[1]);
			account->username = g_strdup(parms[2]);
			account->password = g_strdup(parms[3]);
			account->protocol = PROTO_IMAP;
			account->authmech = AUTH_LOGIN;
			account->use_ssl = SSL_NONE;
			mbox->check_func = check_imap;
			break;

		default:	/* Invalid mailbox line */
			g_free(account);
			g_free(mbox);
			return NULL;
		}
	mailbox_list = g_list_append(mailbox_list, mbox);
	return mbox;
	}

static void
create_mail(GtkWidget *vbox, gint first_create)
	{
	GkrellmStyle	*style;
	GkrellmMargin	*m;
	GkrellmPanel	*p;
	MailAccount		*account;
	Mailproc		*mp = (Mailproc*) mail_fetch->private;
	gchar			*s, buf[64];
	gint			x, w;
	static GkrellmPiximage	*krell_penguin_piximage;


	if (first_create)
		mail = gkrellm_panel_new0();
	else
		{
		gkrellm_destroy_decal_list(mail);
		gkrellm_destroy_krell_list(mail);
		}
	p = mail;

	/* Force create user local mailbox if no mailchecking has been configured.
	*/
	if (!mailbox_list->next && local_supported && !*mp->command)
		{	/* First mailbox is internal fetch */
		if ((s = getenv("MAIL")) == NULL)
			if ((s = getenv("USER")) != NULL)
				{
				if (g_file_test("/var/mail", G_FILE_TEST_IS_DIR))
					snprintf(buf, sizeof(buf), "/var/mail/%s", s);
				else
					snprintf(buf, sizeof(buf), "/var/spool/mail/%s", s);
				s = buf;
				}
		if (s)
			{
			account = g_new0(MailAccount, 1);
			account->path = g_strdup(s);
			account->mboxtype = MBOX_MBOX;
			add_mailbox(account);
			}
		}

	style = gkrellm_meter_style(style_id);
	m = gkrellm_get_style_margins(style);

	if (krell_penguin_piximage)
		{
		gkrellm_destroy_piximage(krell_penguin_piximage);
		krell_penguin_piximage = NULL;
		}
	gkrellm_load_piximage("krell_penguin", NULL, &krell_penguin_piximage,
			MAIL_STYLE_NAME);
	gkrellm_create_krell(p,
			krell_penguin_piximage ? krell_penguin_piximage
								: gkrellm_krell_meter_piximage(style_id),
			style);
	KRELL(p)->full_scale = style->krell_depth - 1;

	gkrellm_load_piximage("decal_mail", decal_mail_xpm, &decal_mail_piximage,
			MAIL_STYLE_NAME);
	mail_icon_decal = gkrellm_make_scaled_decal_pixmap(p, decal_mail_piximage,
				style, _GK.decal_mail_frames, -1, -1, 0, 0);

	/* The new/total mail text needs to be a decal because the text changes
	|  and must be drawn as a layer in update_layers().
	|  Calculate x,w override values. Cannot override w after the create.
	*/
	x = m->left;
	if (style->label_position >= 50)
		x += mail_icon_decal->w;
	w = gkrellm_chart_width() - mail_icon_decal->w - m->left - m->right;

    mail_text_decal = gkrellm_create_decal_text(p, "0",
				gkrellm_meter_textstyle(style_id),
                style, x, -1, w);    /* -1 means use y default */

	gkrellm_panel_configure(p, NULL, style);
	gkrellm_panel_create(vbox, mon_mail, p);

	/* Center the decals with respect to each other.
	*/
	if (mail_icon_decal->h > mail_text_decal->h)
		mail_text_decal->y += (mail_icon_decal->h - mail_text_decal->h) / 2;
	else
		mail_icon_decal->y += (mail_text_decal->h - mail_icon_decal->h) / 2;

	mua_button = gkrellm_put_decal_in_meter_button(p, mail_text_decal,
				cb_mail_button, NULL, NULL);
	set_mua_button_sensitivity();

	if(!enable_mail)
		{
		gkrellm_panel_hide(p);
		gkrellm_spacers_hide(mon_mail);
		}
	else
		gkrellm_spacers_show(mon_mail);

	if (first_create)
		{
		g_signal_connect(G_OBJECT(p->drawing_area), "expose_event",
				G_CALLBACK(mail_expose_event), NULL);
		g_signal_connect(G_OBJECT(p->drawing_area),"button_press_event",
				G_CALLBACK(cb_panel_press), NULL);
		tooltip=gtk_tooltips_new();
		}
	}

static AuthType *
authtype_from_string(gchar *s)
	{
	gint	t;

	for (t = 0; auth_strings[t].string != NULL; ++t)
		if (!strcmp(s, auth_strings[t].string))
			break;
	return &auth_strings[t];
	}

static gint
menu_authtype(gint proto, gint mech)
	{
	gint	t;

	for (t = 0; auth_strings[t].string != NULL; ++t)
		if (auth_strings[t].protocol == proto &&
		    auth_strings[t].authmech == mech)
			break;
	return t;
	}

#define	menu_to_proto(type)	auth_strings[type].protocol
#define	menu_to_mech(type)	auth_strings[type].authmech
#define	auth_string(proto, mech) \
	auth_strings[menu_authtype(proto, mech)].string

static void
save_mail_config(FILE *f)
	{
	Mailbox		*mbox;
	MailAccount	*account;
	Mailproc	*mp		= (Mailproc*) mail_fetch->private;
	GList		*list;
	gchar		*pwd, *qq;

	for (list = mailbox_list; list; list = list->next)
		{
		mbox = (Mailbox *) list->data;
		account = mbox->account;
		switch (account->mboxtype)
			{
			case MBOX_MBOX:
			case MBOX_MAILDIR:
			case MBOX_MH_DIR:
				fprintf(f, "mail mailbox-local %s %s\n",
					mbox_strings[account->mboxtype & 0xf],
							account->homedir_path ?
							account->homedir_path : account->path);
				break;
			case MBOX_REMOTE:
				pwd = account->password;
				if ((qq = strchr(pwd, '"')) != NULL)
					pwd = "password";
				fprintf(f, "mail mailbox-remote %s %s \"%s\" \"%s\" %d",
					auth_string(account->protocol,
						    account->authmech),
					account->server, account->username,
					account->password, account->port);
				if (account->protocol == PROTO_IMAP)
					fprintf(f, " \"%s\"",
						account->imapfolder);
				fprintf(f, "\n");
				if (qq)
					fprintf(f, "mail password %s\n", account->password);
#ifdef HAVE_SSL
				fprintf(f, "mail mailbox-remote-use-ssl %d\n",
							account->use_ssl);
#endif
				break;
			default:
				break;
			}
		}
	fprintf(f, "mail mua %s\n", mail_user_agent.command);
	fprintf(f, "mail notify %s\n", mail_notify);
	fprintf(f, "mail fetch_command %s\n", mp->command);
	fprintf(f, "mail remote_check_timeout %d\n", remote_check_timeout);
	fprintf(f, "mail local_check_timeout %d\n", local_check_timeout);
	fprintf(f, "mail fetch_check_is_local %d\n", fetch_check_is_local);
	fprintf(f, "mail msg_count_mode %d\n", count_mode);
	fprintf(f, "mail animation_select_mode %d\n", animation_mode);
	fprintf(f, "mail fetch_check_only_mode %d\n", fetch_check_only_mode);
	fprintf(f, "mail reset_remote_mode %d\n", reset_remote_mode);
	fprintf(f, "mail unseen_is_new %d\n", unseen_is_new);
	fprintf(f, "mail enable %d %d %d %d\n", enable_mail, super_mute_mode,
						mua_inhibit_mode, enable_multimua);
	fprintf(f, "mail animation_continuous %d\n", cont_animation_mode);
	fprintf(f, "mail show_tooltip %d\n", show_tooltip);
	fprintf(f, "mail mh_seq_ignore %d\n", mh_seq_ignore);
	}

static void
load_mail_config(gchar *arg)
	{
	static MailAccount	*account_prev;
	MailAccount			*account = NULL;
	Mailproc			*mp		= (Mailproc*) mail_fetch->private;
	gchar				mail_config[32], item[CFG_BUFSIZE], path[256];
	gchar				*str, *s;
	gint				n;
	AuthType			*authtype;

	n = sscanf(arg, "%31s %[^\n]", mail_config, item);
	if (n == 2)
		{
		if (   (_GK.debug_level & DEBUG_MAIL)
			&& strcmp(mail_config, "password")
			&& strcmp(mail_config, "mailbox-remote")	/* avoid password */
		   )
			printf("%s %s\n", mail_config, item);
		if (!strcmp(mail_config, "mailbox"))	/* Old config, pre 1.2.7 */
			old_add_mailbox(item);
		else if (!strcmp(mail_config, "mailbox-local"))
			{
			if (local_supported && sscanf(item, "%*s %255[^\n]", path) == 1)
				{
				account = g_new0(MailAccount, 1);
				account->path = g_strdup(path);
				get_local_mboxtype(account);
				add_mailbox(account);
				}
			}
		else if (!strcmp(mail_config, "mailbox-remote"))
			{
			account = g_new0(MailAccount, 1);
			account_prev = NULL;
			account->mboxtype = MBOX_REMOTE;
			str = item;
			s = gkrellm_dup_token(&str, NULL);
			authtype = authtype_from_string(s);
			account->protocol = authtype->protocol;
			account->authmech = authtype->authmech;
			g_free(s);
			account->server = gkrellm_dup_token(&str, NULL);
			account->username = gkrellm_dup_token(&str, NULL);
			account->password = gkrellm_dup_token(&str, NULL);
			s = gkrellm_dup_token(&str, NULL);
			account->port = atoi(s);
			g_free(s);
			account->imapfolder = gkrellm_dup_token(&str, NULL);;
			if (add_mailbox(account))
				account_prev = account;		/* XXX */
			else
				{
				free_account(account);
				account_prev = NULL;
				}
			}
#ifdef HAVE_SSL
		else if (account_prev &&
			 !strcmp(mail_config, "mailbox-remote-use-ssl"))
			sscanf(item, "%d", &account_prev->use_ssl);
#endif
		else if (account_prev && !strcmp(mail_config, "password"))
			gkrellm_dup_string(&account_prev->password, item);
		else if (strcmp(mail_config, "mua") == 0)
			mail_user_agent.command = g_strdup(item);
		else if (strcmp(mail_config, "notify") == 0)
			mail_notify = g_strdup(item);
		else if (strcmp(mail_config, "fetch_command") == 0)
			mp->command = g_strdup(item);
		else if (strcmp(mail_config, "remote_check_timeout") == 0)
			sscanf(item, "%d", &remote_check_timeout);
		else if (strcmp(mail_config, "fetch_timeout") == 0)	/* pre 2.2.5 */
			sscanf(item, "%d", &remote_check_timeout);
		else if (strcmp(mail_config, "local_check_timeout") == 0)
			sscanf(item, "%d", &local_check_timeout);
		else if (strcmp(mail_config, "check_timeout") == 0)	/* pre 2.2.5 */
			sscanf(item, "%d", &local_check_timeout);
		else if (strcmp(mail_config, "fetch_check_is_local") == 0)
			sscanf(item, "%d", &fetch_check_is_local);
		else if (strcmp(mail_config, "msg_count_mode") == 0)
			sscanf(item, "%d", &count_mode);
		else if (strcmp(mail_config, "animation_select_mode") == 0)
			sscanf(item, "%d", &animation_mode);
		else if (strcmp(mail_config, "fetch_check_only_mode") == 0)
			sscanf(item, "%d", &fetch_check_only_mode);
		else if (strcmp(mail_config, "reset_remote_mode") == 0)
			sscanf(item, "%d", &reset_remote_mode);
		else if (strcmp(mail_config, "unseen_is_new") == 0)
			sscanf(item, "%d", &unseen_is_new);
		else if (strcmp(mail_config, "enable") == 0)
			sscanf(item, "%d %d %d %d", &enable_mail, &super_mute_mode,
						&mua_inhibit_mode, &enable_multimua);
		else if (strcmp(mail_config, "animation_continuous") == 0)
			sscanf(item, "%d", &cont_animation_mode);
		else if (strcmp(mail_config, "show_tooltip") == 0)
			sscanf(item, "%d", &show_tooltip);
		else if (strcmp(mail_config, "mh_seq_ignore") == 0)
			sscanf(item, "%d", &mh_seq_ignore);
		}
	}


/* ---------------------------------------------------------------------*/

enum
	{
	PROTOCOL_COLUMN,
#ifdef HAVE_SSL
	SSL_COLUMN,
#endif
	MAILBOX_COLUMN,
	ACCOUNT_COLUMN,
	N_COLUMNS
	};

static GtkTreeView		*treeview;
static GtkTreeRowReference *row_reference;
static GtkTreeSelection	*selection;

static GtkWidget		*account_notebook;
static GtkWidget		*mbox_path_entry;
static GtkWidget		*server_entry,
						*user_entry,
						*password_entry,
						*imapfolder_entry,
						*port_entry,
#ifdef HAVE_SSL
						*ssl_option_menu,
#endif
						*port_button;

static GtkWidget		*local_button,
						*remote_button,
						*delete_button,
						*new_apply_button,
						*remote_option_menu;

static GtkWidget		*mail_user_agent_entry;
static GtkWidget		*enable_multimua_button;
static GtkWidget		*mail_fetch_entry;
static GtkWidget		*mail_notify_entry;
static GtkWidget		*enable_cont_anim_button;
static GtkWidget		*super_mute_button;
static GtkWidget		*count_mode_button[3];
static GtkWidget		*anim_button[4];
static GtkWidget		*check_only_button;
static GtkWidget		*reset_remote_button;
static GtkWidget		*unseen_is_new_button;
static GtkWidget		*mua_inhibit_button;
static GtkWidget		*show_tooltip_button;
static GtkWidget		*mh_seq_ignore_button;

static GList			*config_mailbox_list;

static gint			optmenu_auth_protocol;
static gint			optmenu_use_ssl;	/* Always SSL_NONE if !HAVE_SSL */

static gboolean			selection_in_progress;

static gchar *
x_out(gchar *s)
	{
	gchar			*p;
	static gchar	xbuf[32];

	strncpy(xbuf, s, sizeof(xbuf));
	xbuf[31] = '\0';
	if ((p = strchr(xbuf, '_')) != NULL)
		*p = ' ';
	return xbuf;
	}

static gchar *
default_port_of_proto(gint protocol, gint use_ssl)
	{
	gchar *port = NULL;

	switch (protocol)
		{
		case PROTO_POP3:
			if (use_ssl == SSL_TRANSPORT)
				port = DEFAULT_POP3S_PORT;
			else
				port = DEFAULT_POP3_PORT;
			break;
		case PROTO_IMAP:
			if (use_ssl == SSL_TRANSPORT)
				port = DEFAULT_IMAPS_PORT;
			else
				port = DEFAULT_IMAP_PORT;
			break;
		}
	return port;
	}

static void
set_list_store_model_data(GtkListStore *store, GtkTreeIter *iter,
			MailAccount *account)
	{
	gchar	*protocol, *mailbox, *default_port = NULL, abuf[32], pbuf[32];
	gchar	*s;
#ifdef HAVE_SSL
	gchar	*use_ssl;
#endif

	if (account->mboxtype == MBOX_REMOTE)
		{
		default_port = default_port_of_proto(account->protocol,
						     account->use_ssl);
		sprintf(abuf, "%s",
			auth_string(account->protocol, account->authmech));
		mailbox = g_strdup(account->server);
		if (account->port != atoi(default_port))
			{
			sprintf(pbuf, ":%d", account->port);
			s = g_strconcat(mailbox, pbuf, NULL);
			g_free(mailbox);
			mailbox = s;
			}
		s = g_strconcat(mailbox, "  ", account->username, NULL);
		g_free(mailbox);
		mailbox = s;
		if (account->protocol == PROTO_IMAP)
			{
			s = g_strconcat(mailbox, "  ", account->imapfolder,
					NULL);
			g_free(mailbox);
			mailbox = s;
			}
		}
	else
		{
		sprintf(abuf, "%s", mbox_strings[account->mboxtype & 0xf]);
		mailbox = g_strdup_printf("%s", account->homedir_path ?
					account->homedir_path : account->path);
		}
	protocol = x_out(abuf);
#ifdef HAVE_SSL
	switch (account->use_ssl)
		{
		case SSL_TRANSPORT:
			use_ssl = "SSL";
			break;
		case SSL_STARTTLS:
			use_ssl = "STARTTLS";
			break;
		default:
			use_ssl = "";
			break;
		}
#endif

	gtk_list_store_set(store, iter,
			PROTOCOL_COLUMN, protocol,
#ifdef HAVE_SSL
			SSL_COLUMN, use_ssl,
#endif
			MAILBOX_COLUMN, mailbox,
			ACCOUNT_COLUMN, account,
			-1);
	g_free(mailbox);
	}

static GtkTreeModel *
create_model(void)
	{
	GtkListStore	*store;
	GtkTreeIter		iter;
	GList			*list;
	MailAccount		*account;

	store = gtk_list_store_new(N_COLUMNS,
					G_TYPE_STRING,
					G_TYPE_STRING,
#ifdef HAVE_SSL
					G_TYPE_STRING,
#endif
					G_TYPE_POINTER);

	for (list = config_mailbox_list; list; list = list->next)
		{
		account = (MailAccount *) list->data;
		gtk_list_store_append(store, &iter);
		set_list_store_model_data(store, &iter, account);
		}
	return GTK_TREE_MODEL(store);
	}

static void
change_row_reference(GtkTreeModel *model, GtkTreePath *path)
	{
	gtk_tree_row_reference_free(row_reference);
	if (model && path)
		row_reference = gtk_tree_row_reference_new(model, path);
	else
		row_reference = NULL;
	}

static void
reset_entries(void)
	{
	if (selection_in_progress)
		return;
	if (mbox_path_entry)
		gtk_entry_set_text(GTK_ENTRY(mbox_path_entry), "");

	gtk_entry_set_text(GTK_ENTRY(server_entry), "");
	gtk_entry_set_text(GTK_ENTRY(user_entry), "");
	gtk_entry_set_text(GTK_ENTRY(password_entry), "");
	gtk_entry_set_text(GTK_ENTRY(imapfolder_entry), "");
	gtk_widget_set_sensitive(imapfolder_entry, FALSE);
	gtk_option_menu_set_history(GTK_OPTION_MENU(remote_option_menu), 0);
	optmenu_auth_protocol = 0;
#ifdef HAVE_SSL
	gtk_option_menu_set_history(GTK_OPTION_MENU(ssl_option_menu), 0);
#endif
	optmenu_use_ssl = SSL_NONE;

	gtk_entry_set_text(GTK_ENTRY(port_entry), DEFAULT_POP3_PORT);
	gtk_widget_set_sensitive(port_entry, FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(port_button), FALSE);
	change_row_reference(NULL, NULL);
	gtk_tree_selection_unselect_all(selection);
	}

static void
cb_mailbox_group(GtkWidget *widget, gpointer data)
	{
	gint	group;

	group = GPOINTER_TO_INT(data);
	gtk_notebook_set_current_page(GTK_NOTEBOOK(account_notebook), group);
	reset_entries();
	}

static gboolean
default_port_entry(void)
	{
	gboolean	active;
	gchar*		default_port = NULL;

	active = GTK_TOGGLE_BUTTON(port_button)->active;
	if (!active)
		{
		default_port = default_port_of_proto(
			menu_to_proto(optmenu_auth_protocol),
			optmenu_use_ssl);
		gtk_entry_set_text(GTK_ENTRY(port_entry), default_port);
		}
	return active;
	}

#ifdef HAVE_SSL
static void
cb_ssl_selected(GtkMenuItem *menuitem)
	{
	optmenu_use_ssl =
			GPOINTER_TO_INT(g_object_get_data(G_OBJECT(menuitem),
							  "user_data"));
	default_port_entry();
	}
#endif

static void
cb_specify_port(GtkWidget *widget, gpointer data)
	{
	gtk_widget_set_sensitive(port_entry, default_port_entry());
	}

static void
cb_protocol_selected(GtkMenuItem *menuitem)
	{
	optmenu_auth_protocol =
			GPOINTER_TO_INT(g_object_get_data(G_OBJECT(menuitem),"user_data"));
	switch (menu_to_proto(optmenu_auth_protocol))
		{
		case PROTO_POP3:
			gtk_entry_set_text(GTK_ENTRY(imapfolder_entry), "");
			gtk_widget_set_sensitive(imapfolder_entry, FALSE);
			break;
		case PROTO_IMAP:
			gtk_entry_set_text(GTK_ENTRY(imapfolder_entry),
					   "inbox");
			gtk_widget_set_sensitive(imapfolder_entry, TRUE);
			break;
		}
	default_port_entry();
	}

static void
cb_tree_selection_changed(GtkTreeSelection *selection, gpointer data)
	{
	GtkTreeIter		iter;
	GtkTreeModel	*model;
	GtkTreePath		*path;
	MailAccount		*account;
	gchar			buf[32];
	gint			default_port = 0;
	gboolean		active;

	if (!gtk_tree_selection_get_selected(selection, &model, &iter))
		{
		reset_entries();
		gtk_button_set_label(GTK_BUTTON(new_apply_button), GTK_STOCK_NEW);
		gtk_widget_set_sensitive(delete_button, FALSE);
		return;
		}
	path = gtk_tree_model_get_path(model, &iter);
	change_row_reference(model, path);
	gtk_tree_path_free(path);

	gtk_button_set_label(GTK_BUTTON(new_apply_button), GTK_STOCK_APPLY);
	gtk_widget_set_sensitive(delete_button, TRUE);

	gtk_tree_model_get(model, &iter, ACCOUNT_COLUMN, &account, -1);

	/* Below toggle of group button causes a callback -> reset_entries(),
	|  and I want to lock that out.
	*/
	selection_in_progress = TRUE;
	if (account->mboxtype == MBOX_REMOTE)
		{
		if (remote_button)
			gtk_toggle_button_set_active(
						GTK_TOGGLE_BUTTON(remote_button), TRUE);
		gtk_entry_set_text(GTK_ENTRY(server_entry), account->server);
		gtk_entry_set_text(GTK_ENTRY(user_entry), account->username);
		gtk_entry_set_text(GTK_ENTRY(password_entry), account->password);
		optmenu_auth_protocol = menu_authtype(account->protocol,
						      account->authmech);
		optmenu_use_ssl = account->use_ssl;
		switch (account->protocol)
			{
			case PROTO_POP3:
				gtk_entry_set_text(GTK_ENTRY(imapfolder_entry), "");
				gtk_widget_set_sensitive(imapfolder_entry, FALSE);
				break;
			case PROTO_IMAP:
				gtk_entry_set_text(GTK_ENTRY(imapfolder_entry),
						   account->imapfolder);
				gtk_widget_set_sensitive(imapfolder_entry, TRUE);
				break;
			}
		default_port = atoi(default_port_of_proto(account->protocol,
							  account->use_ssl));
		gtk_option_menu_set_history(GTK_OPTION_MENU(remote_option_menu),
					optmenu_auth_protocol);
#ifdef HAVE_SSL
		gtk_option_menu_set_history(GTK_OPTION_MENU(ssl_option_menu),
					    optmenu_use_ssl);
#endif
		if (account->port < 1)
			account->port = default_port;
		active = (account->port == default_port) ? FALSE : TRUE;
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(port_button), active);
		sprintf(buf, "%d", account->port);
		gtk_entry_set_text(GTK_ENTRY(port_entry), buf);		
		}
	else if (local_supported)
		{
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(local_button), TRUE);
		gtk_entry_set_text(GTK_ENTRY(mbox_path_entry), account->homedir_path ?
					account->homedir_path : account->path);
		}
	selection_in_progress = FALSE;
	}


static gboolean
dup_entry(gchar **buf, GtkWidget **entry)
	{
	gchar	*s;

	s = gkrellm_gtk_entry_get_text(entry);
	if (*s)
		{
		*buf = g_strdup(s);
		return TRUE;
		}
	*buf = NULL;
	return FALSE;
	}

static void
sync_mail_list(void)
	{
	Mailbox			*mbox;
	MailAccount		*account, *new_account;
	GList			*list;

	/* Destroy MBOX_INTERNAL type mailboxes from the mailbox_list, then
	|  recreate them from the config list.  Skip over the first FETCH mbox.
	*/
	for (list = mailbox_list->next; list;  )
		{
		mbox = (Mailbox *) list->data;
		if (mbox->account->mboxtype & MBOX_INTERNAL)
			{
			list = g_list_delete_link(list, list);
			free_mailbox(mbox);
			}
		else
			list = list->next;
		}
	checking_mh_mail = FALSE;
	for (list = config_mailbox_list; list; list = list->next)
		{
		account = (MailAccount *) list->data;
		new_account = g_new0(MailAccount, 1);
		dup_account(new_account, account);
		add_mailbox(new_account);
		}
	force_mail_check = TRUE;
	}

static void
mailbox_enter_cb(void)
	{
	GtkTreeModel	*model;
	GtkTreePath		*path;
	GtkTreeIter		iter;
	GList			*list;
	MailAccount		*new_account, *account;
	gboolean		remote, valid;
	gint			default_port = 0;

	new_account = g_new0(MailAccount, 1);
	valid = FALSE;
	remote = remote_button ? GTK_TOGGLE_BUTTON(remote_button)->active : TRUE;
	if (remote)
		{
		if (   dup_entry(&new_account->server, &server_entry)
			&& dup_entry(&new_account->username, &user_entry)
			&& dup_entry(&new_account->password, &password_entry)
		   )
			valid = TRUE;
		if (GTK_TOGGLE_BUTTON(port_button)->active)
			new_account->port = atoi(gkrellm_gtk_entry_get_text(&port_entry));
		new_account->mboxtype = MBOX_REMOTE;
		new_account->protocol = menu_to_proto(optmenu_auth_protocol);
		new_account->authmech = menu_to_mech(optmenu_auth_protocol);
		new_account->use_ssl = optmenu_use_ssl;
		if (new_account->protocol == PROTO_IMAP && valid)
			valid = dup_entry(&new_account->imapfolder,
					  &imapfolder_entry);
		default_port = atoi(default_port_of_proto(
					    new_account->protocol,
					    new_account->use_ssl));
		if (new_account->port == 0)
			new_account->port = default_port;
		}
	else if (mbox_path_entry)
		{
		valid = dup_entry(&new_account->path, &mbox_path_entry);
		get_local_mboxtype(new_account);
		}

	if (!valid)
		{
		gkrellm_config_message_dialog(_("GKrellM Config Error"),
			_("Incomplete mailbox entries"));
		free_account(new_account);
		return;
		}

	model = gtk_tree_view_get_model(treeview);
	if (row_reference)
		{
		path = gtk_tree_row_reference_get_path(row_reference);
		gtk_tree_model_get_iter(model, &iter, path);
		gtk_tree_model_get(model, &iter, ACCOUNT_COLUMN, &account, -1);
		list = g_list_find(config_mailbox_list, account);
		free_account(account);
		if (list)
			list->data = (gpointer) new_account;
		}
	else
		{
		config_mailbox_list = g_list_append(config_mailbox_list, new_account);
		gtk_list_store_append(GTK_LIST_STORE(model), &iter);
		path = gtk_tree_model_get_path(model, &iter);
		gtk_tree_view_scroll_to_cell(treeview, path, NULL, TRUE, 0.5, 0.5);
		}
	set_list_store_model_data(GTK_LIST_STORE(model), &iter, new_account);

	reset_entries();
	sync_mail_list();
	}


static void
mailbox_delete_cb(GtkWidget *widget, gpointer *data)
	{
	GtkTreeModel	*model;
	GtkTreePath		*path;
	GtkTreeIter		iter;
	MailAccount		*account;

	if (!row_reference)
		return;
	model = gtk_tree_view_get_model(treeview);
	path = gtk_tree_row_reference_get_path(row_reference);
	gtk_tree_model_get_iter(model, &iter, path);

	gtk_tree_model_get(model, &iter, ACCOUNT_COLUMN, &account, -1);
	config_mailbox_list = g_list_remove(config_mailbox_list, account);
	free_account(account);

	gtk_list_store_remove(GTK_LIST_STORE(model), &iter);

	reset_entries();
	sync_mail_list();
	}

static void
cb_animation_mode(GtkWidget *button, gpointer data)
	{
	gint	i = GPOINTER_TO_INT(data);

	if (GTK_TOGGLE_BUTTON(button)->active)
		animation_mode = i;
	}

static void
cb_count_mode(GtkWidget *button, gpointer data)
	{
	gint	i = GPOINTER_TO_INT(data);

	if (GTK_TOGGLE_BUTTON(button)->active)
		count_mode = i;
	mail_text_decal->value = -1;		/* Force a redraw */
	force_mail_check = TRUE;
	}

static void
toggle_button_cb(GtkToggleButton *button, gint *flag)
	{
//	*flag = gtk_toggle_button_get_active(button);
	*flag = button->active;
	}

static void
multi_toggle_button_cb(GtkWidget *button, gpointer data)
	{
	gint	i;

	cont_animation_mode = GTK_TOGGLE_BUTTON(enable_cont_anim_button)->active;
	if (check_only_button)
		fetch_check_only_mode = GTK_TOGGLE_BUTTON(check_only_button)->active;
	reset_remote_mode = GTK_TOGGLE_BUTTON(reset_remote_button)->active;

	i = GTK_TOGGLE_BUTTON(unseen_is_new_button)->active;
	if (unseen_is_new != i)
		force_mail_check = TRUE;
	unseen_is_new = i;

	super_mute_mode = GTK_TOGGLE_BUTTON(super_mute_button)->active;

	mail_text_decal->value = -1;		/* Force a redraw */
	mua_inhibit_mode = GTK_TOGGLE_BUTTON(mua_inhibit_button)->active;

	enable_multimua = GTK_TOGGLE_BUTTON(enable_multimua_button)->active;
	show_tooltip = GTK_TOGGLE_BUTTON(show_tooltip_button)->active;

	if (mh_seq_ignore_button)
		mh_seq_ignore = GTK_TOGGLE_BUTTON(mh_seq_ignore_button)->active;
	update_tooltip();
	}

static void
cb_enable(GtkWidget *button, gpointer data)
	{
	enable_mail = GTK_TOGGLE_BUTTON(button)->active;
	if (enable_mail)
		{
		gkrellm_panel_show(mail);
		gkrellm_spacers_show(mon_mail);
		}
	else
		{
		gkrellm_panel_hide(mail);
		gkrellm_spacers_hide(mon_mail);
		}
	}

static void
cb_mua(GtkWidget *widget, gpointer data)
	{
	gkrellm_dup_string(&mail_user_agent.command,
				gkrellm_gtk_entry_get_text(&mail_user_agent_entry));
	set_mua_button_sensitivity();
	}

static void
remote_check_timeout_cb(GtkWidget *widget, GtkSpinButton *spin)
	{
	remote_check_timeout = gtk_spin_button_get_value_as_int(spin);
	}

static void
local_check_timeout_cb(GtkWidget *widget, GtkSpinButton *spin)
	{
	local_check_timeout = gtk_spin_button_get_value_as_int(spin);
	}


  /* Go to some extra trouble here to avoid incomplete entries being used for
  |  fetch or notify commands which can trigger in the middle of entering the
  |  command.  My UI instant apply behavior is that whatever is typed into the
  |  entry will be used and to be consistent with other entries I don't wan't
  |  to require an "enter" to "activate".  So, save "changed" values and apply
  |  them when config is closed if user never hit the Enter key.   (Is there
  |  a way for Gtk to auto activate?)
  */
static gchar	*pending_fetch,
				*pending_notify;

static void
cb_mail_fetch(GtkWidget *widget, gpointer data)
	{
	Mailproc	*mp          = (Mailproc *) mail_fetch->private;
	gboolean	activate_sig = GPOINTER_TO_INT(data);
	gchar		*s           = gkrellm_gtk_entry_get_text(&mail_fetch_entry);

	if (activate_sig)
		{
		if (gkrellm_dup_string(&mp->command, s))
			reset_mail_fetch();
		g_free(pending_fetch);
		pending_fetch = NULL;
		}
	else	/* "changed" sig, entry is pending on "activate" or config close */
		gkrellm_dup_string(&pending_fetch, s);
	}

static void
cb_mail_notify(GtkWidget *widget, gpointer data)
	{
	gboolean	activate_sig = GPOINTER_TO_INT(data);
	gchar		*s           = gkrellm_gtk_entry_get_text(&mail_notify_entry);

	if (activate_sig)
		{
		gkrellm_dup_string(&mail_notify, s);
		g_free(pending_notify);
		pending_notify = NULL;
		}
	else	/* "changed" sig, entry is pending on "activate" or config close */
		gkrellm_dup_string(&pending_notify, s);
	}

static void
config_destroyed(void)
	{
	GList		*list;
	MailAccount	*account;
	Mailproc	*mp = (Mailproc *) mail_fetch->private;

	if (pending_fetch && gkrellm_dup_string(&mp->command, pending_fetch))
			reset_mail_fetch();
	g_free(pending_fetch);
	pending_fetch = NULL;

	if (pending_notify)
		gkrellm_dup_string(&mail_notify, pending_notify);
	g_free(pending_notify);
	pending_notify = NULL;

	for (list = config_mailbox_list; list; list = list->next)
		{
		account = (MailAccount *) list->data;
		free_account(account);
		}
	g_list_free(config_mailbox_list);
	config_mailbox_list = NULL;
	}

static void
copy_mailbox_accounts(void)
	{
	GList		*list;
	MailAccount	*account;
	Mailbox		*mbox;

	for (list = mailbox_list; list; list = list->next)
		{
		mbox = (Mailbox *) list->data;
		if (! (mbox->account->mboxtype & MBOX_INTERNAL))
			continue;
		account = g_new0(MailAccount, 1);
		dup_account(account, mbox->account);
		config_mailbox_list = g_list_append(config_mailbox_list, account);
		}
	}

static void
add_menu_item(GtkWidget *menu, gchar *label, gint type)
	{
	GtkWidget	*menuitem;

	menuitem = gtk_menu_item_new_with_label(label);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	g_object_set_data(G_OBJECT(menuitem), "user_data", GINT_TO_POINTER(type));
	g_signal_connect(G_OBJECT(menuitem), "activate",
			G_CALLBACK(cb_protocol_selected), NULL);
	}

#ifdef HAVE_SSL
static void
add_ssl_menu_item(GtkWidget *menu, gchar *label, gint type)
	{
	GtkWidget	*menuitem;

	menuitem = gtk_menu_item_new_with_label(label);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	g_object_set_data(G_OBJECT(menuitem), "user_data",
			  GINT_TO_POINTER(type));
	g_signal_connect(G_OBJECT(menuitem), "activate",
			 G_CALLBACK(cb_ssl_selected), NULL);
	}
#endif

static gchar	*mail_info_text0[]	=
{
N_("<h>Mailboxes\n"),

N_("Mailboxes to monitor can be local or remote mailbox types.\n"),
N_("For local mailboxes, the path name may be a mbox file or it may be\n"
"a Maildir or MH mail style directory.\n"),
"\n"
};

static gchar	*mail_info_text1[]	=
{
N_("<h>Setup\n"),
N_("<b>Mail reading program\n"),
N_("If you enter a mail reading program (your mail user agent or MUA)\n"
"it can be launched by clicking on the mail monitor message count button.\n"),
"\n",
N_("<b>Sound notify program\n"),
N_("If you enter a notify (sound) command, it will be run when new mail\n"
"is detected.\n"),
"\n"
};

static gchar	*mail_info_text2[]	=
{
N_("<b>fetch/check Program\n"),
N_("If you want to download remote mail or check for remote mail without\n"
"using the builtin POP3 or IMAP mail checking which is set up in the\n"),
N_("<i>Mailboxes"),
N_(" tab, then do this by entering a mail fetch/check command.\n"
"For example, fetchmail can be run periodically to download mail\n"
"messages from a remote server to your local mailboxes where GKrellM\n"
"can count them.\n\n"

"Or, GKrellM can read the output and report the results from some mail\n"
"checking programs which count remote or local messages.  If you enter\n"
"a command like this that checks mail without downloading, then you\n"
"must tell GKrellM this is the expected behaviour by selecting in the\n"
"Options tab:\n"),
N_("<i>\tFetch/check program checks messages only\n"),
N_("For checking messages on a remote server, GKrellM can read the\n"
"output of the program "),
N_("<i>fetchmail -c"),
N_(" (you must append the -c).\n"),
N_("But, do not combine these methods for the same mailbox!  If you enter a\n"
"POP3 mailbox in the "),
N_("<i>Mailboxes"),
N_(" tab, then don't check it again with fetchmail.\n")
};

static gchar	*mail_info_text3[]	=
{
N_("<h>\nMouse Button Actions:\n"),
N_("<b>\tLeft "),
N_("click the mail count button to launch the mail reading program.\n"),
N_("\t\tIf options permit, also stop animations and reset remote counts.\n"),
N_("<b>\tLeft "),
N_("click the envelope decal to force a mail check regardless of\n"
"\t\tany mute mode or inhibit mail checking option settings.\n"),
N_("<b>\tMiddle "),
N_("click the mail panel to toggle a mute mode which inhibits\n"
	"\t\tthe sound notify program and optionally inhibits all mail\n"
	"\t\tchecking.\n"),
};


static void
create_mail_tab(GtkWidget *tab_vbox)
	{
	GtkWidget		*tabs;
	GtkWidget		*table;
	GtkWidget		*menu;
#ifdef HAVE_SSL
	GtkWidget		*ssl_menu;
#endif
	GtkWidget		*vbox, *vbox1, *hbox, *hbox1;
	GtkWidget		*label;
	GtkWidget		*button;
	GtkWidget		*scrolled;
	GtkWidget		*text;
	GtkTreeModel	*model;
	GtkCellRenderer	*renderer;
	GSList			*group;
	gint			i;

	row_reference = NULL;
	mh_seq_ignore_button = NULL;

	copy_mailbox_accounts();
	tabs = gtk_notebook_new();
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(tabs), GTK_POS_TOP);
	gtk_box_pack_start(GTK_BOX(tab_vbox), tabs, TRUE, TRUE, 0);
	g_signal_connect(G_OBJECT(tabs),"destroy",
				G_CALLBACK(config_destroyed), NULL);

/* --Setup tab */
	vbox = gkrellm_gtk_framed_notebook_page(tabs, _("Setup"));

	gkrellm_gtk_check_button_connected(vbox, NULL,
		enable_mail, FALSE, FALSE, 10,
		cb_enable, NULL,
		_("Enable Mailcheck"));

	vbox1 = gkrellm_gtk_framed_vbox_end(vbox, NULL, 4, FALSE, 0, 2);
	table = gtk_table_new(7, 2, FALSE /*non-homogeneous*/);
	gtk_table_set_col_spacings(GTK_TABLE(table), 2);
	gtk_table_set_row_spacings(GTK_TABLE(table), 3);
	gtk_box_pack_start(GTK_BOX(vbox1), table, FALSE, FALSE, 2);

	hbox = gtk_hbox_new(TRUE, 0);
	/* Attach left right top bottom */
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 0, 1,
				GTK_SHRINK, GTK_SHRINK, 0, 0);
	label = gtk_label_new(_("Mail reading program:"));
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 4);
	mail_user_agent_entry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(mail_user_agent_entry), 255);
	gtk_table_attach_defaults(GTK_TABLE(table), mail_user_agent_entry,
				1, 2, 0, 1);
	gtk_entry_set_text(GTK_ENTRY(mail_user_agent_entry),
				mail_user_agent.command);
	g_signal_connect(G_OBJECT(mail_user_agent_entry), "changed",
				G_CALLBACK(cb_mua), NULL);

	hbox = gtk_hbox_new(TRUE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 1, 2,
				GTK_SHRINK, GTK_SHRINK, 0, 0);
	label = gtk_label_new(_("Notify (sound) program:"));
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 4);
	mail_notify_entry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(mail_notify_entry), 255);
	gtk_table_attach_defaults(GTK_TABLE(table), mail_notify_entry, 1, 2, 1, 2);
	gtk_entry_set_text(GTK_ENTRY(mail_notify_entry), mail_notify);
	g_signal_connect(G_OBJECT(mail_notify_entry), "activate",
				G_CALLBACK(cb_mail_notify), GINT_TO_POINTER(1));
	g_signal_connect(G_OBJECT(mail_notify_entry), "changed",
				G_CALLBACK(cb_mail_notify), GINT_TO_POINTER(0));

	if (local_supported)
		{
		hbox = gtk_hbox_new(TRUE, 0);
		gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 2, 3,
					GTK_SHRINK, GTK_SHRINK, 0, 0);
		label = gtk_label_new(_("Mail fetch/check program:"));
		gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
		gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 4);
		mail_fetch_entry = gtk_entry_new();
		gtk_entry_set_max_length(GTK_ENTRY(mail_fetch_entry), 255);
		gtk_table_attach_defaults(GTK_TABLE(table), mail_fetch_entry,
					1, 2, 2, 3);
		gtk_entry_set_text(GTK_ENTRY(mail_fetch_entry),
					((Mailproc *)mail_fetch->private)->command);
		g_signal_connect(G_OBJECT(mail_fetch_entry), "activate",
					G_CALLBACK(cb_mail_fetch), GINT_TO_POINTER(1));
		g_signal_connect(G_OBJECT(mail_fetch_entry), "changed",
					G_CALLBACK(cb_mail_fetch), GINT_TO_POINTER(0));

		hbox = gtk_hbox_new(FALSE, 0);
		gtk_table_attach_defaults(GTK_TABLE(table), hbox, 1, 2, 3, 4);
		gkrellm_gtk_check_button_connected(hbox, NULL, fetch_check_is_local,
					FALSE, FALSE, 3,
					toggle_button_cb, &fetch_check_is_local,
					_("Run fetch/check program at local interval"));

		label = gtk_label_new(" ");
		gtk_table_attach(GTK_TABLE(table), label, 0, 1, 4, 5,
					GTK_SHRINK, GTK_SHRINK, 0, 0);


		hbox = gtk_hbox_new(TRUE, 0);
		gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 5, 6,
					GTK_SHRINK, GTK_SHRINK, 0, 0);
		label = gtk_label_new(_("Check local mailboxes every"));
		gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
		gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 4);
		hbox = gtk_hbox_new(FALSE, 0);
		gtk_table_attach_defaults(GTK_TABLE(table), hbox, 1, 2, 5, 6);
		gkrellm_gtk_spin_button(hbox, NULL,
				(gfloat) local_check_timeout, 2.0, 100.0, 1.0, 5.0, 0, 60,
				local_check_timeout_cb, NULL, FALSE, _("seconds"));
		}
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, 6, 7,
				GTK_SHRINK, GTK_SHRINK, 0, 0);
	if (local_supported)
		label = gtk_label_new(_("Do remote checks every"));
	else
		label = gtk_label_new(_("Check mail every"));
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 4);
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_table_attach_defaults(GTK_TABLE(table), hbox, 1, 2, 6, 7);
	gkrellm_gtk_spin_button(hbox, NULL,
			(gfloat) remote_check_timeout, 1.0, 60.0, 1.0, 5.0, 0, 60,
			remote_check_timeout_cb, NULL, FALSE, _("minutes"));

/* --Mailboxes tab */
	vbox = gkrellm_gtk_framed_notebook_page(tabs, _("Mailboxes"));
//	vbox1 = gkrellm_gtk_framed_vbox(vbox, NULL, 3, FALSE, 0, 3);
	vbox1 = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(vbox), vbox1);

	account_notebook = gtk_notebook_new();
	gtk_box_pack_start(GTK_BOX(vbox1), account_notebook, FALSE, FALSE, 2);
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(account_notebook), FALSE);

	/* Local mailbox account entry */
	if (local_supported)
		{
		vbox1 = gtk_vbox_new(FALSE, 0);
		gtk_notebook_append_page(GTK_NOTEBOOK(account_notebook), vbox1, NULL);
		hbox = gtk_hbox_new(FALSE, 2);
		gtk_box_pack_start(GTK_BOX(vbox1), hbox, FALSE, FALSE, 0);
		label = gtk_label_new(_("Path name:"));
		gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
		mbox_path_entry = gtk_entry_new();
		gtk_entry_set_max_length(GTK_ENTRY(mbox_path_entry), 255);
		gtk_box_pack_start(GTK_BOX(hbox), mbox_path_entry, TRUE, TRUE, 2);
		gtk_entry_set_text(GTK_ENTRY(mbox_path_entry), "");
		g_signal_connect (G_OBJECT (mbox_path_entry), "activate",
				G_CALLBACK(mailbox_enter_cb), NULL);			
		}

	/* Remote mailbox account entry */
	table = gtk_table_new(4 /* rows */, 4, FALSE /*non-homogeneous*/);
	gtk_table_set_row_spacings(GTK_TABLE(table), 2);
	gtk_table_set_col_spacings(GTK_TABLE(table), 6);
	gtk_table_set_col_spacing(GTK_TABLE(table), 1, 16);
	gtk_notebook_append_page(GTK_NOTEBOOK(account_notebook), table, NULL);

	label = gtk_label_new(_("Server"));
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 0, 1);
	server_entry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(server_entry), 255);
	gtk_table_attach_defaults(GTK_TABLE(table), server_entry, 1, 2, 0, 1);

	label = gtk_label_new(_("User name"));
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 1, 2);
	user_entry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(user_entry), 255);
	gtk_table_attach_defaults(GTK_TABLE(table), user_entry, 1, 2, 1, 2);

	label = gtk_label_new(_("Password"));
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 2, 3);
	password_entry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(password_entry), 255);
	gtk_table_attach_defaults(GTK_TABLE(table), password_entry, 1, 2, 2, 3);
	gtk_entry_set_visibility(GTK_ENTRY(password_entry), FALSE);

	label = gtk_label_new(_("Protocol"));
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 2, 3, 0, 1);

	remote_option_menu = gtk_option_menu_new();
	gtk_table_attach_defaults(GTK_TABLE(table), remote_option_menu, 3, 4, 0,1);
	menu = gtk_menu_new();
	for (i = 0; auth_strings[i].string != NULL; ++i)
		add_menu_item(menu, x_out(auth_strings[i].string), i);
	gtk_widget_show(menu);
	gtk_option_menu_set_menu(GTK_OPTION_MENU(remote_option_menu), menu);

	i = 1;

#ifdef HAVE_SSL
	label = gtk_label_new(_("Use SSL"));
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 2, 3, i, i+1);

	ssl_option_menu = gtk_option_menu_new();
	gtk_table_attach_defaults(GTK_TABLE(table), ssl_option_menu, 3, 4, i, i+1);
	++i;
	ssl_menu = gtk_menu_new();
	add_ssl_menu_item(ssl_menu, _("No"), SSL_NONE);
	add_ssl_menu_item(ssl_menu, "SSL", SSL_TRANSPORT);
	add_ssl_menu_item(ssl_menu, "STARTTLS", SSL_STARTTLS);
	gtk_widget_show(ssl_menu);
	gtk_option_menu_set_menu(GTK_OPTION_MENU(ssl_option_menu), ssl_menu);
#endif


	label = gtk_label_new(_("IMAP folder"));
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_table_attach_defaults(GTK_TABLE(table), label, 2, 3, i, i+1);
	imapfolder_entry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(imapfolder_entry), 255);
	gtk_table_attach_defaults(GTK_TABLE(table), imapfolder_entry,
				3, 4, i, i+1);
	++i;
	gkrellm_gtk_check_button_connected(NULL, &port_button,
				FALSE, FALSE, FALSE, 0, cb_specify_port, NULL,
				_("Specify port"));
	hbox = gtk_hbox_new(TRUE, 0);
	gtk_table_attach_defaults(GTK_TABLE(table), hbox, 2, 3, i, i+1);
	gtk_box_pack_start(GTK_BOX(hbox), port_button, FALSE, FALSE, 0);
	port_entry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(port_entry), 255);
	gtk_table_attach_defaults(GTK_TABLE(table), port_entry, 3, 4, i, i+1);
	gtk_widget_set_sensitive(port_entry, FALSE);

	hbox1 = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox1, FALSE, FALSE, 0);
	hbox = gtk_hbutton_box_new();
	gtk_button_box_set_layout(GTK_BUTTON_BOX(hbox), GTK_BUTTONBOX_END);
	gtk_box_set_spacing(GTK_BOX(hbox), 5);
	gtk_box_pack_end(GTK_BOX(hbox1), hbox, FALSE, FALSE, 0);

	if (local_supported)
		{
		local_button = gtk_radio_button_new_with_label(NULL,
					_("Local mailbox"));
		gtk_box_pack_start(GTK_BOX(hbox1), local_button, FALSE, FALSE, 5);
		g_signal_connect(G_OBJECT(local_button), "clicked",
					G_CALLBACK(cb_mailbox_group), GINT_TO_POINTER(0));
		remote_button = gtk_radio_button_new_with_label_from_widget(
					GTK_RADIO_BUTTON(local_button), _("Remote mailbox"));
		gtk_box_pack_start(GTK_BOX(hbox1), remote_button, FALSE, FALSE, 0);
		g_signal_connect(G_OBJECT(remote_button), "clicked",
					G_CALLBACK(cb_mailbox_group), GINT_TO_POINTER(1));
		}
	else
		default_port_entry();

	gkrellm_gtk_button_connected(hbox, &delete_button, FALSE, FALSE, 5,
			mailbox_delete_cb, NULL, GTK_STOCK_DELETE);
	gtk_widget_set_sensitive(delete_button, FALSE);

	gkrellm_gtk_button_connected(hbox, &new_apply_button, FALSE, FALSE, 5,
			mailbox_enter_cb, NULL, GTK_STOCK_NEW);

	vbox1 = gkrellm_gtk_framed_vbox(vbox, NULL, 6, TRUE, 0, 3);
	scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(GTK_BOX(vbox1), scrolled, TRUE, TRUE, 0);

	model = create_model();
	treeview = GTK_TREE_VIEW(gtk_tree_view_new_with_model(model));
	g_object_unref(G_OBJECT(model));
	gtk_tree_view_set_rules_hint(treeview, TRUE);
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes(treeview, -1, _("Protocol"),
				renderer,
				"text", PROTOCOL_COLUMN, NULL);
#ifdef HAVE_SSL
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes(treeview, -1, "SSL",
				renderer,
				"text", SSL_COLUMN, NULL);
#endif
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes(treeview, -1, _("Mailbox"),
				renderer,
				"text", MAILBOX_COLUMN, NULL);

	gtk_container_add(GTK_CONTAINER(scrolled), GTK_WIDGET(treeview));
	selection = gtk_tree_view_get_selection(treeview);
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
	g_signal_connect(G_OBJECT(selection), "changed",
				G_CALLBACK(cb_tree_selection_changed), NULL);

/* --Animation tab */
	vbox = gkrellm_gtk_framed_notebook_page(tabs, _("Animation"));

	vbox1 = gkrellm_gtk_category_vbox(vbox,
				_("Animation Select"),
				4, 0, TRUE);
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox1), hbox, FALSE, FALSE, 0);

	anim_button[0] = gtk_radio_button_new_with_label(NULL, _("None"));
	gtk_box_pack_start(GTK_BOX(hbox), anim_button[0], TRUE, TRUE, 0);
	anim_button[1] = gtk_radio_button_new_with_label_from_widget(
					GTK_RADIO_BUTTON(anim_button[0]), _("Envelope"));
	gtk_box_pack_start(GTK_BOX(hbox), anim_button[1], TRUE, TRUE, 0);
	anim_button[2] = gtk_radio_button_new_with_label_from_widget(
					GTK_RADIO_BUTTON(anim_button[1]),
#ifdef BSD
					_("Daemon"));
#else
					_("Penguin"));
#endif
	gtk_box_pack_start(GTK_BOX(hbox), anim_button[2], TRUE, TRUE, 0);
	anim_button[3] = gtk_radio_button_new_with_label_from_widget(
					GTK_RADIO_BUTTON(anim_button[2]), _("Both"));
	gtk_box_pack_start(GTK_BOX(hbox), anim_button[3], TRUE, TRUE, 0);

	button = anim_button[animation_mode];
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);

	for (i = 0; i < 4; ++i)
		g_signal_connect(G_OBJECT(anim_button[i]), "toggled",
					G_CALLBACK(cb_animation_mode), GINT_TO_POINTER(i));

    gkrellm_gtk_check_button_connected(vbox, &enable_cont_anim_button,
		cont_animation_mode, FALSE, FALSE, 10,
		multi_toggle_button_cb, NULL,
		_("Run animation continuously as long as there is a new mail count"));


/* --Options tab */
	vbox = gkrellm_gtk_framed_notebook_page(tabs, _("Options"));

	vbox1 = gkrellm_gtk_category_vbox(vbox,
				_("Message Counting"),
				4, 0, TRUE);
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox1), hbox, FALSE, FALSE, 0);

	button = gtk_radio_button_new_with_label(NULL, _("new/total"));
	gtk_box_pack_start(GTK_BOX (hbox), button, TRUE, TRUE, 0);
	group = gtk_radio_button_get_group(GTK_RADIO_BUTTON (button));
	count_mode_button[0] = button;
	button = gtk_radio_button_new_with_label(group, _("new"));
	gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
	group = gtk_radio_button_get_group(GTK_RADIO_BUTTON (button));
	count_mode_button[1] = button;
	button = gtk_radio_button_new_with_label(group, _("do not count"));
	gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
	count_mode_button[2] = button;
	button = count_mode_button[count_mode];
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
	for (i = 0; i < 3; ++i)
		g_signal_connect(G_OBJECT(count_mode_button[i]), "toggled",
					G_CALLBACK(cb_count_mode), GINT_TO_POINTER(i));

	if (local_supported)
	    gkrellm_gtk_check_button_connected(vbox, &check_only_button,
			fetch_check_only_mode, TRUE, TRUE, 0,
			multi_toggle_button_cb, NULL,
			_("Fetch/check program checks messages only (see Mail Info tab)"));

    gkrellm_gtk_check_button_connected(vbox, &reset_remote_button,
		reset_remote_mode, TRUE, TRUE, 0,
		multi_toggle_button_cb, NULL,
	   _("Reset remote message counts when message count button is pressed."));

    gkrellm_gtk_check_button_connected(vbox, &unseen_is_new_button,
		unseen_is_new, TRUE, TRUE, 0,
		multi_toggle_button_cb, NULL,
	_("Count accessed but unseen mail as new (if this status is available)"));

    gkrellm_gtk_check_button_connected(vbox, &super_mute_button,
		super_mute_mode, TRUE, TRUE, 0,
		multi_toggle_button_cb, NULL,
   _("Mute mode inhibits all mail checking, not just notify (sound) program"));

    gkrellm_gtk_check_button_connected(vbox, &mua_inhibit_button,
		mua_inhibit_mode, TRUE, TRUE, 0,
		multi_toggle_button_cb, NULL,
		_("Inhibit all mail checking while the mail reader is running"));

	gkrellm_gtk_check_button_connected(vbox, &enable_multimua_button,
		enable_multimua, TRUE, TRUE, 0,
		multi_toggle_button_cb, NULL,
		_("Allow multiple launches of the mail reader program"));

    gkrellm_gtk_check_button_connected(vbox, &show_tooltip_button,
		show_tooltip, TRUE, TRUE, 0,
		multi_toggle_button_cb, NULL,
		_("List mailboxes containing new mail in a tooltip"));

	if (checking_mh_mail && have_mh_sequences)
	    gkrellm_gtk_check_button_connected(vbox, &mh_seq_ignore_button,
			mh_seq_ignore, TRUE, TRUE, 0,
			multi_toggle_button_cb, NULL,
			_("Ignore .mh_sequences when checking MH mail."));
	
/* --Info tab */
	vbox = gkrellm_gtk_framed_notebook_page(tabs, _("Info"));
	text = gkrellm_gtk_scrolled_text_view(vbox, NULL,
				GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	if (local_supported)
		for (i = 0; i < sizeof(mail_info_text0)/sizeof(gchar *); ++i)
			gkrellm_gtk_text_view_append(text, _(mail_info_text0[i]));
	for (i = 0; i < sizeof(mail_info_text1)/sizeof(gchar *); ++i)
		gkrellm_gtk_text_view_append(text, _(mail_info_text1[i]));
	if (local_supported)
		for (i = 0; i < sizeof(mail_info_text2)/sizeof(gchar *); ++i)
			gkrellm_gtk_text_view_append(text, _(mail_info_text2[i]));
	for (i = 0; i < sizeof(mail_info_text3)/sizeof(gchar *); ++i)
		gkrellm_gtk_text_view_append(text, _(mail_info_text3[i]));
	}




static GkrellmMonitor	monitor_mail =
	{
	N_("Mail"),			/* Name, for config tab.	*/
	MON_MAIL,			/* Id,  0 if a plugin		*/
	create_mail,		/* The create function		*/
	update_mail,		/* The update function		*/
	create_mail_tab,	/* The config tab create function	*/
	NULL,				/* Instant apply		*/

	save_mail_config,	/* Save user conifg			*/
	load_mail_config,	/* Load user config			*/
	"mail",				/* config keyword			*/

	NULL,				/* Undef 2	*/
	NULL,				/* Undef 1	*/
	NULL,				/* Undef 0	*/

	0,					/* insert_before_id - place plugin before this mon */

	NULL,				/* Handle if a plugin, filled in by GKrellM		*/
	NULL				/* path if a plugin, filled in by GKrellM		*/
	};

GkrellmMonitor *
gkrellm_init_mail_monitor(void)
	{
	monitor_mail.name = _(monitor_mail.name);
	enable_mail = TRUE;
	show_tooltip = TRUE;
	enable_multimua = FALSE;
	cont_animation_mode = FALSE;
	super_mute_mode = FALSE;
	mua_inhibit_mode = FALSE;
	_GK.decal_mail_frames = 18;
	_GK.decal_mail_delay = 1;

#ifdef HAVE_GNUTLS
	gcry_control (GCRYCTL_SET_THREAD_CBS, &gcry_threads_pthread);
	gnutls_global_init();
#endif

	mail_fetch = g_new0(Mailbox, 1);
	mail_fetch->account = g_new0(MailAccount, 1);
	mail_fetch->private = g_new0(Mailproc, 1);
	mail_fetch->account->mboxtype = MBOX_FETCH;
	mail_fetch->check_func = run_fetch_program;
	mailbox_list = g_list_append(mailbox_list, mail_fetch);

	((Mailproc *)(mail_fetch->private))->command = g_strdup("");
	((Mailproc *)(mail_fetch->private))->pipe = -1;
	mail_user_agent.command = g_strdup("");
	mail_user_agent.pipe = -1;
	mail_notify = g_strdup("");

	style_id = gkrellm_add_meter_style(&monitor_mail, MAIL_STYLE_NAME);
	force_mail_check = TRUE;	/* Do a check at startup */

	mon_mail = &monitor_mail;
	return &monitor_mail;
	}


  /* ====================== Exported Mail Functions =====================*/

gboolean
gkrellm_get_mail_mute_mode(void)
	{
	return mute_mode;
	}

  /* ====================================================================*/
  /* Functions to allow for external mailboxes, as in plugins which want
  |  to check for alternate mailbox types but have the mail counts reported
  |  by the builtin mailbox monitor.  A plugin outline would be:
  |
  |	typedef struct
  |		{
  |		gpointer mbox_ptr;
  |		... local plugin stuff ...
  |		} PluginMbox;
  |
  |	PluginMbox	pmbox;
  |
  |	void create_plugin_mbox(GtkWidget *vbox, gint first_create)
  |		{  //See plugin programmers reference for general structure
  |		...
  |		pmbox.mbox_ptr = gkrellm_add_external_mbox(pmbox_check, TRUE, &pmbox);
  |		gkrellm_set_external_mbox_tooltip(pmbox.mbox_ptr, "mbox_name_stuff");
  |		...
  |		}
  |
  |	gint pmbox_check(PluginMbox *pmb)
  |		{ //Collect info from the mail box
  |		gkrellm_set_external_mbox_counts(pmb->mbox_ptr, total_mail, new_mail);
  |		}
  */

  /* External mailbox counts won't show in the tooltip unless you call this.
  |  If a plugin wants only to use the sound/animation feature and not show
  |  up in a tooltip, then do not make this call.
  */
void
gkrellm_set_external_mbox_tooltip(gpointer mbox_ptr, gchar *string)
	{
	Mailbox *mbox = (Mailbox *) mbox_ptr;

	gkrellm_dup_string(&mbox->account->path, string);
	}

  /* Set total and new message counts for an external mailbox so the counts
  |  can appear in a tooltip (must also call above routine), and so the
  |  animation/sound can be triggered.  Since sound and animation is triggered
  |  synchronously with remote and local checks, a plugin should
  |  call this routine from within a check_func that is setup in the
  |  gkrellm_add_external_mbox() routine.
  */
void
gkrellm_set_external_mbox_counts(gpointer mbox_ptr, gint total, gint new)
	{
	Mailbox *mbox = (Mailbox *) mbox_ptr;

	mbox->mail_count = total;
	mbox->new_mail_count = new;
	}

  /* A plugin can have a check_func() called at the mail monitors local
  |  check interval (threaded is FALSE) or at the remote check interval
  |  (threaded is TRUE).  Additionally, if threaded is TRUE, the check_func
  |  will be called as a thread.  The data pointer is a pointer to a plugin
  |  defined structure which specifies a plugins unique mailbox.  This data
  |  pointer will be passed as the argument to check_func(data) when it
  |  is called at the update intervals.  gkrellm_add_external_mbox() returns
  |  the internal Mailbox * which should be treated simply as a gpointer
  |  in the plugin and it must be used as the first argument to the above
  |  gkrellm_set_external_tooltip() and gkrellm_set_external_counts().
  */
gpointer
gkrellm_add_external_mbox(gboolean (*check_func)(), gboolean threaded,
			gpointer data)
	{
	Mailbox	*mbox;

	mbox = g_new0(Mailbox, 1);
	mbox->account = g_new0(MailAccount, 1);
	mbox->account->mboxtype = threaded ? MBOX_REMOTE_PLUGIN :MBOX_LOCAL_PLUGIN;
	mbox->check_func = check_func;
	mbox->data = data;
	mailbox_list = g_list_append(mailbox_list, mbox);
	return (gpointer) mbox;
	}

void
gkrellm_destroy_external_mbox(gpointer mbox_ptr)
	{
	GList	*tmp;

	if ((tmp = g_list_find(mailbox_list, mbox_ptr)) == NULL)
		return;
	mailbox_list = g_list_remove_link(mailbox_list, tmp);
	g_list_free(tmp);
	free_mailbox(mbox_ptr);
	}

  /* =======================================================================*/

