#define	MAX_PRINTERS	5

#include "rdesktop.h"

extern RDPDR_DEVICE g_rdpdr_device[];

PRINTER * get_printer_data(HANDLE handle)
{
	int index;

	for (index = 0; index < RDPDR_MAX_DEVICES; index++)
	{
		if (handle == g_rdpdr_device[index].handle)
			return (PRINTER *) g_rdpdr_device[index].pdevice_data;
	}
	return NULL;
}

int
printer_enum_devices(int *id, char *optarg)
{
	PRINTER *pprinter_data;

	char *pos = optarg;
	char *pos2;
	int count = 0;
	int already = 0;

	// we need to know how many printers we've already set up
	// supplied from other -r flags than this one.
	while (count < *id)
	{
		if (g_rdpdr_device[count].device_type == DEVICE_TYPE_PRINTER)
			already++;
		count++;
	}

	count = 0;

	if (*optarg == ':')
		optarg++;

	while ((pos = next_arg(optarg, ',')) && *id < RDPDR_MAX_DEVICES)
	{
		pprinter_data = (PRINTER *) xmalloc(sizeof(PRINTER));

		strcpy(g_rdpdr_device[*id].name, "PRN");
		strcat(g_rdpdr_device[*id].name, ltoa(already + count + 1, 10));

		/* first printer is set as default printer */
		if ((already + count) == 0)
			pprinter_data->default_printer = True;
		else
			pprinter_data->default_printer = False;

		pos2 = next_arg(optarg, ':');
		if (*optarg == (char) 0x00)
			pprinter_data->printer = "mydeskjet";	/* set default */
		else
		{
			pprinter_data->printer = xmalloc(strlen(optarg) + 1);
			strcpy(pprinter_data->printer, optarg);
		}

		if (!pos2 || (*pos2 == (char) 0x00))
			pprinter_data->driver = "HP LaserJet IIIP";	/* no printer driver supplied set default */
		else
		{
			pprinter_data->driver = xmalloc(strlen(pos2) + 1);
			strcpy(pprinter_data->driver, pos2);
		}

		printf("PRINTER %s to %s driver %s\n", g_rdpdr_device[*id].name,
		       pprinter_data->printer, pprinter_data->driver);
		g_rdpdr_device[*id].device_type = DEVICE_TYPE_PRINTER;
		g_rdpdr_device[*id].pdevice_data = (void *) pprinter_data;
		count++;
		(*id)++;

		optarg = pos;
	}
	return count;
}

static NTSTATUS
printer_create(uint32 device_id, uint32 access, uint32 share_mode, uint32 disposition, uint32 flags,
	       char *filename, HANDLE * handle)
{
	char cmd[256];
	PRINTER *pprinter_data;

	pprinter_data = (PRINTER *) g_rdpdr_device[device_id].pdevice_data;

	/* default printer name use default printer queue as well in unix */
	if (pprinter_data->printer == "mydeskjet")
	{
		pprinter_data->printer_fp = popen("lpr", "w");
	}
	else
	{
		sprintf(cmd, "lpr -P %s", pprinter_data->printer);
		pprinter_data->printer_fp = popen(cmd, "w");
	}

	g_rdpdr_device[device_id].handle = pprinter_data->printer_fp->_fileno;
	*handle = g_rdpdr_device[device_id].handle;
	return STATUS_SUCCESS;
}

static NTSTATUS
printer_close(HANDLE handle)
{
	PRINTER *pprinter_data;

	pprinter_data = get_printer_data(handle);

	g_rdpdr_device[get_device_index(handle)].handle = 0;

	pclose(pprinter_data->printer_fp);

	return STATUS_SUCCESS;
}

static NTSTATUS
printer_write(HANDLE handle, uint8 * data, uint32 length, uint32 offset, uint32 * result)
{
	PRINTER *pprinter_data;

	pprinter_data = get_printer_data(handle);
	*result = length * fwrite(data, length, 1, pprinter_data->printer_fp);

	if (ferror(pprinter_data->printer_fp))
	{
		*result = 0;
		return STATUS_INVALID_HANDLE;
	}
	return STATUS_SUCCESS;
}

DEVICE_FNS printer_fns = {
	printer_create,
	printer_close,
	NULL,			/* read */
	printer_write,
	NULL			/* device_control */
};
