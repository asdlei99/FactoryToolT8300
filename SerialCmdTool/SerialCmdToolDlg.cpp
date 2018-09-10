// SerialCmdToolDlg.cpp : implementation file
//

#include "stdafx.h"
#include "SerialCmdTool.h"
#include "SerialCmdToolDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define WM_UPDATE_RX_UI  (WM_APP + 100)
#define WM_UPDATE_TX_UI  (WM_APP + 101)
static DWORD WINAPI SerialPortRxThread(LPVOID params)
{
    CSerialCmdToolDlg *dlg = (CSerialCmdToolDlg*)params;
    OVERLAPPED      osRead = {0};

    while (!(dlg->m_nCOMExit & RX_EXIT)) {
        char  buf[256];
        DWORD readn = 0;
        if (!ReadFile(dlg->m_hCOM, buf, sizeof(buf)-1, &readn, &osRead)) {
            if (GetLastError() == ERROR_IO_PENDING) {
                GetOverlappedResult(dlg->m_hCOM, &osRead, &readn, TRUE);
            }
        }

        if (readn > 0) {
            buf[readn]    = '\0';
            dlg->m_strRx += buf;
            dlg->PostMessage(WM_UPDATE_RX_UI);
        }
    }

    return 0;
}

static DWORD WINAPI SerialPortTxThread(LPVOID params)
{
    CSerialCmdToolDlg *dlg = (CSerialCmdToolDlg*)params;
    CString cmds = dlg->m_strTx;
    int    delay = dlg->m_nDelayTime;
    int    start = 0;
    int    end   = 0;
    OVERLAPPED osWrite = {0};

    dlg->GetDlgItem(IDC_BTN_SEND_CMDS)->EnableWindow(FALSE);
    while (end != -1 && !(dlg->m_nCOMExit & TX_EXIT)) {
        end   = cmds.Find(TEXT('\n'), start);
        CString str = cmds.Mid(start, end == -1 ? cmds.GetLength() : end - start + 1);
        start = end + 1;
//      OutputDebugString(str);
        dlg->m_strTxStatus = str;
        dlg->PostMessage(WM_UPDATE_TX_UI);
        if (0) {
            int   len = WideCharToMultiByte(CP_ACP, 0, str, -1, NULL, 0, NULL, NULL);
            char *buf = (char*)malloc(len + 1);
            DWORD written = 0;
            if (buf) {
                WideCharToMultiByte(CP_ACP, 0, str, -1, buf, len, NULL, NULL);
                if (!WriteFile(dlg->m_hCOM, buf, len, &written, &osWrite)) {
                    if (GetLastError() == ERROR_IO_PENDING) {
                        GetOverlappedResult(dlg->m_hCOM, &osWrite, &written, TRUE);
                    }
                }
                free(buf);
            }
        }
        if (delay > 0) Sleep(delay);
    }
    if (!(dlg->m_nCOMExit & TX_EXIT)) {
        dlg->GetDlgItem(IDC_BTN_SEND_CMDS)->EnableWindow(TRUE);
    }
    return TRUE;
}

// CSerialCmdToolDlg dialog

CSerialCmdToolDlg::CSerialCmdToolDlg(CWnd* pParent /*=NULL*/)
    : CDialog(CSerialCmdToolDlg::IDD, pParent)
    , m_strRx(_T(""))
    , m_strTx(_T(""))
    , m_nDelayTime(500)
    , m_strTxStatus(_T(""))
{
    m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CSerialCmdToolDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Text(pDX, IDC_EDT_DELAY_TIME, m_nDelayTime);
    DDX_Control(pDX, IDC_COMBO1, m_ctlComPort);
    DDX_Control(pDX, IDC_EDT_RX, m_ctrEdtRx);
    DDX_Control(pDX, IDC_EDT_TX, m_ctrEdtTx);
    DDX_Text(pDX, IDC_TXT_TX_STATUS, m_strTxStatus);
}

