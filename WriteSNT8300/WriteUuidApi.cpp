#include "stdafx.h"
#include "WriteUuidApi.h"
#include <tchar.h>
#include <windows.h>
#include <winioctl.h>
#include <ntddscsi.h>
#include <strsafe.h>

typedef struct {
    char  cSNSignature[8];
    WORD  wSNLen;
    char  bSNData[502];
} CMD_UPDATE_SN;

typedef struct {
    ULONG   Version;
    ULONG   Size;
    UCHAR   DeviceType;
    UCHAR   DeviceTypeModifier;
    BOOLEAN RemovableMedia;
    BOOLEAN CommandQueueing;
    ULONG   VendorIdOffset;
    ULONG   ProductIdOffset;
    ULONG   ProductRevisionOffset;
    ULONG   SerialNumberOffset;
    STORAGE_BUS_TYPE BusType;
    ULONG   RawPropertiesLength;
    UCHAR   RawDeviceProperties[1];
} STORAGE_DEVICE_DESCRIPTOR;

typedef enum _STORAGE_PROPERTY_ID {
    StorageDeviceProperty   = 0,
    StorageAdapterProperty,
    StorageDeviceIdProperty,
    StorageDeviceUniqueIdProperty,
    StorageDeviceWriteCacheProperty,
    StorageMiniportProperty,
    StorageAccessAlignmentProperty,
    StorageDeviceSeekPenaltyProperty,
    StorageDeviceTrimProperty,
    StorageDeviceWriteAggregationProperty,
    StorageDeviceDeviceTelemetryProperty,
    StorageDeviceLBProvisioningProperty,
    StorageDevicePowerProperty,
    StorageDeviceCopyOffloadProperty,
    StorageDeviceResiliencyProperty,
    StorageDeviceMediumProductType,
    StorageDeviceIoCapabilityProperty   = 48,
    StorageAdapterProtocolSpecificProperty,
    StorageDeviceProtocolSpecificProperty,
    StorageAdapterTemperatureProperty,
    StorageDeviceTemperatureProperty,
    StorageAdapterPhysicalTopologyProperty,
    StorageDevicePhysicalTopologyProperty,
    StorageDeviceAttributesProperty
} STORAGE_PROPERTY_ID, *PSTORAGE_PROPERTY_ID;

typedef enum {
    PropertyStandardQuery,
    PropertyExistsQuery,
    PropertyMaskQuery,
    PropertyQueryMaxDefined
} STORAGE_QUERY_TYPE;

typedef struct {
    STORAGE_PROPERTY_ID PropertyId;
    STORAGE_QUERY_TYPE  QueryType;
    BYTE                AdditionalParameters[1];
} STORAGE_PROPERTY_QUERY;

#define IOCTL_STORAGE_QUERY_PROPERTY CTL_CODE(IOCTL_STORAGE_BASE, 0x0500, METHOD_BUFFERED, FILE_ANY_ACCESS)

BOOL IsUsbDisk(TCHAR *disk)
{
    HANDLE hDisk            = NULL;
    TCHAR  path[MAX_PATH]   = {0};
    BYTE   desc_buffer[512] = {0};
    STORAGE_PROPERTY_QUERY     query = {};
    STORAGE_DEVICE_DESCRIPTOR *pdesc = (STORAGE_DEVICE_DESCRIPTOR*)desc_buffer;
    BOOL   bResult  = FALSE;
    BOOL   bIsUDisk = FALSE;
    DWORD  nBytes   = 0;
    size_t len;

    _tcscpy(path, TEXT("\\\\.\\"));
    _tcscat(path, disk);
    len = _tcslen(path);
    if (len > 0 && path[len - 1] == TEXT('\\')) path[len - 1] = TEXT('\0');

    hDisk = CreateFile(path, GENERIC_WRITE|GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (hDisk == INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    pdesc->Size = sizeof(desc_buffer);
    query.PropertyId = StorageDeviceProperty;
    query.QueryType  = PropertyStandardQuery;
    bResult  = DeviceIoControl(hDisk, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(STORAGE_PROPERTY_QUERY), pdesc, pdesc->Size, &nBytes, (LPOVERLAPPED)NULL);
    bIsUDisk = bResult && pdesc->BusType == BusTypeUsb && pdesc->RemovableMedia;
    CloseHandle(hDisk);
    return bIsUDisk;
}

static void ErrorExit(LPTSTR lpszFunction, HWND hwnd)
{
    // Retrieve the system error message for the last-error code
    LPVOID lpMsgBuf;
    LPVOID lpDisplayBuf;
    DWORD dw = GetLastError();

    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR) &lpMsgBuf,
        0, NULL );

    // Display the error message and exit the process
    lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT,
        (lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)lpszFunction) + 40) * sizeof(TCHAR));
    StringCchPrintf((LPTSTR)lpDisplayBuf,
        LocalSize(lpDisplayBuf) / sizeof(TCHAR),
        TEXT("%s failed with error %d: %s"),
        lpszFunction, dw, lpMsgBuf);
    MessageBox(hwnd, (LPCTSTR)lpDisplayBuf, TEXT("Error"), MB_OK);

    LocalFree(lpMsgBuf);
    LocalFree(lpDisplayBuf);
}

