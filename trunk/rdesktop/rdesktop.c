/*
   rdesktop: A Remote Desktop Protocol client.
   Entrypoint and utility functions
   Copyright (C) Matthew Chapman 1999-2002

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

#include <stdarg.h>		/* va_list va_start va_end */
#include <unistd.h>		/* read close getuid getgid getpid getppid gethostname */
#include <fcntl.h>		/* open */
#include <pwd.h>		/* getpwuid */
#include <limits.h>		/* PATH_MAX */
#include <termios.h>		/* tcgetattr tcsetattr */
#include <sys/stat.h>		/* stat */
#include <sys/time.h>		/* gettimeofday */
#include <sys/times.h>		/* times */
#include "rdesktop.h"

#ifdef EGD_SOCKET
#include <sys/socket.h>		/* socket connect */
#include <sys/un.h>		/* sockaddr_un */
#endif

#ifdef WITH_OPENSSL
#include <openssl/md5.h>
#else
#include "crypto/md5.h"
#endif

char title[32] = "";
char username[16];
char hostname[16];
char keymapname[16];
int keylayout = 0x409;		/* Defaults to US keyboard layout */
int width = 800;
int height = 600;
int tcp_port_rdp = TCP_PORT_RDP;
BOOL bitmap_compression = True;
BOOL sendmotion = True;
BOOL orders = True;
BOOL encryption = True;
BOOL desktop_save = True;
BOOL fullscreen = False;
BOOL grab_keyboard = True;

/* Display usage information */
static void
usage(char *program)
{
	fprintf(stderr, "rdesktop: A Remote Desktop Protocol client.\n");
	fprintf(stderr, "Version " VERSION ". Copyright (C) 1999-2002 Matt Chapman.\n");
	fprintf(stderr, "See http://www.rdesktop.org/ for more information.\n\n");

	fprintf(stderr, "Usage: %s [options] server\n", program);
	fprintf(stderr, "   -u: user name\n");
	fprintf(stderr, "   -d: domain\n");
	fprintf(stderr, "   -s: shell\n");
	fprintf(stderr, "   -c: working directory\n");
	fprintf(stderr, "   -p: password (- to prompt)\n");
	fprintf(stderr, "   -n: client hostname\n");
	fprintf(stderr, "   -k: keyboard layout on terminal server (us,sv,gr etc.)\n");
	fprintf(stderr, "   -g: desktop geometry (WxH)\n");
	fprintf(stderr, "   -f: full-screen mode\n");
	fprintf(stderr, "   -b: force bitmap updates\n");
	fprintf(stderr, "   -e: disable encryption (French TS)\n");
	fprintf(stderr, "   -m: do not send motion events\n");
	fprintf(stderr, "   -t: rdp tcp port\n");
	fprintf(stderr, "   -K: keep window manager key bindings\n");
	fprintf(stderr, "   -w: window title\n");
}

static BOOL
read_password(char *password, int size)
{
	struct termios tios;
	BOOL ret = False;
	int istty = 0;
	char *p;

	if (tcgetattr(STDIN_FILENO, &tios) == 0)
	{
		fprintf(stderr, "Password: ");
		tios.c_lflag &= ~ECHO;
		tcsetattr(STDIN_FILENO, TCSANOW, &tios);
		istty = 1;
	}

	if (fgets(password, size, stdin) != NULL)
	{
		ret = True;

		/* strip final newline */
		p = strchr(password, '\n');
		if (p != NULL)
			*p = 0;
	}

	if (istty)
	{
		tios.c_lflag |= ECHO;
		tcsetattr(STDIN_FILENO, TCSANOW, &tios);
		fprintf(stderr, "\n");
	}

	return ret;
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
	BOOL prompt_password;
	struct passwd *pw;
	char *server, *p;
	uint32 flags;
	int c;

	flags = RDP_LOGON_NORMAL;
	prompt_password = False;
	domain[0] = password[0] = shell[0] = directory[0] = 0;
	strcpy(keymapname, "us");

	while ((c = getopt(argc, argv, "u:d:s:c:p:n:k:g:t:fbemKw:h?")) != -1)
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
				if ((optarg[0] == '-') && (optarg[1] == 0))
				{
					prompt_password = True;
					break;
				}

				STRNCPY(password, optarg, sizeof(password));
				flags |= RDP_LOGON_AUTO;

				/* try to overwrite argument so it won't appear in ps */
				p = optarg;
				while (*p)
					*(p++) = 'X';
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
					height = strtol(p + 1, NULL, 10);

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

			case 't':
				tcp_port_rdp = strtol(optarg, NULL, 10);
				break;

			case 'K':
				grab_keyboard = False;
				break;

			case 'w':
				strncpy(title, optarg, sizeof(title));
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

	if (prompt_password && read_password(password, sizeof(password)))
		flags |= RDP_LOGON_AUTO;

	if (title[0] == 0)
	{
		strcpy(title, "rdesktop - ");
		strncat(title, server, sizeof(title) - sizeof("rdesktop - "));
	}

	if (!ui_init())
		return 1;

	if (!rdp_connect(server, flags, domain, password, shell, directory))
		return 1;

	DEBUG(("Connection successful.\n"));
	memset(password, 0, sizeof(password));

	if (ui_create_window())
	{
		rdp_main_loop();
		ui_destroy_window();
	}

	DEBUG(("Disconnecting...\n"));
	rdp_disconnect();
	ui_deinit();
	return 0;
}

