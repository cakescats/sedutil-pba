/* C:B**************************************************************************
This software is Copyright 2014-2017 Bright Plaza Inc. <drivetrust@drivetrust.com>

This file is part of sedutil.

sedutil is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

sedutil is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with sedutil.  If not, see <http://www.gnu.org/licenses/>.

* C:E********************************************************************** */

#include "os.h"
#include <stdio.h>
#include <iostream>
#pragma warning(push)
#pragma warning(disable : 4091)
#include <Ntddscsi.h>
#pragma warning(pop)
#include <vector>
#include "DtaDiskNVMe.h"
#include "DtaEndianFixup.h"
#include "DtaStructures.h"
#include "DtaHexDump.h"

using namespace std;
DtaDiskNVMe::DtaDiskNVMe() {};
void DtaDiskNVMe::init(const char * devref)
{
    LOG(D1) << "Creating DtaDiskNVMe::DtaDiskNVMe() " << devref;
    
     hDev = CreateFile(devref,
                      GENERIC_WRITE | GENERIC_READ | GENERIC_EXECUTE,
                      FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE,
                      NULL,
                      OPEN_EXISTING,
                      0,
                      NULL);
	 if (INVALID_HANDLE_VALUE == hDev) {
		 LOG(E) << "Unable to open the device " << devref;
		 return;
	 }
    else 
        isOpen = TRUE;
}
uint8_t DtaDiskNVMe::sendCmd(ATACOMMAND cmd, uint8_t protocol, uint16_t comID,
	void * buffer, uint32_t bufferlen)
{
	LOG(D1) << "Entering DtaDiskNVMe::sendCmd";
	DWORD bytesReturned = 0; // data returned
	if (!isOpen) {
		LOG(E) << "Device open failed";
		return DTAERROR_OPEN_ERR; //disk open failed so this will too
	}
	SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER sptdwb;

	memset(&sptdwb, 0, sizeof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER));
	sptdwb.sptd.Length = sizeof(SCSI_PASS_THROUGH_DIRECT);
	sptdwb.sptd.PathId = 0;
	sptdwb.sptd.TargetId = 1;
	sptdwb.sptd.Lun = 0;
	sptdwb.sptd.SenseInfoLength = 32;
	
	sptdwb.sptd.DataTransferLength = bufferlen;
	sptdwb.sptd.TimeOutValue = 2;
	sptdwb.sptd.DataBuffer = buffer;
	sptdwb.sptd.SenseInfoOffset =
		offsetof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER, ucSenseBuf);
	switch (cmd) {
	case IF_RECV:
	/* Security Protocol IN */
		sptdwb.sptd.Cdb[0] = 0xa2;
		sptdwb.sptd.Cdb[1] = protocol;
		sptdwb.sptd.Cdb[2] = ((comID & 0xff00) >> 8);
		sptdwb.sptd.Cdb[3] = (comID & 0x00ff);
		sptdwb.sptd.Cdb[6] = (UCHAR)((bufferlen & 0xff000000) >> 24);
		sptdwb.sptd.Cdb[7] = (UCHAR)((bufferlen & 0x00ff0000) >> 16);
		sptdwb.sptd.Cdb[8] = (UCHAR)((bufferlen & 0x0000ff00) >> 8);
		sptdwb.sptd.Cdb[9] = (UCHAR)((bufferlen & 0x000000ff));
		sptdwb.sptd.CdbLength = 12;
		sptdwb.sptd.DataIn = SCSI_IOCTL_DATA_IN;
		break;
	case IF_SEND:
		/* Security Protocol OUT */
		sptdwb.sptd.Cdb[0] = 0xb5;
		sptdwb.sptd.Cdb[1] = protocol;
		sptdwb.sptd.Cdb[2] = ((comID & 0xff00) >> 8);
		sptdwb.sptd.Cdb[3] = (comID & 0x00ff);
		sptdwb.sptd.Cdb[6] = (UCHAR)((bufferlen & 0xff000000) >> 24);
		sptdwb.sptd.Cdb[7] = (UCHAR)((bufferlen & 0x00ff0000) >> 16);
		sptdwb.sptd.Cdb[8] = (UCHAR)((bufferlen & 0x0000ff00) >> 8);
		sptdwb.sptd.Cdb[9] = (UCHAR)((bufferlen & 0x000000ff));
		sptdwb.sptd.CdbLength = 12;
		sptdwb.sptd.DataIn = SCSI_IOCTL_DATA_OUT;
		break;
	case IDENTIFY:
		sptdwb.sptd.Cdb[0] = 0x12;
		sptdwb.sptd.Cdb[4] = 0x60;
		sptdwb.sptd.CdbLength = 6;
		sptdwb.sptd.DataIn = SCSI_IOCTL_DATA_IN;
		break;
	default:
		LOG(E) << "Invalid IO Command";
		return 1;
	}
	
	BOOL iorc = DeviceIoControl(hDev,
		IOCTL_SCSI_PASS_THROUGH_DIRECT,
		&sptdwb,
		sizeof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER),
		&sptdwb,
		sizeof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER),
		&bytesReturned,
		FALSE);

	DWORD lasterror = GetLastError();
	if ((sptdwb.sptd.ScsiStatus == 0) && (iorc != 0)) {
		return 0;
	}
	LOG(D1) << "Error: GetLastError() = " << lasterror << " ScsiStatus = " << (uint16_t)sptdwb.sptd.ScsiStatus << " iorc = " << iorc;
	return 1;
}

