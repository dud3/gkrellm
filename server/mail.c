/* GKrellM
|  Copyright (C) 1999-2009 Bill Wilson
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
|
|
|  Additional permission under GNU GPL version 3 section 7
|
|  If you modify this program, or any covered work, by linking or
|  combining it with the OpenSSL project's OpenSSL library (or a
|  modified version of that library), containing parts covered by
|  the terms of the OpenSSL or SSLeay licenses, you are granted
|  additional permission to convey the resulting work.
|  Corresponding Source for a non-source form of such a combination
|  shall include the source code for the parts of OpenSSL used as well
|  as that of the covered work.
*/

#include "gkrellmd.h"
#include "gkrellmd-private.h"


#if !defined(WIN32)

#define MBOX_MBOX		0
#define MBOX_MAILDIR	1
#define MBOX_MH_DIR		2


typedef struct
	{
	gchar		*path;
	gchar		*homedir_path;
	gint		mboxtype;
	gboolean	(*check_func)();
	gint		mail_count;
	gint		new_mail_count;
	gint		old_mail_count;
	gint		prev_mail_count,
				prev_new_mail_count;
	time_t		last_mtime;
	off_t		last_size;
	gboolean	is_internal;	/* Internal mail message (ie: localmachine) */
	gboolean	changed;
	}
	Mailbox;

static GList	*mailbox_list;

static gint		mail_check_timeout = 5;			/* Seconds */

static gboolean	unseen_is_new = TRUE;			/* Accessed but unread */

static gboolean	mail_need_serve;


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

