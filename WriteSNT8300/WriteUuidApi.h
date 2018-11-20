#pragma once

BOOL IsT8300UsbDisk(TCHAR *disk);
BOOL WriteUUID(TCHAR *disk, char *uuid, int len, HWND hwnd);