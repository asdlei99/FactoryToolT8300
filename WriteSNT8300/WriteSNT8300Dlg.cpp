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
#define TIMER_SET_FOCUS    2
#define TIMER_WRITE_UUID   3


static void get_app_dir(char *path, int size)
{
    HMODULE handle = GetModuleHandle(NULL);
    GetModuleFileNameA(handle, path, size);
    char  *str = path + strlen(path);
    while (*--str != '\\');
    *str = '\0';
}

static void parse_params(const char *str, const char *key, char *val)
{
    char *p = (char*)strstr(str, key);
    int   i;

    if (!p) return;
    p += strlen(key);
    if (*p == '\0') return;

    while (1) {
        if (*p != ' ' && *p != '=' && *p != ':') break;
        else p++;
    }

    for (i=0; i<MAX_PATH; i++) {
        if (*p == ',' || *p == ';' || *p == '\r' || *p == '\n' || *p == '\0') {
            val[i] = '\0';
            break;
        } else {
            val[i] = *p++;
        }
    }
}

static int load_config_from_file(char *snlen, char *autoinc, char *start, char *end, char *cur, char *check)
{
    char  file[MAX_PATH];
    FILE *fp = NULL;
    char *buf= NULL;
    int   len= 0;

    // open params file
    get_app_dir(file, MAX_PATH);
    strcat(file, "\\WriteSnT8300.ini");
    fp = fopen(file, "rb");

    if (fp) {
        fseek(fp, 0, SEEK_END);
        len = ftell(fp);
        buf = (char*)malloc(len);
        if (buf) {
            fseek(fp, 0, SEEK_SET);
            fread(buf, len, 1, fp);
            parse_params(buf, "sn_len"  , snlen  );
            parse_params(buf, "sn_inc"  , autoinc);
            parse_params(buf, "sn_start", start  );
            parse_params(buf, "sn_end"  , end    );
            parse_params(buf, "sn_cur"  , cur    );
            parse_params(buf, "sn_check", check  );
            free(buf);
        }
        fclose(fp);
        return 0;
    }

    return -1;
}

static void sn_auto_inc(char *start, char *end, char *cur, char *check, int len, int inc)
{
    char  file[MAX_PATH];
    FILE *fp = NULL;
    int   c = inc ? 1 : 0;
    int   i;

    for (i=len-1; i>=0; i--) {
        if (c) cur[i]++;
        else break;

        switch (inc) {
        case 1: // 十进制
            if (cur[i] != ':') c = 0;
            else cur[i] = '0';
            break;
        case 2: // 十六进制
            if (cur[i] == ':') cur[i] = 'A';
            if (cur[i] != 'G') c = 0;
            else cur[i] = '0';
            break;
        }
    }

    get_app_dir(file, MAX_PATH);
    strcat(file, "\\WriteSnT8300.ini");
    fp = fopen(file, "wb");
    if (fp) {
        fprintf(fp, "sn_len   = %d\r\n", len  );
        fprintf(fp, "sn_inc   = %d\r\n", inc  );
        fprintf(fp, "sn_start = %s\r\n", start);
        fprintf(fp, "sn_end   = %s\r\n", end  );
        fprintf(fp, "sn_cur   = %s\r\n", cur  );
        fprintf(fp, "sn_check = %s\r\n", check);
        fclose(fp);
    }
}

static void sn_save_burned(char *burned)
{
    char  file[MAX_PATH];
    FILE *fp = NULL;
    get_app_dir(file, MAX_PATH);
    strcat(file, "\\已烧写的序列号.txt");
    fp = fopen(file, "ab");
    if (fp) {
        fprintf(fp, "%s\r\n", burned);
        fclose(fp);
    }
}

