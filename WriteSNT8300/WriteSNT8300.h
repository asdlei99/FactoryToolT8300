// WriteSNT8300.h : main header file for the PROJECT_NAME application
//

#pragma once

#ifndef __AFXWIN_H__
    #error "include 'stdafx.h' before including this file for PCH"
#endif

#include "resource.h"       // main symbols


// CWriteSNT8300App:
// See WriteSNT8300.cpp for the implementation of this class
//

class CWriteSNT8300App : public CWinApp
{
public:
    CWriteSNT8300App();

// Overrides
public:
    virtual BOOL InitInstance();

// Implementation

    DECLARE_MESSAGE_MAP()
};

extern CWriteSNT8300App theApp;