#ifdef EGD_SOCKET
/* Read 32 random bytes from PRNGD or EGD socket (based on OpenSSL RAND_egd) */
static BOOL
generate_random_egd(uint8 * buf)
{
	struct sockaddr_un addr;
	BOOL ret = False;
	int fd;

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd == -1)
		return False;

	addr.sun_family = AF_UNIX;
	memcpy(addr.sun_path, EGD_SOCKET, sizeof(EGD_SOCKET));
	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
		goto err;

	/* PRNGD and EGD use a simple communications protocol */
	buf[0] = 1;  /* Non-blocking (similar to /dev/urandom) */
	buf[1] = 32; /* Number of requested random bytes */
	if (write(fd, buf, 2) != 2)
		goto err;

	if ((read(fd, buf, 1) != 1) || (buf[0] == 0)) /* Available? */
		goto err;

	if (read(fd, buf, 32) != 32)
		goto err;

	ret = True;

err:
	close(fd);
	return ret;
}
#endif

/* Generate a 32-byte random for the secure transport code. */
void
generate_random(uint8 * random)
{
	struct stat st;
	struct tms tmsbuf;
	MD5_CTX md5;
	uint32 *r;
	int fd, n;

	/* If we have a kernel random device, try that first */
	if (((fd = open("/dev/urandom", O_RDONLY)) != -1)
	    || ((fd = open("/dev/random", O_RDONLY)) != -1))
	{
		n = read(fd, random, 32);
		close(fd);
		if (n == 32)
			return;
	}

#ifdef EGD_SOCKET
	/* As a second preference use an EGD */
	if (generate_random_egd(random))
		return;
#endif

	/* Otherwise use whatever entropy we can gather - ideas welcome. */
	r = (uint32 *)random;
	r[0] = (getpid()) | (getppid() << 16);
	r[1] = (getuid()) | (getgid() << 16);
	r[2] = times(&tmsbuf);	/* system uptime (clocks) */
	gettimeofday((struct timeval *) &r[3], NULL);	/* sec and usec */
	stat("/tmp", &st);
	r[5] = st.st_atime;
	r[6] = st.st_mtime;
	r[7] = st.st_ctime;

	/* Hash both halves with MD5 to obscure possible patterns */
	MD5_Init(&md5);
	MD5_Update(&md5, random, 16); 
	MD5_Final(random, &md5);
	MD5_Update(&md5, random+16, 16);
	MD5_Final(random+16, &md5);
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
			printf("%c", (line[i] >= 0x20 && line[i] < 0x7f) ? line[i] : '.');

		printf("\n");
		offset += thisline;
		line += thisline;
	}
}

#ifdef SAVE_LICENCE
int
load_licence(unsigned char **data)
{
	char path[PATH_MAX];
	char *home;
	struct stat st;
	int fd;

	home = getenv("HOME");
	if (home == NULL)
		return -1;

	STRNCPY(path, home, sizeof(path));
	strncat(path, "/.rdesktop/licence", sizeof(path) - strlen(path) - 1);

	fd = open(path, O_RDONLY);
	if (fd == -1)
		return -1;

	if (fstat(fd, &st))
		return -1;

	*data = xmalloc(st.st_size);
	return read(fd, *data, st.st_size);
}

void
save_licence(unsigned char *data, int length)
{
	char path[PATH_MAX];
	char *home;
	int fd;

	home = getenv("HOME");
	if (home == NULL)
		return;

	STRNCPY(path, home, sizeof(path));
	strncat(path, "/.rdesktop", sizeof(path) - strlen(path) - 1);
	mkdir(path, 0700);

	strncat(path, "/licence", sizeof(path) - strlen(path) - 1);

	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
	if (fd == -1)
	{
		perror("open");
		return;
	}

	write(fd, data, length);
	close(fd);
}
#endif
