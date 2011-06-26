#ifndef GK_CONFIGURE_H
#define GK_CONFIGURE_H
#cmakedefine HAVE_GETADDRINFO
#cmakedefine HAVE_GETHOSTBYNAME
#cmakedefine INET6 /* TODO: rename to HAVE_INET6 */
#cmakedefine HAVE_GNUTLS
#cmakedefine HAVE_NTLM
#cmakedefine HAVE_LIBSENSORS
#cmakedefine HAVE_SSL
#define LOCALEDIR "${GKRELLM_LOCALEDIR}"
#endif // GK_CONFIGURE_H

