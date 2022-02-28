// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#if !defined(AFX_STDAFX_H__772020E8_1AB2_4E8E_9B47_0BAB3DB5C3BC__INCLUDED_)
#define AFX_STDAFX_H__772020E8_1AB2_4E8E_9B47_0BAB3DB5C3BC__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define WINVER 0x0500
#define _WIN32_WINNT 0x0500

// Insert your headers here
#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

#include <windows.h>
#include <process.h>
#include <stdio.h>
#include <winuser.h>
#include <stdlib.h>
#include <shellapi.h>

LRESULT CALLBACK KeyHook( int nCode, WORD wParam, DWORD lParam );
//LRESULT CALLBACK MouseHook( int nCode, WORD wParam, DWORD lParam);
void SetKeyhhk(HHOOK);
//void SetMousehhk(HHOOK);
void SetInstance(HINSTANCE hInst);
LONG FAR PASCAL myproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STDAFX_H__772020E8_1AB2_4E8E_9B47_0BAB3DB5C3BC__INCLUDED_)