BEGIN_MESSAGE_MAP(CSerialCmdToolDlg, CDialog)
    ON_WM_PAINT()
    ON_WM_QUERYDRAGICON()
    //}}AFX_MSG_MAP
    ON_BN_CLICKED(IDC_BTN_CLEAR_RECEIVED, &CSerialCmdToolDlg::OnBnClickedBtnClearReceived)
    ON_BN_CLICKED(IDC_BTN_CLEAR_SEND, &CSerialCmdToolDlg::OnBnClickedBtnClearSend)
    ON_BN_CLICKED(IDC_BTN_SEND_CMDS, &CSerialCmdToolDlg::OnBnClickedBtnSendCmds)
    ON_BN_CLICKED(IDC_BTN_OPEN_CLOSE_COM, &CSerialCmdToolDlg::OnBnClickedBtnOpenCloseCom)
    ON_WM_TIMER()
    ON_WM_DESTROY()
    ON_MESSAGE(WM_UPDATE_RX_UI, &CSerialCmdToolDlg::OnWMUpdateRxUI)
    ON_MESSAGE(WM_UPDATE_TX_UI, &CSerialCmdToolDlg::OnWMUpdateTxUI)
    ON_CBN_DROPDOWN(IDC_COMBO1, &CSerialCmdToolDlg::OnCbnDropdownCombo1)
END_MESSAGE_MAP()


// CSerialCmdToolDlg message handlers

BOOL CSerialCmdToolDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    // Set the icon for this dialog.  The framework does this automatically
    //  when the application's main window is not a dialog
    SetIcon(m_hIcon, TRUE);         // Set big icon
    SetIcon(m_hIcon, FALSE);        // Set small icon

    memset(m_nCOMList, 0, sizeof(m_nCOMList));
    m_hRxThread  = NULL;
    m_hTxThread  = NULL;
    m_hCOM       = NULL;
    m_nCOMExit   = 0;
    DetectComPort();
    m_ctlComPort.SetCurSel(0);
    return TRUE;  // return TRUE  unless you set the focus to a control
}

