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

static int load_config_from_file(char *snlen, char *autoinc, char *start, char *end, char *cur)
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
            free(buf);
        }
        fclose(fp);
        return 0;
    }

    return -1;
}

static void sn_auto_inc(char *start, char *end, char *cur, int len, int inc)
{
    char  file[MAX_PATH];
    FILE *fp = NULL;
    int   c = 1;
    int   i;

    if (!inc) return;
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
        fclose(fp);
    }
}

// CWriteSNT8300Dlg dialog
CWriteSNT8300Dlg::CWriteSNT8300Dlg(CWnd* pParent /*=NULL*/)
    : CDialog(CWriteSNT8300Dlg::IDD, pParent)
    , m_nSnLen(0)
    , m_strSnStr(_T(""))
    , m_strSnScan(_T(""))
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
}

BEGIN_MESSAGE_MAP(CWriteSNT8300Dlg, CDialog)
    ON_WM_PAINT()
    ON_WM_QUERYDRAGICON()
    //}}AFX_MSG_MAP
    ON_BN_CLICKED(IDC_BTN_WRITE_UUID, &CWriteSNT8300Dlg::OnBnClickedBtnWriteUuid)
    ON_WM_TIMER()
    ON_WM_DESTROY()
    ON_EN_CHANGE(IDC_EDT_UUID_SCAN, &CWriteSNT8300Dlg::OnEnChangeEdtUuidScan)
END_MESSAGE_MAP()


// CWriteSNT8300Dlg message handlers

BOOL CWriteSNT8300Dlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    // Set the icon for this dialog.  The framework does this automatically
    //  when the application's main window is not a dialog
    SetIcon(m_hIcon, TRUE);         // Set big icon
    SetIcon(m_hIcon, FALSE);        // Set small icon

    strcpy(m_strSnLength , "16");
    strcpy(m_strSnAutoInc, "0" );
    strcpy(m_strSnStart  , "T8300E0018320001");
    strcpy(m_strSnEnd    , "T8300E001832FFFF");
    strcpy(m_strSnCur    , "T8300E0018320001");
    load_config_from_file(m_strSnLength, m_strSnAutoInc, m_strSnStart, m_strSnEnd, m_strSnCur);

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
    case TIMER_SET_FOCUS:
        GetDlgItem(IDC_EDT_UUID_SCAN)->SetFocus();
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

    BOOL ret = WriteUUID(disk, m_strSnCur, m_nSnLen);
    AfxMessageBox(ret ? TEXT("烧写成功！") : TEXT("烧写失败！"));
    if (ret) {
        sn_auto_inc(m_strSnStart, m_strSnEnd, m_strSnCur, m_nSnLen, m_nAutoInc);
        m_strSnStr = m_strSnCur;
        UpdateData(FALSE);
    }
}

BOOL CWriteSNT8300Dlg::PreTranslateMessage(MSG *pMsg)
{
    if (pMsg->message == WM_KEYDOWN || pMsg->message == WM_KEYUP) {
        switch (pMsg->wParam) {
        case VK_SPACE: if (pMsg->message == WM_KEYDOWN) OnBnClickedBtnWriteUuid(); return TRUE;
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
    if (m_strSnScan.GetLength() >= 16) {
        m_strSnStr  = m_strSnScan.Trim();
        m_strSnScan = "";
        UpdateData(FALSE);
    }
}
