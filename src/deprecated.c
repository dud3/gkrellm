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

#include <string.h>


/* ---------------- Intercept GdkFont ------------------ */

extern int		gkrellm_gdk_text_width(void *font_desc,
						const char *text, int len);

extern void		*gkrellm_default_font(int n);
extern void		gkrellm_text_extents(void *font_desc, const char *text,
						int len, int *width, int *height,
						int *baseline, int *y_ink);

int
gdk_text_width(void *gdkfont, const char *string, int len)
	{
	void	*pfd;

	if (gdkfont == gkrellm_default_font(0))
		pfd = gdkfont;
	else if (gdkfont == gkrellm_default_font(2))
		pfd = gdkfont;
	else
		pfd = gkrellm_default_font(1);

	return gkrellm_gdk_text_width(pfd, string, len);
	}

int
gdk_string_width(void *font_desc, const char *string)
	{
	return gdk_text_width(font_desc, string, strlen(string));
	}

void
gdk_string_extents(void *font_desc, const char *string,
		int *l, int *r, int *w, int *a, int *d)
	{
	int	width, height, baseline, y_ink;

	gkrellm_text_extents(font_desc, string, strlen(string),
				&width, &height, &baseline, &y_ink);

	if (l)
		*l = 0;
	if (r)
		*r = width;
	if (w)
		*w = width;
	if (a)
		*a = baseline - y_ink;
	if (d)
		*d = y_ink + height - baseline;
	}


/* ---------------- Intercept setuid() and setreuid() ------------------ */
/* Enable this code as a debug test for when the gkrellmms plugin hangs.
|  libxmms calls setuid() and setreuid() which can cause a gkrellm
|  hang on Linux because of some kind of an interaction with running threads.
*/

#if 0

#include <sys/types.h>

int
setuid(uid_t u)
	{
//	printf("setuid() intercepted\n");
	return 0;
	}

int
setreuid(uid_t r, uid_t e)
	{
//	printf("setreuid() intercepted\n");
	return 0;
	}

#endif