// CWriteSNT8300Dlg dialog
CWriteSNT8300Dlg::CWriteSNT8300Dlg(CWnd* pParent /*=NULL*/)
    : CDialog(CWriteSNT8300Dlg::IDD, pParent)
    , m_nSnLen(0)
    , m_strSnStr(_T(""))
    , m_strSnScan(_T(""))
    , m_bDevFound(FALSE)
    , m_bScaned(FALSE)
    , m_nResult(2)
    , m_strWriteSnResult(_T(""))
{
    m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CWriteSNT8300Dlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Text(pDX, IDC_EDT_UUID_LEN, m_nSnLen);
    DDX_Text(pDX, IDC_EDT_UUID_STR, m_strSnStr);
    DDX_Text(pDX, IDC_EDT_UUID_SCAN, m_strSnScan);
    DDX_Control(pDX, IDC_EDT_DRIVERS, m_comboDriverList);
    DDX_Text(pDX, IDC_TXT_RESULT, m_strWriteSnResult);
}

BEGIN_MESSAGE_MAP(CWriteSNT8300Dlg, CDialog)
    ON_WM_PAINT()
    ON_WM_QUERYDRAGICON()
    //}}AFX_MSG_MAP
    ON_BN_CLICKED(IDC_BTN_WRITE_UUID, &CWriteSNT8300Dlg::OnBnClickedBtnWriteUuid)
    ON_EN_CHANGE(IDC_EDT_UUID_SCAN, &CWriteSNT8300Dlg::OnEnChangeEdtUuidScan)
    ON_WM_TIMER()
    ON_WM_DESTROY()
    ON_WM_CTLCOLOR()
END_MESSAGE_MAP()


// CWriteSNT8300Dlg message handlers

BOOL CWriteSNT8300Dlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    // Set the icon for this dialog.  The framework does this automatically
    //  when the application's main window is not a dialog
    SetIcon(m_hIcon, TRUE);         // Set big icon
    SetIcon(m_hIcon, FALSE);        // Set small icon

    m_fntResult.CreatePointFont(150, TEXT("黑体"), NULL);
    GetDlgItem(IDC_TXT_RESULT)->SetFont(&m_fntResult);
    m_strWriteSnResult = TEXT("      序列号烧写工具 v1.0.1");

    strcpy(m_strSnLength , "16");
    strcpy(m_strSnAutoInc, "0" );
    strcpy(m_strSnStart  , "T8300E0018320001");
    strcpy(m_strSnEnd    , "T8300FFFFFFFFFFF");
    strcpy(m_strSnCur    , "T8300E0018320001");
    strcpy(m_strSnCheck  , "T8300");
    if (load_config_from_file(m_strSnLength, m_strSnAutoInc, m_strSnStart, m_strSnEnd, m_strSnCur, m_strSnCheck) < 0) {
        sn_auto_inc(m_strSnStart, m_strSnEnd, m_strSnCur, m_strSnCheck, atoi(m_strSnLength), atoi(m_strSnAutoInc));
    }

    m_strSnStr = m_strSnCur;
    m_nSnLen   = atoi(m_strSnLength );
    m_nAutoInc = atoi(m_strSnAutoInc);
    UpdateData(FALSE);
    SetTimer(TIMER_SCAN_UDISK, 500 , NULL);
    SetTimer(TIMER_SET_FOCUS , 5000, NULL);
    return TRUE;  // return TRUE  unless you set the focus to a control
}

void CWriteSNT8300Dlg::OnDestroy()
{
    CDialog::OnDestroy();

    // TODO: Add your message handler code here
    KillTimer(TIMER_SCAN_UDISK);
    KillTimer(TIMER_SET_FOCUS );
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
    TCHAR buffer[256] = {0};
    switch (nIDEvent) {
    case TIMER_SCAN_UDISK:
        GetLogicalDriveStrings(sizeof(buffer), buffer);
        if (memcmp(m_strDriver, buffer, sizeof(buffer)) != 0) {
            memcpy(m_strDriver, buffer, sizeof(buffer));
            m_comboDriverList.ResetContent();
            m_bDevFound = FALSE;
            for (int i=0; i<sizeof(buffer) && buffer[i]; i+=4) {
                m_comboDriverList.AddString(&buffer[i]);
                if (IsUsbDisk(&buffer[i])) {
                    m_comboDriverList.SelectString(0, &buffer[i]);
                    m_bDevFound = TRUE;
                }
            }
            if (m_bDevFound && m_bScaned) {
                SetTimer(TIMER_WRITE_UUID, 100, NULL);
            } else {
                m_nResult = 2;
                m_strWriteSnResult = TEXT("请扫描条码...");
                UpdateData(FALSE);
            }
        }
        break;
    case TIMER_SET_FOCUS:
        GetDlgItem(IDC_EDT_UUID_SCAN)->SetFocus();
        break;
    case TIMER_WRITE_UUID:
        KillTimer(TIMER_WRITE_UUID);
        OnBnClickedBtnWriteUuid();
        break;
    }
    CDialog::OnTimer(nIDEvent);
}