static gboolean
mh_sequences_new_count(Mailbox *mbox)
	{
	FILE	*f;
	gchar	buf[1024];
	gchar	*path, *tok;
	gint	n0, n1;

	path = g_strconcat(mbox->path, G_DIR_SEPARATOR_S,
				".mh_sequences", NULL);
	f = fopen(path, "r");
	g_free(path);
	if (!f)
		return FALSE;
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
	FILE	*f;
	gchar	*path;
	gint	msgnum, flags, ver, mark_files = 0;

	path = g_strconcat(mbox->path, G_DIR_SEPARATOR_S,
				".sylpheed_mark", NULL);
	f = fopen(path, "rb");
	g_free(path);
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

	if ((dir = g_dir_open(mbox->path, 0, NULL)) == NULL)
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
	snprintf(path, sizeof(path), "%s%cnew", mbox->path,
				G_DIR_SEPARATOR);
	if ((dir = g_dir_open(path, 0, NULL)) != NULL)
		{
		while ((name = (gchar *) g_dir_read_name(dir)) != NULL)
			mbox->new_mail_count++;
		g_dir_close(dir);
		}
	mbox->mail_count = mbox->new_mail_count;
	snprintf(path, sizeof(path), "%s%ccur", mbox->path,
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
		g_print(_("mdir %s total=%d old=%d new=%d\n"), mbox->path,
			mbox->mail_count, mbox->old_mail_count, mbox->new_mail_count);
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

	if (   !strncmp(buf, "Status:", 7)  /* Standard mail clients */
	    || !strncmp(buf, "X-Mozilla-Status:", 17) /* Netscape */
	    || !strncmp(buf, "X-Evolution:", 12)      /* Mozilla */
	   )
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

	if (stat(mbox->path, &s) != 0)
		{
		mbox->mail_count = mbox->old_mail_count = mbox->new_mail_count = 0;
		mbox->last_mtime = 0;
		mbox->last_size = 0;
		if (_GK.debug_level & DEBUG_MAIL)
			printf("check_mbox can't stat(%s): %s\n", mbox->path,
						g_strerror(errno));
		return FALSE;
		}

	/* If the mailboxes have been modified since last check, count
	|  the new/total messages.
	*/
	if (   s.st_mtime != mbox->last_mtime
		|| s.st_size  != mbox->last_size
	   )
		{
		if ((f = fopen(mbox->path, "r")) == NULL)
			{
			if (_GK.debug_level & DEBUG_MAIL)
				printf("check_mbox can't fopen(%s): %s\n", mbox->path,
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
				}
			else if (is_From_line(mbox, buf))
				{
				mbox->mail_count += 1;
				in_header = TRUE;
				marked_read = FALSE;
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
		utime(mbox->path, &ut);

		mbox->last_mtime = s.st_mtime;
		mbox->last_size = s.st_size;
		if (_GK.debug_level & DEBUG_MAIL)
			g_print("mbox read <%s> total=%d old=%d\n",
					mbox->path,
					mbox->mail_count, mbox->old_mail_count);
		}

	/* If mbox has been accessed since last modify a MUA has probably read
	|  the mbox.
	*/
	mbox->new_mail_count = mbox->mail_count - mbox->old_mail_count;
	if (s.st_atime > s.st_mtime)
		{
		mbox->prev_new_mail_count = mbox->new_mail_count;
		}
    return TRUE;
	}


static void
update_mail(GkrellmdMonitor *mon, gboolean force)
	{
	Mailbox		*mbox;
	GList		*list;
	static gint	second_count;

	if (   (!GK.second_tick || (++second_count % mail_check_timeout) != 0)
		&& !force
	   )
		return;

	for (list = mailbox_list; list; list = list->next)
		{
		mbox = (Mailbox *) list->data;
		if (mbox->check_func)
			(*mbox->check_func)(mbox);

		if (   mbox->prev_mail_count != mbox->mail_count
			|| mbox->prev_new_mail_count != mbox->new_mail_count
		   )
			{
			mbox->changed = TRUE;
			mail_need_serve = TRUE;
			gkrellmd_need_serve(mon);
			}
		mbox->prev_mail_count = mbox->mail_count;
		mbox->prev_new_mail_count = mbox->new_mail_count;
		}
	}


static void
get_local_mboxtype(Mailbox *mbox)
	{
	gchar	*path;

	if (*(mbox->path) == '~')
		{
		mbox->homedir_path = mbox->path;
		mbox->path = g_strdup_printf("%s%s", g_get_home_dir(),
					mbox->homedir_path + 1);
		}
	if (g_file_test(mbox->path, G_FILE_TEST_IS_DIR))
		{
#if GLIB_CHECK_VERSION(2,0,0)
		path = g_build_path(G_DIR_SEPARATOR_S, mbox->path, "new", NULL);
#else
		path = g_strconcat(mbox->path, G_DIR_SEPARATOR_S, "new", NULL);
#endif
		if (g_file_test(path, G_FILE_TEST_IS_DIR))
			mbox->mboxtype = MBOX_MAILDIR;
		else
			mbox->mboxtype = MBOX_MH_DIR;
		g_free(path);
		}
	else
		mbox->mboxtype = MBOX_MBOX;
	}

void
gkrellmd_add_mailbox(gchar *path)
	{
	Mailbox	*mbox;

	if (!path || !*path)
		return;
	mbox = g_new0(Mailbox, 1);
	mbox->path = g_strdup(path);
	get_local_mboxtype(mbox);

	if (mbox->mboxtype == MBOX_MAILDIR)
		mbox->check_func = check_maildir;
	else if (mbox->mboxtype == MBOX_MH_DIR)
		mbox->check_func = check_mh_dir;
	else
		mbox->check_func = check_mbox;

	mailbox_list = g_list_append(mailbox_list, mbox);
	gkrellmd_add_serveflag_done(&mbox->changed);
	}

/* ============================================================= */

static void
serve_mail_data(GkrellmdMonitor *mon, gboolean first_serve)
	{
	Mailbox		*mbox;
	GList		*list;
	gchar		*line;

	if ((!mail_need_serve && !first_serve) || !mailbox_list)
		return;
	gkrellmd_set_serve_name(mon, "mail");
	for (list = mailbox_list; list; list = list->next)
		{
		mbox = (Mailbox *) list->data;
		if (mbox->changed || first_serve)
			{
			line = g_strdup_printf("%s %d %d\n", mbox->homedir_path ?
						mbox->homedir_path : mbox->path,
						mbox->mail_count, mbox->new_mail_count);
			gkrellmd_serve_data(mon, line);
			g_free(line);
			}
		}
	}

static void
serve_mail_setup(GkrellmdMonitor *mon)
	{
	GkrellmdClient	*client = mon->privat->client;
	GList			*list;
	Mailbox			*mbox;
	gchar			*line;

	gkrellmd_send_to_client(client, "<mail_setup>\n");
	for (list = mailbox_list; list; list = list->next)
		{
		mbox = (Mailbox *) list->data;
		line = g_strdup_printf("%s\n", mbox->homedir_path ?
					mbox->homedir_path : mbox->path);
		gkrellmd_send_to_client(client, line);
		g_free(line);
		}
	}

static GkrellmdMonitor mail_monitor =
	{
	"mail",
	update_mail,
	serve_mail_data,
	serve_mail_setup
	};

GkrellmdMonitor *
gkrellmd_init_mail_monitor(void)
	{
	gkrellmd_add_serveflag_done(&mail_need_serve);
	return &mail_monitor;
	}

#else	/* defined(WIN32) */

GkrellmdMonitor *
gkrellmd_init_mail_monitor(void)
	{
	return NULL;
	}

void
gkrellmd_add_mailbox(gchar *path)
    {
    }

#endif


void
gkrellm_mail_local_unsupported(void)
	{
	/* WIN32 only calls this and it is taken care of by above #if */
	}

#if GLIB_CHECK_VERSION(2,0,0)
GThread *
#else
gpointer
#endif
gkrellm_mail_get_active_thread(void)
	{
	return NULL;
	}

