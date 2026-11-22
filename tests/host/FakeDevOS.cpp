/* A stand-in for linux/DtaDevOS.cpp so the whole sedutil-cli builds and runs
 * on a host without a drive (Linux or macOS). It knows three paths:
 *   /dev/fake0  an Opal 2.0 NVMe drive: answers Discovery 0, nothing else
 *   /dev/fake1  a drive without TCG support
 *   /dev/fake9  listed by --scan but does not open (as without root)
 * Any other command fails like a drive that does not answer. */
#include "os.h"
#include <cstring>
#include "DtaDevOS.h"
#include "DtaEndianFixup.h"

using namespace std;

DtaDevOS::DtaDevOS() { drive = NULL; }
DtaDevOS::~DtaDevOS() {}

static void put16(uint8_t *p, uint16_t v) { p[0] = v >> 8; p[1] = v & 0xff; }
static void put32(uint8_t *p, uint32_t v) { put16(p, v >> 16); put16(p + 2, v & 0xffff); }
static void put64(uint8_t *p, uint64_t v) { put32(p, v >> 32); put32(p + 4, v & 0xffffffff); }

/* Feature descriptors as the TCG specs lay them out */
static uint32_t buildDiscovery0(uint8_t *buf)
{
	uint8_t *f = buf + 48;
	// TPer: sync, streaming
	put16(f, 0x0001); f[2] = 0x10; f[3] = 12; f[4] = 0x01 | 0x10; f += 16;
	// Locking: supported, enabled, locked, media encryption, MBR enabled
	put16(f, 0x0002); f[2] = 0x10; f[3] = 12; f[4] = 0x01 | 0x02 | 0x04 | 0x08 | 0x10; f += 16;
	// Geometry: align, 512 byte blocks, granularity 8, lowest aligned LBA 0
	put16(f, 0x0003); f[2] = 0x10; f[3] = 28; f[4] = 0x01;
	put32(f + 12, 512); put64(f + 16, 8); put64(f + 24, 0); f += 32;
	// Opal SSC 2.0: base comID 0x1000, 1 comID, 4 admins, 8 users
	put16(f, 0x0203); f[2] = 0x10; f[3] = 16;
	put16(f + 4, 0x1000); put16(f + 6, 1); f[8] = 0; put16(f + 9, 4); put16(f + 11, 8);
	f[13] = 0; f[14] = 0; f += 20;
	// Block SID Authentication: SID differs from MSID, not blocked, HW reset clears
	put16(f, 0x0402); f[2] = 0x10; f[3] = 12; f[4] = 0x01; f[5] = 0x01; f += 16;
	const uint32_t len = (uint32_t)(f - buf);
	put32(buf, len);
	put32(buf + 4, 1);
	return len;
}

void DtaDevOS::init(const char *devref)
{
	memset(&disk_info, 0, sizeof(OPAL_DiskInfo));
	dev = devref;
	isOpen = FALSE;
	if (strcmp(devref, "/dev/fake0") && strcmp(devref, "/dev/fake1")) {
		LOG(E) << "Cannot open " << devref;
		return;
	}
	isOpen = TRUE;
	identify(disk_info);
	discovery0();
}

uint8_t DtaDevOS::sendCmd(ATACOMMAND cmd, uint8_t protocol, uint16_t comID,
	void *buffer, uint32_t bufferlen)
{
	if (!strcmp(dev, "/dev/fake0") && cmd == IF_RECV && protocol == 0x01 && comID == 0x0001) {
		memset(buffer, 0, bufferlen);
		buildDiscovery0((uint8_t *)buffer);
		return 0;
	}
	return 0xff;
}

void DtaDevOS::osmsSleep(uint32_t ms) { (void)ms; }

void DtaDevOS::identify(OPAL_DiskInfo &di)
{
	di.devType = DEVICE_TYPE_NVME;
	// the serial is the salt of the password hash; same as in test_main.cpp
	memcpy(di.serialNum, "S5GXNF0R123456      ", 20);
	memcpy(di.firmwareRev, "FW1.0   ", 8);
	const char *model = strcmp(dev, "/dev/fake0") ? "FAKE PLAIN SSD" : "FAKE \"OPAL\" SSD";
	memset(di.modelNum, ' ', sizeof(di.modelNum));
	memcpy(di.modelNum, model, strlen(model));
}

unsigned long long DtaDevOS::getSize() { return 0; }

std::vector<std::string> DtaDevOS::deviceNames()
{
	return { "/dev/fake0", "/dev/fake1", "/dev/fake9" };
}

int DtaDevOS::diskScan()
{
	printf("Scanning for Opal compliant disks\n");
	printf("No more disks present ending scan\n");
	return 0;
}