void CWriteSNT8300Dlg::OnBnClickedBtnWriteUuid()
{
    int sel = m_comboDriverList.GetCurSel();
    if (sel == -1) {
        AfxMessageBox(TEXT("请选择正确的驱动器盘符！"));
        return;
    }

    UpdateData(TRUE);
    TCHAR disk[128] = {0};
    m_comboDriverList.GetLBText(sel, disk);
    WideCharToMultiByte(CP_ACP, 0, m_strSnStr, m_strSnStr.GetLength(), m_strSnCur, sizeof(m_strSnCur), NULL, NULL);
    if (strcmp(m_strSnCur, m_strSnEnd) > 0) {
        AfxMessageBox(TEXT("序列号资源已用完！"));
        return;
    }

    m_nResult = WriteUUID(disk, m_strSnCur, m_nSnLen, GetSafeHwnd()) ? 1 : 0;
    m_strWriteSnResult = (m_nResult == 1) ? TEXT("          烧写成功！；-）") : TEXT("烧写失败！\r\n请检查设备状态和 USB 连接状态！");
    if (m_nResult == 1) {
        sn_save_burned(m_strSnCur);
        sn_auto_inc(m_strSnStart, m_strSnEnd, m_strSnCur, m_strSnCheck, m_nSnLen, m_nAutoInc);
        m_strSnStr = m_strSnCur;
        m_bScaned  = FALSE;
    }
    UpdateData(FALSE);
}

BOOL CWriteSNT8300Dlg::PreTranslateMessage(MSG *pMsg)
{
    if (pMsg->message == WM_KEYDOWN || pMsg->message == WM_KEYUP) {
        switch (pMsg->wParam) {
        case VK_SPACE: if (pMsg->message == WM_KEYDOWN) OnBnClickedBtnWriteUuid(); return TRUE;
        case VK_RETURN: return TRUE;
        }
    }
    return CDialog::PreTranslateMessage(pMsg);
}

void CWriteSNT8300Dlg::OnEnChangeEdtUuidScan()
{
    // TODO:  If this is a RICHEDIT control, the control will not
    // send this notification unless you override the CDialog::OnInitDialog()
    // function and call CRichEditCtrl().SetEventMask()
    // with the ENM_CHANGE flag ORed into the mask.

    // TODO:  Add your control notification handler code here
    UpdateData(TRUE);
    if (m_strSnScan.GetLength() >= m_nSnLen) {
        m_strSnStr  = m_strSnScan.Trim();
        m_strSnStr  = m_strSnScan.Left(m_nSnLen);
        m_strSnScan = "";

        if (strlen(m_strSnCheck) > 0) {
            if (m_strSnStr.Find(CString(m_strSnCheck)) != 0) {
                AfxMessageBox(TEXT("序列号格式错误！\r\n请重新扫码！"));
                UpdateData(FALSE);
                return;
            }
        }

        m_bScaned   = TRUE;
        if (m_bDevFound && m_bScaned) {
            SetTimer(TIMER_WRITE_UUID, 100, NULL);
        } else {
            m_nResult = 2;
            m_strWriteSnResult = TEXT("          请连接设备...");
        }
        UpdateData(FALSE);
    }
}

HBRUSH CWriteSNT8300Dlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    HBRUSH hbr = CDialog::OnCtlColor(pDC, pWnd, nCtlColor);
    switch (pWnd->GetDlgCtrlID()) {
    case IDC_TXT_RESULT:
        switch (m_nResult) {
        case 0: pDC->SetTextColor(RGB(255, 0, 0)); break;
        case 1: pDC->SetTextColor(RGB(0, 155, 0)); break;
        case 2: pDC->SetTextColor(RGB(0,   0, 0)); break;
        }
        break;
    }
    return hbr;
}
