/*
 * IRC notification system
 *
 * Copyright (c) 2008-2011, Christoph Mende <mende.christoph@gmail.com>
 * All rights reserved. Released under the 2-clause BSD license.
 */


#include <errno.h>
#include <stdarg.h>
#include <sys/un.h>

#include <glib.h>

#include "notifyserv.h"

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

/* Generic IPv6-capable connect function that opens a socket and connects it */
int server_connect(const char *host, unsigned short port)
{
	char service[6];
	fd_set write_flags;
	int sockfd = 0, valopt, ret;
	socklen_t lon;
	struct addrinfo hints, *result, *rp;
	struct timeval timeout;

	memset(&hints,0,sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	sprintf(service,"%hu",port);

	if((ret = getaddrinfo(host,service,&hints,&result)) != 0) {
		g_warning("[IRC] Failed to get address information: %s",
				gai_strerror(ret));
		freeaddrinfo(result);
		return -1;
	}

	for(rp = result;rp;rp = rp->ai_next) {
		if((sockfd = socket(rp->ai_family,rp->ai_socktype,rp->ai_protocol)) >= 0)
			break;
		close(sockfd);
	}

	if(fcntl(sockfd,F_SETFL,fcntl(sockfd,F_GETFL,0) | O_NONBLOCK) < 0) {
		freeaddrinfo(result);
		return -1;
	}

	if(connect(sockfd,rp->ai_addr,rp->ai_addrlen) < 0) {
		if(errno == EINPROGRESS) {
			timeout.tv_sec = 15;
			timeout.tv_usec = 0;

			FD_ZERO(&write_flags);
			FD_SET(sockfd,&write_flags);
			if(select(sockfd+1,NULL,&write_flags,NULL,&timeout) > 0) {
				lon = sizeof(int);
				getsockopt(sockfd,SOL_SOCKET,SO_ERROR,
					(void*)(&valopt),&lon);
				if(valopt) {
					errno = valopt;
					freeaddrinfo(result);
					return -1;
				}
			}
			else {
				errno = ETIMEDOUT;
				freeaddrinfo(result);
				return -1;
			}
		}
		else {
			freeaddrinfo(result);
			return -1;
		}
	}

	errno = 0;
	freeaddrinfo(result);
	return sockfd;
}

/* Logging function */
void notify_log(G_GNUC_UNUSED const gchar *log_domain, GLogLevelFlags log_level,
		const gchar *message, G_GNUC_UNUSED gpointer user_data)
{
	GDateTime *datetime;
	char *ts;
	FILE *fp;

	if (log_level > prefs.verbosity)
		return;

	if (!prefs.fork)
		fp = stdout;
	else
		fp = notify_info.log_fp;


	datetime = g_date_time_new_now_local();
	ts = g_date_time_format(datetime, "%Y-%m-%d %H:%M:%S  ");
	fputs(ts, fp);
	g_free(ts);

	fputs(message, fp);
	fputs("\n", fp);
	fflush(fp);
}