BOOL WriteUUID(TCHAR *disk, char *uuid, int len, HWND hwnd)
{
    TCHAR  path  [MAX_PATH];
    BYTE   buffer[sizeof(SCSI_PASS_THROUGH_DIRECT) + 512] = {0};
    SCSI_PASS_THROUGH *spt = (SCSI_PASS_THROUGH*)buffer;
    CMD_UPDATE_SN     *aus = (CMD_UPDATE_SN*)&(buffer[sizeof(SCSI_PASS_THROUGH_DIRECT)]);
    BOOL   bIsUDisk = FALSE;
    BOOL   bResult  = FALSE;
    HANDLE hDevice  = NULL;
    DWORD  nBytes   = 0;
    size_t slen;

    aus->wSNLen = (int)(len < sizeof(aus->bSNData) ? len : sizeof(aus->bSNData));
    memcpy(aus->cSNSignature, "WR_DEVSN", 8);
    memcpy(aus->bSNData, uuid, aus->wSNLen);

    _tcscpy(path, TEXT("\\\\.\\"));
    _tcscat(path, disk);
    slen = _tcslen(path);
    if (len > 0 && path[slen - 1] == TEXT('\\')) path[slen - 1] = TEXT('\0');

    hDevice = CreateFile(path, GENERIC_WRITE|GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (hDevice == INVALID_HANDLE_VALUE) {
        printf("open usb dev file handle failed !\n");
        return FALSE;
    }

    if (1) { // make sure it is a usb removable disk
        BYTE desc_buffer[512];
        STORAGE_PROPERTY_QUERY     query;
        STORAGE_DEVICE_DESCRIPTOR *pdesc = (STORAGE_DEVICE_DESCRIPTOR*)desc_buffer;
        pdesc->Size = sizeof(desc_buffer);
        query.PropertyId = StorageDeviceProperty;
        query.QueryType  = PropertyStandardQuery;
        bResult  = DeviceIoControl(hDevice, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(STORAGE_PROPERTY_QUERY), pdesc, pdesc->Size, &nBytes, (LPOVERLAPPED)NULL);
        bIsUDisk = bResult && pdesc->BusType == BusTypeUsb && pdesc->RemovableMedia;
        if (!bIsUDisk) {
            printf("not udisk !\n");
            CloseHandle(hDevice);
            return FALSE;
        }
    }

    if (0) {
        spt->Length             = sizeof(SCSI_PASS_THROUGH_DIRECT);
        spt->PathId             = 0;
        spt->TargetId           = 0;
        spt->Lun                = 0;
        spt->SenseInfoLength    = 0;
        spt->SenseInfoOffset    = 0;
        spt->DataIn             = SCSI_IOCTL_DATA_OUT;
        spt->TimeOutValue       = 10;
        spt->DataTransferLength = 512;
        spt->DataBufferOffset   = sizeof(SCSI_PASS_THROUGH_DIRECT);
        spt->CdbLength          = 10;
        spt->Cdb[0]             = 0x2a;
        spt->Cdb[1]             = 0x00;
        spt->Cdb[2]             = 0x00;
        spt->Cdb[3]             = 0x00;
        spt->Cdb[4]             = 0x00;
        spt->Cdb[5]             = 0x00;
        spt->Cdb[6]             = 0x00;
        spt->Cdb[7]             = 0x00;
        spt->Cdb[8]             = 0x01;
        spt->Cdb[9]             = 0x00;
        bResult = DeviceIoControl(hDevice, IOCTL_SCSI_PASS_THROUGH, spt, sizeof(buffer), spt, sizeof(buffer), &nBytes, NULL);
        if (!bResult) {
            ErrorExit(TEXT("WriteUUID"), hwnd);
        }
    } else {
        bResult = WriteFile(hDevice, aus, 512, &nBytes, NULL);
        if (!bResult) {
            ErrorExit(TEXT("WriteUUID"), hwnd);
        }
    }

    CloseHandle(hDevice);
    return bResult;
}

