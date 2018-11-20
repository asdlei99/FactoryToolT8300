// FlashToolDlg.cpp : implementation file
//

#include "stdafx.h"
#include "FlashTool.h"
#include "FlashToolDlg.h"
#include <tchar.h>
#include <windows.h>
#include <winioctl.h>
#include <ntddscsi.h>
#include <strsafe.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

static void get_app_dir(TCHAR *path, int size)
{
    HMODULE handle = GetModuleHandle(NULL);
    GetModuleFileName(handle, path, size);
    TCHAR *str = path + _tcslen(path);
    while (*--str != TEXT('\\'));
    *str = TEXT('\0');
}

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

static BOOL IsT8300UsbDisk(TCHAR *disk)
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
    if (!bIsUDisk) return FALSE;

    TCHAR volname [256];
    TCHAR fsname  [256];
    DWORD serialnum = 0;
    DWORD fsflags   = 0;
    DWORD maxlen    = 0;
    BOOL  ret;
    ret = GetVolumeInformation(disk, volname, 256, &serialnum, &maxlen, &fsflags, fsname, 256);
    if (!ret) return FALSE;

    if ((_tcsncmp(volname, TEXT("SN9370"), 6) == 0 || _tcsncmp(volname, TEXT("ROM"), 3) == 0) && _tcscmp(fsname, TEXT("FAT")) == 0) {
        return TRUE;
    } else {
        return FALSE;
    }
}

// CFlashToolDlg dialog
#define TIMER_SCAN_UDISK    1
#define TIMER_START_CHECK   2
enum {
    WM_FW_UPDATE_START = WM_APP + 100,
    WM_FW_UPDATE_OK,
    WM_FW_UPDATE_FAILED,
};

CFlashToolDlg::CFlashToolDlg(CWnd* pParent /*=NULL*/)
    : CDialog(CFlashToolDlg::IDD, pParent)
    , m_nDeviceType(0)
    , m_strUpdateStep(_T(""))
    , m_strDeviceStatus(_T(""))
    , m_bStopDevCheck(TRUE)
    , m_bDevFound(FALSE)
    , m_bCancelUpdate(FALSE)
    , m_hUpdateThread(NULL)
{
    m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CFlashToolDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Radio(pDX, IDC_RDO_DEVICE_TYPE, m_nDeviceType);
    DDX_Text(pDX, IDC_TXT_UPDATE_STEP, m_strUpdateStep);
    DDX_Text(pDX, IDC_TXT_DEV_STATUS, m_strDeviceStatus);
    DDX_Control(pDX, IDC_PROG_UPDATE, m_ctrlProgressBar);
}

BEGIN_MESSAGE_MAP(CFlashToolDlg, CDialog)
    ON_WM_PAINT()
    ON_WM_QUERYDRAGICON()
    //}}AFX_MSG_MAP
    ON_WM_DESTROY()
    ON_WM_TIMER()
    ON_WM_CTLCOLOR()
    ON_BN_CLICKED(IDC_BTN_APPLY_UPDATE, &CFlashToolDlg::OnBnClickedBtnApplyUpdate)
    ON_MESSAGE(WM_FW_UPDATE_START , &CFlashToolDlg::OnFwUpdateStart )
    ON_MESSAGE(WM_FW_UPDATE_OK    , &CFlashToolDlg::OnFwUpdateOK    )
    ON_MESSAGE(WM_FW_UPDATE_FAILED, &CFlashToolDlg::OnFwUpdateFailed)
END_MESSAGE_MAP()


// CFlashToolDlg message handlers

BOOL CFlashToolDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    // Set the icon for this dialog.  The framework does this automatically
    //  when the application's main window is not a dialog
    SetIcon(m_hIcon, TRUE );        // Set big icon
    SetIcon(m_hIcon, FALSE);        // Set small icon

    m_fntDeviceStatus.CreatePointFont(200, TEXT("ºÚÌå"), NULL);
    GetDlgItem(IDC_TXT_DEV_STATUS)->SetFont(&m_fntDeviceStatus);

    m_bStopDevCheck = FALSE;
    SetTimer(TIMER_SCAN_UDISK, 500 , NULL);
    return TRUE;  // return TRUE  unless you set the focus to a control
}