/* NVMe Identify Controller through the storage protocol query of Windows 10
 * (stornvme). The SCSI translation that sendCmd goes through gives a 16 byte
 * model and no serial number at all, and the serial number is the salt of the
 * password hash (DtaHashPwd): it has to be the bytes Linux and the PBA read,
 * or every password set elsewhere is refused as NOT_AUTHORIZED. */
namespace {
/* winioctl.h layouts (Windows 10), spelled out so the SDK version does not matter */
struct ProtocolSpecificData {
	DWORD ProtocolType;
	DWORD DataType;
	DWORD ProtocolDataRequestValue;
	DWORD ProtocolDataRequestSubValue;
	DWORD ProtocolDataOffset;
	DWORD ProtocolDataLength;
	DWORD FixedProtocolReturnData;
	DWORD ProtocolDataRequestSubValue2;
	DWORD ProtocolDataRequestSubValue3;
	DWORD ProtocolDataRequestSubValue4;
};
/* STORAGE_PROPERTY_QUERY with the data as AdditionalParameters */
struct ProtocolQuery {
	DWORD PropertyId;
	DWORD QueryType;
	ProtocolSpecificData data;
};
/* STORAGE_PROTOCOL_DATA_DESCRIPTOR */
struct ProtocolDescriptor {
	DWORD Version;
	DWORD Size;
	ProtocolSpecificData data;
};
const DWORD StorageAdapterProtocolSpecific = 49;
const DWORD StorageDeviceProtocolSpecific = 50;
const DWORD ProtocolNvme = 3;
const DWORD NVMeIdentify = 1;
const DWORD CnsController = 1;
const DWORD IdentifySize = 4096;
/* the fields DtaDevLinuxNvme::identify copies end here (firmware at 64..71) */
const DWORD IdentifyUsed = 72;
}

