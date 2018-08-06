// WriteSNT8300Dlg.h : header file
//

#pragma once
#include "afxwin.h"


// CWriteSNT8300Dlg dialog
class CWriteSNT8300Dlg : public CDialog
{
// Construction
public:
    CWriteSNT8300Dlg(CWnd* pParent = NULL); // standard constructor

// Dialog Data
    enum { IDD = IDD_WRITESNT8300_DIALOG };

protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support


// Implementation
protected:
    HICON m_hIcon;

    // Generated message map functions
    virtual BOOL OnInitDialog();
    afx_msg void OnPaint();
    afx_msg void OnBnClickedBtnWriteUuid();
    afx_msg HCURSOR OnQueryDragIcon();
    DECLARE_MESSAGE_MAP()

private:
    CComboBox m_comboDriverList;
    int       m_nUUIDLen;
    CString   m_strUUIDStr;
    TCHAR     m_strDriver[256];

public:
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnDestroy();
};