void CFlashToolDlg::OnDestroy()
{
    CDialog::OnDestroy();

    // TODO: Add your message handler code here
    KillTimer(TIMER_SCAN_UDISK);

    if (m_hUpdateThread) {
        m_bCancelUpdate = TRUE;
        WaitForSingleObject(m_hUpdateThread, -1);
        CloseHandle(m_hUpdateThread);
        m_hUpdateThread = NULL;
    }
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CFlashToolDlg::OnPaint()
{
    if (IsIconic()) {
        CPaintDC dc(this); // device context for painting

        SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

        // Center icon in client rectangle
        int cxIcon = GetSystemMetrics(SM_CXICON);
        int cyIcon = GetSystemMetrics(SM_CYICON);
        CRect rect;
        GetClientRect(&rect);
        int x = (rect.Width() - cxIcon + 1) / 2;
        int y = (rect.Height() - cyIcon + 1) / 2;

        // Draw the icon
        dc.DrawIcon(x, y, m_hIcon);
    } else {
        CDialog::OnPaint();
    }
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CFlashToolDlg::OnQueryDragIcon()
{
    return static_cast<HCURSOR>(m_hIcon);
}

void CFlashToolDlg::OnTimer(UINT_PTR nIDEvent)
{
    TCHAR buffer[256] = {0};
    switch (nIDEvent) {
    case TIMER_SCAN_UDISK:
        if (m_bStopDevCheck) break;
        GetLogicalDriveStrings(sizeof(buffer), buffer);
        if (memcmp(m_strDriver, buffer, sizeof(buffer)) != 0) {
            memcpy(m_strDriver, buffer, sizeof(buffer));
            m_bDevFound = FALSE;
            for (int i=0; i<sizeof(buffer) && buffer[i]; i+=4) {
                if (IsT8300UsbDisk(&buffer[i])) {
                    m_strUsbDisk = &buffer[i];
                    m_bDevFound  = TRUE;
                }
            }
            if (m_bDevFound) {
                m_strUpdateStep = TEXT("Please click UPDATE button to start update ...");
                m_strDeviceStatus = TEXT("Device connected !");
                GetDlgItem(IDC_BTN_APPLY_UPDATE)->EnableWindow(TRUE );
            } else {
                m_strUpdateStep   = TEXT("Please connect device to PC via USB cable ...");
                m_strDeviceStatus = TEXT("Device not connected !");
                GetDlgItem(IDC_BTN_APPLY_UPDATE)->EnableWindow(FALSE);
            }
            UpdateData(FALSE);
        }
        break;
    case TIMER_START_CHECK:
        KillTimer(TIMER_START_CHECK);
        m_bStopDevCheck = FALSE;
        break;
    }
    CDialog::OnTimer(nIDEvent);
}

static DWORD WINAPI CopyProgressProc(
    LARGE_INTEGER TotalFileSize,
    LARGE_INTEGER TotalBytesTransferred,
    LARGE_INTEGER StreamSize,
    LARGE_INTEGER StreamBytesTransferred,
    DWORD dwStreamNumber,
    DWORD dwCallbackReason,
    HANDLE hSourceFile,
    HANDLE hDestinationFile,
    LPVOID lpData
)
{
    CFlashToolDlg *dlg = (CFlashToolDlg*)lpData;
    dlg->m_ctrlProgressBar.SetRange32(0, (int)TotalFileSize.LowPart     );
    dlg->m_ctrlProgressBar.SetPos    ((int)TotalBytesTransferred.LowPart);
    return 0;
}

static DWORD WINAPI UpdateFwThreadProc(LPVOID pParam)
{
    CFlashToolDlg *dlg = (CFlashToolDlg*)pParam;
    dlg->DoUpdateFw();
    return 0;
}

BOOL CFlashToolDlg::DoUpdateFw()
{
    TCHAR srcfn[MAX_PATH];
    TCHAR dstfn[MAX_PATH];
    BOOL  ret;

    get_app_dir(srcfn, MAX_PATH);
    _tcscat(srcfn, m_nDeviceType ? TEXT("\\rx.bin") : TEXT("\\tx.bin"));
    _tcscpy(dstfn, m_strUsbDisk);
    _tcscat(dstfn, TEXT("fw.bin"));

    PostMessage(WM_FW_UPDATE_START);
    ret = CopyFileEx(srcfn, dstfn, CopyProgressProc, this, &m_bCancelUpdate, 0);
    PostMessage(ret ? WM_FW_UPDATE_OK : WM_FW_UPDATE_FAILED);

    CloseHandle(m_hUpdateThread);
    m_hUpdateThread = NULL;
    return ret;
}

void CFlashToolDlg::OnBnClickedBtnApplyUpdate()
{
    if (m_hUpdateThread) {
        return;
    }

    UpdateData(TRUE);
    m_bCancelUpdate = FALSE;
    m_hUpdateThread = CreateThread(NULL, 0, UpdateFwThreadProc, this, 0, NULL);
}

LRESULT CFlashToolDlg::OnFwUpdateStart(WPARAM wParam, LPARAM lParam)
{
    GetDlgItem(IDC_PROG_UPDATE)->ShowWindow(SW_SHOW);
    GetDlgItem(IDC_BTN_APPLY_UPDATE)->EnableWindow(FALSE);
    m_strUpdateStep   = TEXT("Please wait until update finish...");
    m_strDeviceStatus = TEXT("Updating...");
    m_bStopDevCheck   = TRUE;
    UpdateData(FALSE);
    return 0;
}

LRESULT CFlashToolDlg::OnFwUpdateOK(WPARAM wParam, LPARAM lParam)
{
    GetDlgItem(IDC_PROG_UPDATE)->ShowWindow(SW_HIDE);
    m_strDeviceStatus = TEXT("Update Successfully !");
    UpdateData(FALSE);
    SetTimer(TIMER_START_CHECK, 3000, NULL);
    return 0;
}

LRESULT CFlashToolDlg::OnFwUpdateFailed(WPARAM wParam, LPARAM lParam)
{
    GetDlgItem(IDC_PROG_UPDATE)->ShowWindow(SW_HIDE);
    m_strDeviceStatus = TEXT("Update Failed !");
    UpdateData(FALSE);
    SetTimer(TIMER_START_CHECK, 3000, NULL);
    return 0;
}

HBRUSH CFlashToolDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    HBRUSH hbr = CDialog::OnCtlColor(pDC, pWnd, nCtlColor);
    switch (pWnd->GetDlgCtrlID()) {
    case IDC_TXT_DEV_STATUS:
        if (m_strDeviceStatus.Find(TEXT("Failed")) != -1) {
            pDC->SetTextColor(RGB(255, 0, 0));
        } else if (m_strDeviceStatus.Find(TEXT("Successfully")) != -1) {
            pDC->SetTextColor(RGB(0, 188, 0));
        } else {
            pDC->SetTextColor(RGB(0, 0, 0));
        }
        break;
    }
    return hbr;
}

