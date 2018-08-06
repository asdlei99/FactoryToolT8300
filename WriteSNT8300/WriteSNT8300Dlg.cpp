// WriteSNT8300Dlg.cpp : implementation file
//

#include "stdafx.h"
#include "WriteSNT8300.h"
#include "WriteSNT8300Dlg.h"
#include "WriteUuidApi.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define TIMER_SCAN_UDISK   1

// CWriteSNT8300Dlg dialog

CWriteSNT8300Dlg::CWriteSNT8300Dlg(CWnd* pParent /*=NULL*/)
    : CDialog(CWriteSNT8300Dlg::IDD, pParent)
    , m_nUUIDLen(0)
    , m_strUUIDStr(_T(""))
{
    m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CWriteSNT8300Dlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Text(pDX, IDC_EDT_UUID_LEN, m_nUUIDLen);
    DDX_Text(pDX, IDC_EDT_UUID_STR, m_strUUIDStr);
    DDX_Control(pDX, IDC_EDT_DRIVERS, m_comboDriverList);
}

BEGIN_MESSAGE_MAP(CWriteSNT8300Dlg, CDialog)
    ON_WM_PAINT()
    ON_WM_QUERYDRAGICON()
    //}}AFX_MSG_MAP
    ON_BN_CLICKED(IDC_BTN_WRITE_UUID, &CWriteSNT8300Dlg::OnBnClickedBtnWriteUuid)
    ON_WM_TIMER()
    ON_WM_DESTROY()
END_MESSAGE_MAP()


// CWriteSNT8300Dlg message handlers

BOOL CWriteSNT8300Dlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    // Set the icon for this dialog.  The framework does this automatically
    //  when the application's main window is not a dialog
    SetIcon(m_hIcon, TRUE);         // Set big icon
    SetIcon(m_hIcon, FALSE);        // Set small icon

    // TODO: Add extra initialization here
    m_strUUIDStr = "SN20180806110801";
    m_nUUIDLen   = 16;
    UpdateData(FALSE);
    SetTimer(TIMER_SCAN_UDISK, 500, NULL);
    return TRUE;  // return TRUE  unless you set the focus to a control
}

void CWriteSNT8300Dlg::OnDestroy()
{
    CDialog::OnDestroy();

    // TODO: Add your message handler code here
    KillTimer(TIMER_SCAN_UDISK);
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CWriteSNT8300Dlg::OnPaint()
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
HCURSOR CWriteSNT8300Dlg::OnQueryDragIcon()
{
    return static_cast<HCURSOR>(m_hIcon);
}

void CWriteSNT8300Dlg::OnTimer(UINT_PTR nIDEvent)
{
    // TODO: Add your message handler code here and/or call default
    switch (nIDEvent) {
    case TIMER_SCAN_UDISK:
        TCHAR buffer[256] = {0};
        GetLogicalDriveStrings(sizeof(buffer), buffer);
        if (memcmp(m_strDriver, buffer, sizeof(buffer)) != 0) {
            memcpy(m_strDriver, buffer, sizeof(buffer));
            m_comboDriverList.ResetContent();
            for (int i=0; i<sizeof(buffer); i+=4) {
                if (buffer[i]) {
                    m_comboDriverList.AddString(&buffer[i]);
                    if (IsUsbDisk(&buffer[i])) {
                        m_comboDriverList.SelectString(0, &buffer[i]);
                    }
                } else {
                    break;
                }
            }
        }
        break;
    }
    CDialog::OnTimer(nIDEvent);
}

void CWriteSNT8300Dlg::OnBnClickedBtnWriteUuid()
{
    // TODO: Add your control notification handler code here
    int sel = m_comboDriverList.GetCurSel();
    if (sel == -1) {
        AfxMessageBox(TEXT("ÇëÑ¡ÔñÕýÈ·µÄÇý¶¯Æ÷ÅÌ·û£¡"));
        return ;
    }
    TCHAR disk[128] = {0};
    char  uuid[512] = {0};
    m_comboDriverList.GetLBText(sel, disk);
    WideCharToMultiByte(CP_ACP, 0, m_strUUIDStr, m_strUUIDStr.GetLength(), uuid, 512, NULL, NULL);
    BOOL ret = WriteUUID(disk, uuid, m_nUUIDLen);
    AfxMessageBox(ret ? TEXT("ÉÕÐ´³É¹¦£¡") : TEXT("ÉÕÐ´Ê§°Ü£¡"));
}




