/*
   rdesktop: A Remote Desktop Protocol client.
   Entrypoint and utility functions
   Copyright (C) Matthew Chapman 1999-2001
   
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

#include <stdlib.h>		/* malloc realloc free */
#include <stdarg.h>		/* va_list va_start va_end */
#include <unistd.h>		/* read close getuid getgid getpid getppid gethostname */
#include <fcntl.h>		/* open */
#include <pwd.h>		/* getpwuid */
#include <sys/stat.h>		/* stat */
#include <sys/time.h>		/* gettimeofday */
#include <sys/times.h>		/* times */
#include "rdesktop.h"

char username[16];
char hostname[16];
char keymapname[16];
int keylayout;
int width;
int height;
BOOL bitmap_compression = True;
BOOL sendmotion = True;
BOOL orders = True;
BOOL licence = True;
BOOL encryption = True;
BOOL desktop_save = True;
BOOL fullscreen = False;

/* Display usage information */
static void
usage(char *program)
{
	printf("Usage: %s [options] server\n", program);
	printf("   -u: user name\n");
	printf("   -d: domain\n");
	printf("   -s: shell\n");
	printf("   -c: working directory\n");
	printf("   -p: password (autologon)\n");
	printf("   -n: client hostname\n");
	printf("   -k: keyboard layout\n");
	printf("   -g: desktop geometry (WxH)\n");
	printf("   -f: full-screen mode\n");
	printf("   -b: force bitmap updates\n");
	printf("   -e: disable encryption (French TS)\n");
	printf("   -m: do not send motion events\n");
	printf("   -l: do not request licence\n\n");
}

/* Client program */
int
main(int argc, char *argv[])
{
	char fullhostname[64];
	char domain[16];
	char password[16];
	char shell[32];
	char directory[32];
	char title[32];
	struct passwd *pw;
	char *server, *p;
	uint32 flags;
	int c;

	printf("rdesktop: A Remote Desktop Protocol client.\n");
	printf("Version " VERSION ". Copyright (C) 1999-2001 Matt Chapman.\n");
	printf("See http://www.rdesktop.org/ for more information.\n\n");

	flags = RDP_LOGON_NORMAL;
	domain[0] = password[0] = shell[0] = directory[0] = 0;
	strcpy(keymapname, "us");

	while ((c = getopt(argc, argv, "u:d:s:c:p:n:k:g:fbemlh?")) != -1)
	{
		switch (c)
		{
			case 'u':
				STRNCPY(username, optarg, sizeof(username));
				break;

			case 'd':
				STRNCPY(domain, optarg, sizeof(domain));
				break;

			case 's':
				STRNCPY(shell, optarg, sizeof(shell));
				break;

			case 'c':
				STRNCPY(directory, optarg, sizeof(directory));
				break;

			case 'p':
				STRNCPY(password, optarg, sizeof(password));
				flags |= RDP_LOGON_AUTO;
				break;

			case 'n':
				STRNCPY(hostname, optarg, sizeof(hostname));
				break;

			case 'k':
				STRNCPY(keymapname, optarg, sizeof(keymapname));
				break;

			case 'g':
				width = strtol(optarg, &p, 10);
				if (*p == 'x')
					height = strtol(p+1, NULL, 10);

				if ((width == 0) || (height == 0))
				{
					error("invalid geometry\n");
					return 1;
				}
				break;

			case 'f':
				fullscreen = True;
				break;

			case 'b':
				orders = False;
				break;

			case 'e':
				encryption = False;
				break;

			case 'm':
				sendmotion = False;
				break;

			case 'l':
				licence = False;
				break;

			case 'h':
			case '?':
			default:
				usage(argv[0]);
				return 1;
		}
	}

	if (argc - optind < 1)
	{
		usage(argv[0]);
		return 1;
	}

	server = argv[optind];

	if (username[0] == 0)
	{
		pw = getpwuid(getuid());
		if ((pw == NULL) || (pw->pw_name == NULL))
		{
			error("could not determine username, use -u\n");
			return 1;
		}

		STRNCPY(username, pw->pw_name, sizeof(username));
	}

	if (hostname[0] == 0)
	{
		if (gethostname(fullhostname, sizeof(fullhostname)) == -1)
		{
			error("could not determine local hostname, use -n\n");
			return 1;
		}

		p = strchr(fullhostname, '.');
		if (p != NULL)
			*p = 0;

		STRNCPY(hostname, fullhostname, sizeof(hostname));
	}

	if (!strcmp(password, "-"))
	{
		p = getpass("Password: ");
		if (p == NULL)
		{
			error("failed to read password\n");
			return 0;
		}
		STRNCPY(password, p, sizeof(password));
	}

	if ((width == 0) || (height == 0))
	{
		width = 800;
		height = 600;
	}

	strcpy(title, "rdesktop - ");
	strncat(title, server, sizeof(title) - sizeof("rdesktop - "));

	if (ui_create_window(title))
	{
		if (!rdp_connect(server, flags, domain, password, shell,
				 directory))
			return 1;

		printf("Connection successful.\n");
		rdp_main_loop();
		printf("Disconnecting...\n");
		ui_destroy_window();
		rdp_disconnect();
	}

	return 0;
}

