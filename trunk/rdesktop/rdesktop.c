/*
   rdesktop: A Remote Desktop Protocol client.
   Entrypoint and utility functions
   Copyright (C) Matthew Chapman 1999-2000
   
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

#include <stdlib.h>	/* malloc realloc free */
#include <unistd.h>	/* read close getuid getgid getpid getppid gethostname */
#include <fcntl.h>	/* open */
#include <getopt.h>	/* getopt */
#include <pwd.h>	/* getpwuid */
#include <sys/stat.h>	/* stat */
#include <sys/time.h>	/* gettimeofday */
#include <sys/times.h>	/* times */
#include "rdesktop.h"

char username[16];
char hostname[16];
int width = 800;
int height = 600;
int keylayout = 0x409;
BOOL motion = False;
BOOL orders = True;
BOOL licence = True;

/* Display usage information */
static void usage(char *program)
{
	STATUS("Usage: %s [options] server\n", program);
	STATUS("   -u: user name\n");
	STATUS("   -n: client hostname\n");
	STATUS("   -w: desktop width\n");
	STATUS("   -h: desktop height\n");
	STATUS("   -k: keyboard layout (hex)\n");
	STATUS("   -m: send motion events\n");
	STATUS("   -b: force bitmap updates\n");
	STATUS("   -l: do not request licence\n\n");
}

/* Client program */
int main(int argc, char *argv[])
{
	struct passwd *pw;
	char *server;
	char title[32];
	int c;

	STATUS("rdesktop: A Remote Desktop Protocol client.\n");
	STATUS("Version "VERSION". Copyright (C) 1999-2000 Matt Chapman.\n\n");

	while ((c = getopt(argc, argv, "u:n:w:h:k:mbl?")) != -1)
	{
		switch (c)
		{
			case 'u':
				strncpy(username, optarg, sizeof(username));
				break;

			case 'n':
				strncpy(hostname, optarg, sizeof(hostname));
				break;

			case 'w':
				width = strtol(optarg, NULL, 10);
				break;

			case 'h':
				height = strtol(optarg, NULL, 10);
				break;

			case 'k':
				keylayout = strtol(optarg, NULL, 16);
				break;

			case 'm':
				motion = True;
				break;

			case 'b':
				orders = False;
				break;

			case 'l':
				licence = False;
				break;

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
			STATUS("Could not determine user name.\n");
			return 1;
		}

		strncpy(username, pw->pw_name, sizeof(username));
	}

	if (hostname[0] == 0)
	{
		if (gethostname(hostname, sizeof(hostname)) == -1)
		{
			STATUS("Could not determine host name.\n");
			return 1;
		}
	}

	if (!rdp_connect(server))
		return 1;

	STATUS("Connection successful.\n");

	snprintf(title, sizeof(title), "rdesktop - %s", server);
	if (ui_create_window(title))
	{
		rdp_main_loop();
		ui_destroy_window();
	}

	rdp_disconnect();
	return 0;
}

/* Generate a 32-byte random for the secure transport code. */
void generate_random(uint8 *random)
{
	struct stat st;
	uint32 *r = (uint32 *)random;
	int fd;

	/* If we have a kernel random device, use it. */
	if ((fd = open("/dev/urandom", O_RDONLY)) != -1)
	{
		read(fd, random, 32);
		close(fd);
		return;
	}

	/* Otherwise use whatever entropy we can gather - ideas welcome. */
	r[0] = (getpid()) | (getppid() << 16);
	r[1] = (getuid()) | (getgid() << 16);
	r[2] = times(NULL); /* system uptime (clocks) */
	gettimeofday((struct timeval *)&r[3], NULL); /* sec and usec */
	stat("/tmp", &st);
	r[5] = st.st_atime;
	r[6] = st.st_mtime;
	r[7] = st.st_ctime;
}

/* malloc; exit if out of memory */
void *xmalloc(int size)
{
	void *mem = malloc(size);
	if (mem == NULL)
	{
		ERROR("xmalloc %d\n", size);
		exit(1);
	}
	return mem;
}

/* realloc; exit if out of memory */
void *xrealloc(void *oldmem, int size)
{
	void *mem = realloc(oldmem, size);
	if (mem == NULL)
	{
		ERROR("xrealloc %d\n", size);
		exit(1);
	}
	return mem;
}

/* free */
void xfree(void *mem)
{
	free(mem);
}

/* Produce a hex dump */
void hexdump(unsigned char *p, unsigned int len)
{
	unsigned char *line = p;
	unsigned int thisline, offset = 0;
	int i;

	while (offset < len)
	{
		STATUS("%04x ", offset);
		thisline = len - offset;
		if (thisline > 16)
			thisline = 16;

		for (i = 0; i < thisline; i++)
			STATUS("%02x ", line[i])

		for (; i < 16; i++)
			STATUS("   ");

		for (i = 0; i < thisline; i++)
			STATUS("%c", (line[i] >= 0x20 && line[i] < 0x7f) ? line[i] : '.');

		STATUS("\n");
		offset += thisline;
		line += thisline;
	}
}
