#define	MAX_PARALLEL_DEVICES		1

#define FILE_DEVICE_PARALLEL		0x22

#define IOCTL_PAR_QUERY_RAW_DEVICE_ID	0x0c

#include "rdesktop.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>

#if defined(__linux__)
#include <linux/lp.h>
#endif

extern int errno;

extern RDPDR_DEVICE g_rdpdr_device[];

static PARALLEL_DEVICE *
get_parallel_data(HANDLE handle)
{
	int index;

	for (index = 0; index < RDPDR_MAX_DEVICES; index++)
	{
		if (handle == g_rdpdr_device[index].handle)
			return (PARALLEL_DEVICE *) g_rdpdr_device[index].pdevice_data;
	}
	return NULL;
}


/* Enumeration of devices from rdesktop.c        */
/* returns numer of units found and initialized. */
/* optarg looks like ':LPT1=/dev/lp0'            */
/* when it arrives to this function.             */
int
parallel_enum_devices(uint32 * id, char *optarg)
{
	PARALLEL_DEVICE *ppar_info;

	char *pos = optarg;
	char *pos2;
	int count = 0;

	// skip the first colon
	optarg++;
	while ((pos = next_arg(optarg, ',')) && *id < RDPDR_MAX_DEVICES)
	{
		ppar_info = (PARALLEL_DEVICE *) xmalloc(sizeof(PARALLEL_DEVICE));

		pos2 = next_arg(optarg, '=');
		strcpy(g_rdpdr_device[*id].name, optarg);

		toupper_str(g_rdpdr_device[*id].name);

		g_rdpdr_device[*id].local_path = xmalloc(strlen(pos2) + 1);
		strcpy(g_rdpdr_device[*id].local_path, pos2);
		printf("PARALLEL %s to %s\n", optarg, pos2);

		// set device type
		g_rdpdr_device[*id].device_type = DEVICE_TYPE_PARALLEL;
		g_rdpdr_device[*id].pdevice_data = (void *) ppar_info;
		g_rdpdr_device[*id].handle = 0;
		count++;
		(*id)++;

		optarg = pos;
	}
	return count;
}

static NTSTATUS
parallel_create(uint32 device_id, uint32 access, uint32 share_mode, uint32 disposition,
		uint32 flags, char *filename, HANDLE * handle)
{
	int parallel_fd;

	parallel_fd = open(g_rdpdr_device[device_id].local_path, O_RDWR);
	if (parallel_fd == -1)
	{
		perror("open");
		return STATUS_ACCESS_DENIED;
	}

	/* all read and writes should be non blocking */
	if (fcntl(parallel_fd, F_SETFL, O_NONBLOCK) == -1)
		perror("fcntl");

#if defined(LPABORT)
	/* Retry on errors */
	ioctl(parallel_fd, LPABORT, (int) 1);
#endif

	g_rdpdr_device[device_id].handle = parallel_fd;

	*handle = parallel_fd;

	return STATUS_SUCCESS;
}

static NTSTATUS
parallel_close(HANDLE handle)
{
	int i = get_device_index(handle);
	if (i >= 0)
		g_rdpdr_device[i].handle = 0;
	close(handle);
	return STATUS_SUCCESS;
}

static NTSTATUS
parallel_read(HANDLE handle, uint8 * data, uint32 length, uint32 offset, uint32 * result)
{
	*result = read(handle, data, length);
	return STATUS_SUCCESS;
}

static NTSTATUS
parallel_write(HANDLE handle, uint8 * data, uint32 length, uint32 offset, uint32 * result)
{
	int rc = STATUS_SUCCESS;

	int n = write(handle, data, length);
	if (n < 0)
	{
		int status;

		*result = 0;
		switch (errno)
		{
			case EAGAIN:
				rc = STATUS_DEVICE_OFF_LINE;
			case ENOSPC:
				rc = STATUS_DEVICE_PAPER_EMPTY;
			case EIO:
				rc = STATUS_DEVICE_OFF_LINE;
			default:
				rc = STATUS_DEVICE_POWERED_OFF;
		}
#if defined(LPGETSTATUS)
		if (ioctl(handle, LPGETSTATUS, &status) == 0)
		{
			/* coming soon: take care for the printer status */
			printf("parallel_write: status = %d, errno = %d\n", status, errno);
		}
#endif
	}
	*result = n;
	return rc;
}

static NTSTATUS
parallel_device_control(HANDLE handle, uint32 request, STREAM in, STREAM out)
{
	if ((request >> 16) != FILE_DEVICE_PARALLEL)
		return STATUS_INVALID_PARAMETER;

	/* extract operation */
	request >>= 2;
	request &= 0xfff;

	printf("PARALLEL IOCTL %d: ", request);

	switch (request)
	{
		case IOCTL_PAR_QUERY_RAW_DEVICE_ID:

		default:

			printf("\n");
			unimpl("UNKNOWN IOCTL %d\n", request);
	}
	return STATUS_SUCCESS;
}

DEVICE_FNS parallel_fns = {
	parallel_create,
	parallel_close,
	parallel_read,
	parallel_write,
	parallel_device_control
};