void CSerialCmdToolDlg::OnDestroy()
{
    CDialog::OnDestroy();
    m_nCOMExit |= (RX_EXIT|TX_EXIT);
    WaitForSingleObject(m_hRxThread, -1);
    CloseHandle(m_hRxThread);
    WaitForSingleObject(m_hTxThread, -1);
    CloseHandle(m_hTxThread);
    if (m_hCOM) CloseHandle(m_hCOM);
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CSerialCmdToolDlg::OnPaint()
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
HCURSOR CSerialCmdToolDlg::OnQueryDragIcon()
{
    return static_cast<HCURSOR>(m_hIcon);
}

void CSerialCmdToolDlg::OnBnClickedBtnClearReceived()
{
    m_strRx = TEXT("");
    m_ctrEdtRx.SetWindowText(m_strRx);
}

void CSerialCmdToolDlg::OnBnClickedBtnClearSend()
{
    m_strTx = TEXT("");
    m_ctrEdtTx.SetWindowText(m_strTx);
}

void CSerialCmdToolDlg::OnBnClickedBtnSendCmds()
{
    if (!m_hCOM) return;
    UpdateData(TRUE);
    m_ctrEdtTx.GetWindowText(m_strTx);

    if (m_hTxThread) {
        m_nCOMExit |= TX_EXIT;
        WaitForSingleObject(m_hTxThread, -1);
        CloseHandle(m_hTxThread);
        m_hTxThread = NULL;
    }
    m_nCOMExit &= ~TX_EXIT;
    m_hTxThread = CreateThread(NULL, 0, SerialPortTxThread, this, 0, NULL);
}

void CSerialCmdToolDlg::OnBnClickedBtnOpenCloseCom()
{
    if (m_hCOM) {
        GetDlgItem(IDC_BTN_OPEN_CLOSE_COM)->SetWindowText(TEXT("打开串口"));
        GetDlgItem(IDC_COMBO1)->EnableWindow(TRUE);
        m_nCOMExit |= RX_EXIT|TX_EXIT;
        WaitForSingleObject(m_hRxThread, -1);
        CloseHandle(m_hRxThread);
        WaitForSingleObject(m_hTxThread, -1);
        CloseHandle(m_hTxThread);
        m_hTxThread = NULL;
        m_hRxThread = NULL;
        CloseHandle(m_hCOM);
        m_hCOM = NULL;
        GetDlgItem(IDC_BTN_SEND_CMDS)->EnableWindow(TRUE);
    } else {
        TCHAR name[MAX_PATH];
        int sel = m_ctlComPort.GetCurSel();
        if (sel == -1) {
            AfxMessageBox(TEXT("请选择正确的驱动器盘符！"));
            return;
        }
        m_ctlComPort.GetLBText(sel, name);
        m_hCOM = CreateFile(name, GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
        if (!m_hCOM) {
            AfxMessageBox(TEXT("打开串口失败！"));
            return;
        }
        GetDlgItem(IDC_BTN_OPEN_CLOSE_COM)->SetWindowText(TEXT("关闭串口"));
        GetDlgItem(IDC_COMBO1)->EnableWindow(FALSE);

        // timeouts
        COMMTIMEOUTS timeouts = {0};
        GetCommTimeouts(m_hCOM, &timeouts);
        timeouts.ReadIntervalTimeout         = 0;
        timeouts.ReadTotalTimeoutMultiplier  = 0;
        timeouts.ReadTotalTimeoutConstant    = 500;
        timeouts.WriteTotalTimeoutMultiplier = 0;
        timeouts.WriteTotalTimeoutConstant   = 500;
        SetCommTimeouts(m_hCOM, &timeouts);

        // dcb
        DCB dcb = {0};
        GetCommState(m_hCOM, &dcb);
        dcb.DCBlength = sizeof(DCB);
        dcb.BaudRate  = 115200;
        dcb.Parity    = 0;
        dcb.ByteSize  = 8;
        dcb.StopBits  = ONESTOPBIT;
        SetCommState(m_hCOM, &dcb);

        // tx & rx buffer
        SetupComm(m_hCOM, 1*1024, 1*1024);

        // clear tx & rx buffer
        PurgeComm(m_hCOM, PURGE_RXCLEAR|PURGE_TXCLEAR);

        // clear errors
        DWORD   dwError =  0;
        COMSTAT cs      = {0};
        ClearCommError(m_hCOM, &dwError, &cs);

        // set event mask
        SetCommMask(m_hCOM, EV_RXCHAR|EV_TXEMPTY);

        m_nCOMExit &=~RX_EXIT;
        m_hRxThread = CreateThread(NULL, 0, SerialPortRxThread, this, 0, NULL);
    }
}


void CSerialCmdToolDlg::DetectComPort()
{
    TCHAR name[MAX_PATH];
    int   list[10] = {0};
    int   i;
    for (i=0; i<10; i++) {
        HANDLE h = INVALID_HANDLE_VALUE;
        _stprintf_s(name, MAX_PATH, TEXT("\\\\.\\COM%d"), i);
        h = CreateFile(name, GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
        CloseHandle(h);
        list[i] = (h != INVALID_HANDLE_VALUE) ? 1 : 0;
    }
    if (memcmp(m_nCOMList, list, sizeof(list)) != 0) {
        memcpy(m_nCOMList, list, sizeof(list));
        m_ctlComPort.ResetContent();
        for (i=0; i<10; i++) {
            if (m_nCOMList[i]) {
                _stprintf_s(name, MAX_PATH, TEXT("COM%d"), i);
                m_ctlComPort.AddString(name);
            }
        }
    }
}

void CSerialCmdToolDlg::OnCbnDropdownCombo1()
{
    DetectComPort();
}

LRESULT CSerialCmdToolDlg::OnWMUpdateRxUI(WPARAM wParam, LPARAM lParam)
{
    int len = m_strRx.GetLength();
    m_ctrEdtRx.SetWindowText(m_strRx);
    m_ctrEdtRx.SetSel(len, len, FALSE);
    return 0;
}

LRESULT CSerialCmdToolDlg::OnWMUpdateTxUI(WPARAM wParam, LPARAM lParam)
{
    UpdateData(FALSE);
    return 0;
}

void CSerialCmdToolDlg::OnOK() {}
