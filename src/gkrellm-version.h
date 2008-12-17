/* GKrellM
|  Copyright (C) 1999-2008 Bill Wilson
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
#ifndef GKRELLM_VERSION_H
#define GKRELLM_VERSION_H

#define	GKRELLM_VERSION_MAJOR	2
#define	GKRELLM_VERSION_MINOR	3
#define	GKRELLM_VERSION_REV		2
#define	GKRELLM_EXTRAVERSION	""

#define GKRELLM_CHECK_VERSION(major,minor,rev)    \
	(GKRELLM_VERSION_MAJOR > (major) || \
	(GKRELLM_VERSION_MAJOR == (major) && GKRELLM_VERSION_MINOR > (minor)) || \
	(GKRELLM_VERSION_MAJOR == (major) && GKRELLM_VERSION_MINOR == (minor) && \
	GKRELLM_VERSION_REV >= (rev)))

#endif // GKRELLM_VERSION_H