static bool nvmeIdentifyController(HANDLE hDev, vector<uint8_t> &ctrl)
{
	const DWORD properties[] = { StorageAdapterProtocolSpecific, StorageDeviceProtocolSpecific };
	for (DWORD property : properties) {
		vector<uint8_t> buf(sizeof(ProtocolQuery) + IdentifySize, 0);
		ProtocolQuery *q = (ProtocolQuery *)buf.data();
		q->PropertyId = property;
		q->QueryType = 0; // PropertyStandardQuery
		q->data.ProtocolType = ProtocolNvme;
		q->data.DataType = NVMeIdentify;
		q->data.ProtocolDataRequestValue = CnsController;
		q->data.ProtocolDataOffset = sizeof(ProtocolSpecificData);
		q->data.ProtocolDataLength = IdentifySize;
		DWORD returned = 0;
		if (!DeviceIoControl(hDev, IOCTL_STORAGE_QUERY_PROPERTY, buf.data(), (DWORD)buf.size(),
				buf.data(), (DWORD)buf.size(), &returned, NULL)) {
			LOG(D1) << "NVMe Identify through property " << property << " failed, error " << GetLastError();
			continue;
		}
		const ProtocolDescriptor *d = (const ProtocolDescriptor *)buf.data();
		const size_t start = sizeof(d->Version) + sizeof(d->Size) + d->data.ProtocolDataOffset;
		if ((d->Version != sizeof(ProtocolDescriptor)) || (d->Size != sizeof(ProtocolDescriptor)) ||
				(d->data.ProtocolDataOffset < sizeof(ProtocolSpecificData)) ||
				(d->data.ProtocolDataLength < IdentifyUsed) || (start + IdentifyUsed > buf.size())) {
			LOG(D1) << "NVMe Identify through property " << property << ": unexpected answer";
			continue;
		}
		ctrl.assign(buf.begin() + start, buf.begin() + start + IdentifyUsed);
		return true;
	}
	return false;
}

/** adds the IDENTIFY information to the disk_info structure */

void DtaDiskNVMe::identify(OPAL_DiskInfo& disk_info)
{
	LOG(D1) << "Entering DtaDiskNVMe::identify()";
	vector<uint8_t> nullz(512, 0x00);
    void * identifyResp = NULL;
	identifyResp = _aligned_malloc(MIN_BUFFER_LENGTH, IO_BUFFER_ALIGNMENT);
    if (NULL == identifyResp) return;
    memset(identifyResp, 0, MIN_BUFFER_LENGTH);
    uint8_t iorc = sendCmd(IDENTIFY, 0x00, 0x0000, identifyResp, MIN_BUFFER_LENGTH);
    // TODO: figure out why iorc = 4
    if ((0x00 != iorc) && (0x04 != iorc)) {
		printf("%s %d \n", "IDENTIFY Failed ", (uint16_t) iorc);
       _aligned_free(identifyResp);
        return;
    }
	if (!(memcmp(identifyResp, nullz.data(), 512))) {
		disk_info.devType = DEVICE_TYPE_OTHER;
		return;
	}
	SCSI_INQUIRY_RESPONSE * id = (SCSI_INQUIRY_RESPONSE *)identifyResp;
	memcpy(disk_info.firmwareRev, id->ProductRev, sizeof(id->ProductRev));
	memcpy(disk_info.modelNum, id->ProductID, sizeof(id->ProductID));
	_aligned_free(identifyResp);
	vector<uint8_t> ctrl;
	if (nvmeIdentifyController(hDev, ctrl)) {
		// the same bytes as linux/DtaDevLinuxNvme.cpp: serial 4..23, model 24..63, firmware 64..71
		memcpy(disk_info.serialNum, ctrl.data() + 4, sizeof(disk_info.serialNum));
		memcpy(disk_info.modelNum, ctrl.data() + 24, sizeof(disk_info.modelNum));
		memcpy(disk_info.firmwareRev, ctrl.data() + 64, sizeof(disk_info.firmwareRev));
	}
	else {
		LOG(W) << "Cannot read the NVMe serial number: passwords set on another system or in the PBA will not match";
	}
	return;
}

/** Close the filehandle so this object can be delete. */
DtaDiskNVMe::~DtaDiskNVMe()
{
	LOG(D1) << "Destroying DtaDiskNVMe";
    CloseHandle(hDev);
}