/* Generate a 32-byte random for the secure transport code. */
void
generate_random(uint8 *random)
{
	struct stat st;
	struct tms tmsbuf;
	uint32 *r = (uint32 *) random;
	int fd;

	/* If we have a kernel random device, use it. */
	if (((fd = open("/dev/urandom", O_RDONLY)) != -1)
	    || ((fd = open("/dev/random", O_RDONLY)) != -1))
	{
		read(fd, random, 32);
		close(fd);
		return;
	}

	/* Otherwise use whatever entropy we can gather - ideas welcome. */
	r[0] = (getpid()) | (getppid() << 16);
	r[1] = (getuid()) | (getgid() << 16);
	r[2] = times(&tmsbuf);	/* system uptime (clocks) */
	gettimeofday((struct timeval *) &r[3], NULL);	/* sec and usec */
	stat("/tmp", &st);
	r[5] = st.st_atime;
	r[6] = st.st_mtime;
	r[7] = st.st_ctime;
}

/* malloc; exit if out of memory */
void *
xmalloc(int size)
{
	void *mem = malloc(size);
	if (mem == NULL)
	{
		error("xmalloc %d\n", size);
		exit(1);
	}
	return mem;
}

/* realloc; exit if out of memory */
void *
xrealloc(void *oldmem, int size)
{
	void *mem = realloc(oldmem, size);
	if (mem == NULL)
	{
		error("xrealloc %d\n", size);
		exit(1);
	}
	return mem;
}

/* free */
void
xfree(void *mem)
{
	free(mem);
}

/* report an error */
void
error(char *format, ...)
{
	va_list ap;

	fprintf(stderr, "ERROR: ");

	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);
}

/* report an unimplemented protocol feature */
void
unimpl(char *format, ...)
{
	va_list ap;

	fprintf(stderr, "NOT IMPLEMENTED: ");

	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);
}

/* produce a hex dump */
void
hexdump(unsigned char *p, unsigned int len)
{
	unsigned char *line = p;
	unsigned int thisline, offset = 0;
	int i;

	while (offset < len)
	{
		printf("%04x ", offset);
		thisline = len - offset;
		if (thisline > 16)
			thisline = 16;

		for (i = 0; i < thisline; i++)
			printf("%02x ", line[i]);

		for (; i < 16; i++)
				printf("   ");

		for (i = 0; i < thisline; i++)
			printf("%c",
			       (line[i] >= 0x20
				&& line[i] < 0x7f) ? line[i] : '.');

		printf("\n");
		offset += thisline;
		line += thisline;
	}
}
