// SerialCmdToolDlg.h : header file
//

#pragma once
#include "afxwin.h"


// CSerialCmdToolDlg dialog
class CSerialCmdToolDlg : public CDialog
{
// Construction
public:
	CSerialCmdToolDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
	enum { IDD = IDD_SERIALCMDTOOL_DIALOG };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
    virtual void OnOK();

// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	virtual BOOL OnInitDialog();
    afx_msg void OnDestroy();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
    afx_msg void OnBnClickedBtnClearReceived();
    afx_msg void OnBnClickedBtnClearSend();
    afx_msg void OnBnClickedBtnSendCmds();
    afx_msg void OnBnClickedBtnOpenCloseCom();
    afx_msg void OnCbnDropdownCombo1();
    afx_msg LRESULT OnWMUpdateRxUI(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnWMUpdateTxUI(WPARAM wParam, LPARAM lParam);
	DECLARE_MESSAGE_MAP()

private:    
    CEdit     m_ctrEdtTx;
    CEdit     m_ctrEdtRx;
    CComboBox m_ctlComPort;
    int       m_nCOMList[10];
    HANDLE    m_hRxThread;
    HANDLE    m_hTxThread;

public:
    CString   m_strTx;
    CString   m_strRx;
    CString   m_strTxStatus;
    int       m_nDelayTime;
    HANDLE    m_hCOM;
    #define RX_EXIT  (1 << 0)
    #define TX_EXIT  (1 << 1)
    int       m_nCOMExit;

private:
    void DetectComPort();
};
