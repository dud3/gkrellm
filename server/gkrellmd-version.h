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
#ifndef GKRELLMD_VERSION_H
#define GKRELLMD_VERSION_H

#define GKRELLMD_VERSION_MAJOR   2
#define GKRELLMD_VERSION_MINOR   3
#define GKRELLMD_VERSION_REV     3
#define GKRELLMD_EXTRAVERSION    ""

#define GKRELLMD_CHECK_VERSION(major,minor,rev)    \
(GKRELLMD_VERSION_MAJOR > (major) || \
(GKRELLMD_VERSION_MAJOR == (major) && GKRELLMD_VERSION_MINOR > (minor)) || \
(GKRELLMD_VERSION_MAJOR == (major) && GKRELLMD_VERSION_MINOR == (minor) && \
GKRELLMD_VERSION_REV >= (rev)))

#endif // GKRELLMD_VERSION_H
