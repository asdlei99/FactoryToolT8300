// FlashToolDlg.h : header file
//

#pragma once
#include "afxcmn.h"


// CFlashToolDlg dialog
class CFlashToolDlg : public CDialog
{
// Construction
public:
    CFlashToolDlg(CWnd* pParent = NULL);    // standard constructor

// Dialog Data
    enum { IDD = IDD_FLASHTOOL_DIALOG };

protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support


// Implementation
protected:
    HICON m_hIcon;

    // Generated message map functions
    virtual BOOL OnInitDialog();
    afx_msg void OnDestroy();
    afx_msg void OnPaint();
    afx_msg HCURSOR OnQueryDragIcon();
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnBnClickedBtnApplyUpdate();
    afx_msg HBRUSH  OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
    afx_msg LRESULT OnFwUpdateStart (WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnFwUpdateOK    (WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnFwUpdateFailed(WPARAM wParam, LPARAM lParam);
    DECLARE_MESSAGE_MAP()

private:
    int           m_nDeviceType;
    CString       m_strUpdateStep;
    CString       m_strDeviceStatus;
    CFont         m_fntDeviceStatus;

private:
    BOOL          m_bStopDevCheck;
    BOOL          m_bDevFound;
    TCHAR         m_strDriver[256];
    CString       m_strUsbDisk;
    BOOL          m_bCancelUpdate;
    HANDLE        m_hUpdateThread;

public:
    CProgressCtrl m_ctrlProgressBar;
    BOOL DoUpdateFw();
};
