/*
   rdesktop: A Remote Desktop Protocol client.
   Protocol services - TCP layer
   Copyright (C) Matthew Chapman 1999-2005
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <unistd.h>		/* select read write close */
#include <sys/socket.h>		/* socket connect setsockopt */
#include <sys/time.h>		/* timeval */
#include <netdb.h>		/* gethostbyname */
#include <netinet/in.h>		/* sockaddr_in */
#include <netinet/tcp.h>	/* TCP_NODELAY */
#include <arpa/inet.h>		/* inet_addr */
#include <errno.h>		/* errno */
#include "rdesktop.h"

#ifndef INADDR_NONE
#define INADDR_NONE ((unsigned long) -1)
#endif

static int sock;
static struct stream in;
static struct stream out;
int g_tcp_port_rdp = TCP_PORT_RDP;

/* Initialise TCP transport data packet */
STREAM
tcp_init(uint32 maxlen)
{
	if (maxlen > out.size)
	{
		out.data = (uint8 *) xrealloc(out.data, maxlen);
		out.size = maxlen;
	}

	out.p = out.data;
	out.end = out.data + out.size;
	return &out;
}

/* Send TCP transport data packet */
void
tcp_send(STREAM s)
{
	int length = s->end - s->data;
	int sent, total = 0;

	while (total < length)
	{
		sent = send(sock, s->data + total, length - total, 0);
		if (sent <= 0)
		{
			error("send: %s\n", strerror(errno));
			return;
		}

		total += sent;
	}
}

/* Receive a message on the TCP layer */
STREAM
tcp_recv(STREAM s, uint32 length)
{
	unsigned int new_length, end_offset, p_offset;
	int rcvd = 0;

	if (s == NULL)
	{
		/* read into "new" stream */
		if (length > in.size)
		{
			in.data = (uint8 *) xrealloc(in.data, length);
			in.size = length;
		}
		in.end = in.p = in.data;
		s = &in;
	}
	else
	{
		/* append to existing stream */
		new_length = (s->end - s->data) + length;
		if (new_length > s->size)
		{
			p_offset = s->p - s->data;
			end_offset = s->end - s->data;
			s->data = (uint8 *) xrealloc(s->data, new_length);
			s->size = new_length;
			s->p = s->data + p_offset;
			s->end = s->data + end_offset;
		}
	}

	while (length > 0)
	{
		if (!ui_select(sock))
			/* User quit */
			return NULL;

		rcvd = recv(sock, s->end, length, 0);
		if (rcvd < 0)
		{
			error("recv: %s\n", strerror(errno));
			return NULL;
		}
		else if (rcvd == 0)
		{
			error("Connection closed\n");
			return NULL;
		}

		s->end += rcvd;
		length -= rcvd;
	}

	return s;
}

/* Establish a connection on the TCP layer */
BOOL
tcp_connect(char *server)
{
	int true_value = 1;

#ifdef IPv6

	int n;
	struct addrinfo hints, *res, *ressave;
	char tcp_port_rdp_s[10];

	snprintf(tcp_port_rdp_s, 10, "%d", g_tcp_port_rdp);

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((n = getaddrinfo(server, tcp_port_rdp_s, &hints, &res)))
	{
		error("getaddrinfo: %s\n", gai_strerror(n));
		return False;
	}

	ressave = res;
	sock = -1;
	while (res)
	{
		sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (!(sock < 0))
		{
			if (connect(sock, res->ai_addr, res->ai_addrlen) == 0)
				break;
			close(sock);
			sock = -1;
		}
		res = res->ai_next;
	}
	freeaddrinfo(ressave);

	if (sock == -1)
	{
		error("%s: unable to connect\n", server);
		return False;
	}

#else /* no IPv6 support */

	struct hostent *nslookup;
	struct sockaddr_in servaddr;

	if ((nslookup = gethostbyname(server)) != NULL)
	{
		memcpy(&servaddr.sin_addr, nslookup->h_addr, sizeof(servaddr.sin_addr));
	}
	else if ((servaddr.sin_addr.s_addr = inet_addr(server)) == INADDR_NONE)
	{
		error("%s: unable to resolve host\n", server);
		return False;
	}

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		error("socket: %s\n", strerror(errno));
		return False;
	}

	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(g_tcp_port_rdp);

	if (connect(sock, (struct sockaddr *) &servaddr, sizeof(struct sockaddr)) < 0)
	{
		error("connect: %s\n", strerror(errno));
		close(sock);
		return False;
	}

#endif /* IPv6 */

	setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (void *) &true_value, sizeof(true_value));

	in.size = 4096;
	in.data = (uint8 *) xmalloc(in.size);

	out.size = 4096;
	out.data = (uint8 *) xmalloc(out.size);

	return True;
}

/* Disconnect on the TCP layer */
void
tcp_disconnect(void)
{
	close(sock);
}

char *
tcp_get_address()
{
	static char ipaddr[32];
	struct sockaddr_in sockaddr;
	socklen_t len = sizeof(sockaddr);
	if (getsockname(sock, (struct sockaddr *) &sockaddr, &len) == 0)
	{
		unsigned char *ip = (unsigned char *) &sockaddr.sin_addr;
		sprintf(ipaddr, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
	}
	else
		strcpy(ipaddr, "127.0.0.1");
	return ipaddr;
}
