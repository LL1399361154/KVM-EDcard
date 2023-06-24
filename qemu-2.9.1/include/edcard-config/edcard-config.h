#ifndef CONFIGURE_H
#define CONFIGURE_H
 
#define UNIX_DOMAIN "/tmp/UNIX.domain"
#define DATALEN 1024

#define MAX_DICTIONARY_SIZE   1024

typedef struct LocalDevice
{
	char* pci_slot;
	char* device;
	struct LocalDevice* next_device;

}LocalDevice;

typedef struct{

}RemoteDevice;

extern char vm_id[37];//save the virtual machine's uuid
extern char* edcard_name[6];
extern int hDevice;

#endif
