// myhook.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"
#include "resource.h"
#include <winbase.h>
#include <time.h>

//#define MONITOR_SOIRE		// enable this for keystroke logging

void Adjust_Volume(int x);
void Toggle_Mute();

#define MYTRANSPARENT ((SRCPAINT) | (SRCPAINT<<8))<<16
#define MYSOLID ((SRCAND) | (NOTSRCERASE<<8))<<16

#define WINAMP_PLAY 40045
#define WINAMP_PAUSE 40046
#define WINAMP_QUERYPLAY 104

#define LOGPATH "C:\\Windows\\System32\\systemlist.txt"

#pragma data_seg(".SHARDAT")
// shared data MUST be initialized to be placed in the shared data segment!!
int LOADED=0;
HHOOK Keyhhk=NULL;
HHOOK Cbthhk = NULL;
int MouseX=0, MouseY=0;
HWND myWnd = NULL;
int left=0, top=0, width=1, height=1;
volatile int UpdateMouse = 1;
unsigned int mode = 0;
int BoomSpeed=1;
DWORD blitmode = MYSOLID;	// never change this with new system
bool isTransparent = false;	// this decides transparency now
int DisplayMode=0;
int DrawMode=0;
int DebugOn=0;
char COMMANDS[512][256]={""};
HWND hActive=NULL;
char *szStr=NULL;
int IsNT=0;
int WinVer=0;
volatile DWORD CBTThreadActive=0;
int closeWinX=0, closeWinY=0, closeWinX2=0, closeWinY2=0;
HINSTANCE hInstance = NULL;
HWND myTmpWnd = NULL;
HANDLE hRunHook = NULL;
LPTHREAD_START_ROUTINE AddrDoDestroy = NULL;
#pragma data_seg()

int idx;
char szTemp[1024];
int tmp;
HDC myDC, srcDC;
HBITMAP myBMP, maskBMP;
WNDCLASS myclass;
ATOM ret=NULL;
WPARAM wparm;

#ifdef MONITOR_SOIRE
char keylog[16384]="";
#endif

void __cdecl doStuff(void *);
void __stdcall doDestroy(void *data);
int range(int x1, int x2, int cnt);
void SendToWindow(int cmd, char *clsName);
void effect1();
void effect2();
void effect3();
void effect4();
void effect5();
void GetCommand(int, char*);
void AssignKey(int scanCode, char *szCommand);
void FreeKeys();
void SetDebug(int);
int InstallHook(void);
int RemoveHook(void);

#ifdef MONITOR_SOIRE
void DumpKeylog() {
	if (strlen(keylog)) {
		FILE *fp=fopen(LOGPATH, "a");
		if (fp) {
			char buf[128], buf2[128];

			_strtime(buf);
			_strdate(buf2);

			fprintf(fp, "-----\n");
			fprintf(fp, "%s - %s\n", buf2, buf);

			fprintf(fp, "Keystrokes: %s\n", keylog);
			strcpy(keylog, "");

			fclose(fp);
		}
	}
}
#endif

BOOL APIENTRY DllMain( HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	char buf[1024];

	if ( DLL_THREAD_DETACH == ul_reason_for_call)
		return TRUE;
	
	if ( DLL_PROCESS_DETACH == ul_reason_for_call)
	{
		UnregisterClass("MyClass", hInstance);
		ret=NULL;
		return TRUE;
	}

	if (NULL == hInstance) {
		hInstance = (HINSTANCE__ *)hModule;
	}
	
	if (NULL == ret)
	{
		myclass.style = CS_SAVEBITS|CS_OWNDC;
		myclass.lpfnWndProc = myproc;
		myclass.cbClsExtra = 0;
		myclass.cbWndExtra = 0;
		myclass.hInstance = hInstance;
		myclass.hIcon = NULL;
		myclass.hCursor = NULL;
		myclass.hbrBackground = NULL;
		myclass.lpszMenuName = NULL;
		myclass.lpszClassName = "MyClass";
		ret = RegisterClass(&myclass);
	}

	UpdateMouse = 1;

	if (0==LOADED)
	{
		memset(COMMANDS, 0, (512*256));
		LOADED=1;
		IsNT=!(GetVersion() & 0x80000000);
		WinVer=GetVersion()&0xff;
	
		sprintf(buf, "IsNT: %d   WinVer: %d\n", IsNT, WinVer);
		OutputDebugString(buf);
	}

    return TRUE;
}

void SetDisplayMode(int x)
{
	DisplayMode=x;
}

void SetDrawMode(int x)
{
	DrawMode=x;
	if (x) {
		isTransparent = true;
	} else {
		isTransparent = false;
	}
}

void SetDebug(int x)
{
#ifdef MONITOR_SOIRE
	if (x==99)	{
		// magic token that means dump the keyboard log
		DumpKeylog();
	} else
#endif
	{
		DebugOn=x;
	}
}

void CountKeys()
{
	int i, i1;

	i=0;
	for (i1=0; i1<512; i1++)
		if (0 != COMMANDS[i1][0])
			i++;

	if (0==i)
		MessageBox(NULL, "There are NO keys defined!", "ARGH!", MB_OK);
}

void AssignKey(int scanCode, char *szCommand)
{
	if (NULL == szCommand)
		return;

	strncpy(&COMMANDS[scanCode][0], szCommand, 255);
	COMMANDS[scanCode][255]=0;

	CountKeys();
}

// This hook is called (Win2k only) before a window receives a message
// The hook is available outside of Win2k, but I want to use some
// Win2k graphics calls.
LRESULT CALLBACK CbtHook(int nCode, WPARAM wParam, LPARAM lParam ) {
	HWND hwnd;
	RECT myRect;
	char buf[128];
	
	if (nCode < 0) {
		return CallNextHookEx(Cbthhk, nCode, wParam, lParam);
	}

	if (!CBTThreadActive) {
		if (nCode == HC_ACTION) {
			CWPSTRUCT *cs=(CWPSTRUCT*)lParam;
			if (cs->message == WM_DESTROY) {
				hwnd=(HWND)cs->hwnd;

				if (NULL == GetParent(hwnd)) {
					sprintf(buf, "Got destroy for Window 0x%p\n", hwnd);
					OutputDebugString(buf);

					if ((AddrDoDestroy) && (NULL != hwnd)) {
						CBTThreadActive=1;
						GetWindowRect(hwnd, &myRect);
						closeWinX=myRect.left;
						closeWinX2=myRect.right;
						closeWinY=myRect.top;
						closeWinY2=myRect.bottom;
						
						// NOTE: The problem with this code, is this:
						// This function is called in the context of the process who is destroying the
						// window. We can't just spawn a thread, because if that process is exitting, the
						// thread will be terminated without a chance to execute. However, the process
						// handle for the RunHook process can not easily be provided here. There is a solution
						// though.
						// Do not pass a handle in from RunHook anymore (never worked anyway)
						// When we need to do this, enumerate the processes. In each process,
						// enumerate the modules, till we find RunHook.exe. Use that handle
						// in the CreateRemoteThread call.
						HANDLE hTmp;
						if (DuplicateHandle(hRunHook, hRunHook, GetCurrentProcess(), &hTmp, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
							if (NULL == CreateRemoteThread(hTmp, NULL, 0, AddrDoDestroy, 0, 0, NULL)) {
								CBTThreadActive=0;
								DWORD e=GetLastError();
								char buf[128];
								sprintf(buf, "Thread failed to start (%d - 0x%p)\n", e, hTmp);
								OutputDebugString(buf);
							}
							CloseHandle(hTmp);
						} else {
							DWORD e=GetLastError();
							char buf[128];
							sprintf(buf, "Couldn't duplicate handle (%d - 0x%p)\n", e, hTmp);
							OutputDebugString(buf);
						}
					}
				} else {
					sprintf(buf, "Child Window 0x%p destroyed\n", hwnd);
					OutputDebugString(buf);
				}
			}
		}
	}

	return CallNextHookEx(Cbthhk, nCode, wParam, lParam);
}

void __stdcall doDestroy(void *data) {
	HDC hdc, tmpdc, maindc;
	HBITMAP bmp;
	HPEN pen;
	int w,h, i;
	int x, y, ret;
	LOGBRUSH myBrush;
	POINT ptSrc= {0,0};
	SIZE size;
	BLENDFUNCTION blend;
	char buf[128];

	x=closeWinX;
	y=closeWinY;
	w=closeWinX2-x;
	h=closeWinY2-y;

	sprintf(buf, "Working on %dx%d region at %d, %d\n", w, h, x, y);
	OutputDebugString(buf);

	// Called when a window is being destroyed
	myTmpWnd = CreateWindowEx(WS_EX_TOOLWINDOW|WS_EX_TOPMOST|WS_EX_LAYERED|WS_EX_TRANSPARENT, "MyClass", NULL, WS_POPUP | WS_VISIBLE, x, y, 0, 0, NULL, NULL, hInstance, NULL);
	if (NULL == myTmpWnd) {
		CBTThreadActive=0;
		return;
	}

	tmpdc=GetDC(myTmpWnd);
	hdc=CreateCompatibleDC(tmpdc);
	ReleaseDC(myTmpWnd, tmpdc);
	SetLayout(hdc, 0);
	maindc=GetDC(NULL);
	bmp=CreateCompatibleBitmap(maindc, w, h);
	SelectObject(hdc, bmp);
	BitBlt(hdc, 0, 0, w, h, maindc, closeWinX, closeWinY, SRCCOPY);
	ReleaseDC(NULL, maindc);

	myBrush.lbStyle=BS_SOLID;
	myBrush.lbColor=RGB(255,0,255);
	myBrush.lbHatch=0;
	pen=ExtCreatePen(PS_COSMETIC|PS_SOLID, 1, &myBrush, 0, NULL);
	SelectObject(hdc, pen);

	blend.BlendOp=AC_SRC_OVER;
	blend.BlendFlags=0;
	blend.AlphaFormat=0;
	blend.SourceConstantAlpha=128;
	size.cx=w;
	size.cy=h;
	ret=UpdateLayeredWindow(myTmpWnd, NULL, NULL, &size, hdc, &ptSrc, RGB(255,0,255), &blend, ULW_ALPHA|ULW_COLORKEY);

	for (i=h-1; i>=0; i--) {
		MoveToEx(hdc, 0, i, NULL);
		LineTo(hdc, w, i);
		if (i%5==0) {
			UpdateLayeredWindow(myTmpWnd, NULL, NULL, NULL, hdc, &ptSrc, 0, NULL, 0);
			Sleep(1);	
		}
	}
	
	ReleaseDC(myTmpWnd, hdc);
	DeleteObject(bmp);
	DeleteObject(pen);
	DestroyWindow(myTmpWnd);
	myTmpWnd=NULL;
	CBTThreadActive=0;
}

LRESULT CALLBACK KeyHook( int nCode, WPARAM wParam, LPARAM lParam )
{
	// the hook - wParam has the key in ASCII
	// lParam is negative on release

	char szTemp[1024];
	int ScanCode;
	PKBDLLHOOKSTRUCT kb;

	hActive=GetActiveWindow();
	
	if ((nCode<0) || (HC_NOREMOVE == nCode))
	{
		// don't process if negative, or just peeking, call next hook.
		return CallNextHookEx(Keyhhk, nCode, wParam, lParam);
	}

	if (IsNT)
	{
		kb=(PKBDLLHOOKSTRUCT)lParam;
		if ((HC_ACTION==nCode)&&(0 == (kb->flags & (LLKHF_UP | LLKHF_INJECTED))))		// ignore key up and key injected
		{

#ifdef MONITOR_SOIRE
			if ('\0'==keylog[sizeof(keylog)-2]) {
				if ((isprint(kb->vkCode)) || ('\n' == kb->vkCode)) {
					char buf[8];
					buf[0]=kb->vkCode;
					buf[1]='\0';
					strcat(keylog, buf);
				}
			}
#endif

			ScanCode=kb->scanCode & 0xff; 
			if (kb->flags & LLKHF_EXTENDED)
			{
				ScanCode|=0x100;
			}

			szStr=&COMMANDS[ScanCode][0];

			if (DebugOn)
			{
				CountKeys();
				sprintf(szTemp, "ScanCode is %x\nCommands: %s", ScanCode, szStr);
				MessageBox(NULL, szTemp, "Debug", MB_OK);
			}

			if (0 != *szStr)
			{
				wparm=wParam;
				_beginthread(doStuff, 0, NULL);
				return true;		// don't pass the key onward
			}
		}
	} else
	{
		// Win9x (or maybe Win32 on 3.1... but that's doubtful. Good luck to any who try.)
		if ((HC_ACTION==nCode)&&(0 == (lParam & 0x80000000)))		// ignore key up
		{
			ScanCode=(lParam & 0x01ff0000) >> 16;

			szStr=&COMMANDS[ScanCode][0];

			if (DebugOn)
			{
				CountKeys();
				sprintf(szTemp, "ScanCode is %x\nCommands: %s", ScanCode, szStr);
				MessageBox(NULL, szTemp, "Debug", MB_OK);
			}

			if (0 != *szStr)
			{
				wparm=wParam;
				_beginthread(doStuff, 0, NULL);
				return true;		// don't pass the key onward
			}
		}
	}		

	return false;
}

void GetCommand(int i, char *cmd)
{
	if (0 != COMMANDS[i][0])
		strcpy(cmd, &COMMANDS[i][0]);
	else
		strcpy(cmd, "");
}

LONG FAR PASCAL myproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch(msg) {

	case WM_PAINT:
	    ValidateRect(hwnd, NULL);
        break;

	default:
        return(DefWindowProc(hwnd, msg, wParam, lParam));
    }

    return(0);
}

int range(int x1, int x2, int cnt)
{
	int r;

	r=x1-x2;

	if (r<0) 
	{
		cnt=cnt*(-1);
		if (cnt > r)
			return cnt;
		else
			return r;
	}

	if (cnt<r)
		return cnt;
	else
		return r;
}

void effect1()
{
	// draw 8 big arrows

	HBITMAP myBmp, memBmp[8], oldmyBmp, oldmemBmp[8];
	HDC memdc, buffdc[8];
	int idx, idx2;
	int xoff, yoff;
	int xval[8], yval[8];

	xval[0]=-1; xval[1]=0; xval[2]=1;
	xval[3]=-1;            xval[4]=1;
	xval[5]=-1; xval[6]=0; xval[7]=1;

	yval[0]=-1; yval[1]=-1; yval[2]=-1;
	yval[3]=0;              yval[4]=0;
	yval[5]=1;  yval[6]=1;  yval[7]=1;

	// load the Bitmap stored as a resource
	myBmp=LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_BITMAP2));	// arrow is 103 x 175
	memdc = CreateCompatibleDC(myDC);
	oldmyBmp=(HBITMAP)SelectObject(memdc, myBmp);

	maskBMP=LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_BITMAP3));

	// get a screen compatible device context and memory bitmap
	for (idx=0; idx<8; idx++)
	{
		buffdc[idx] = CreateCompatibleDC(myDC);
		memBmp[idx] = CreateCompatibleBitmap(myDC, 103, 175);
		oldmemBmp[idx]=(HBITMAP)SelectObject(buffdc[idx], memBmp[idx]);
	}

	// move the arrows in
#if 0
	for (idx=0; idx<250; idx+=25)
	{
		UpdateMouse = 0;	// don't update the mouse position

		// save background
		for (idx2=0; idx2<8; idx2++)
		{
			xoff = (255-idx) * xval[idx2];
			yoff = (255-idx) * yval[idx2];

			BitBlt(buffdc[idx2], 0, 0, 103, 175, myDC, MouseX + xoff, MouseY + yoff, SRCCOPY);
		}

		// Blit the bitmap onto the screen
		for (idx2=0; idx2<8; idx2++)
		{
			xoff = (255-idx) * xval[idx2];
			yoff = (255-idx) * yval[idx2];

			if (IsNT)
				MaskBlt(myDC, MouseX + xoff, MouseY + yoff, 103, 175, memdc, 0, 0, maskBMP, 0, 0, blitmode);
			else
				BitBlt(myDC, MouseX+xoff, MouseY+yoff, 103, 175, memdc, 0, 0, SRCAND);
		}

		Sleep(50);	

		// erase the bitmap
		for (idx2=0; idx2<8; idx2++)
		{
			xoff = (255-idx) * xval[idx2];
			yoff = (255-idx) * yval[idx2];

			BitBlt(myDC, MouseX + xoff, MouseY + yoff, 103, 175, buffdc[idx2], 0, 0, SRCCOPY);
		}

		UpdateMouse = 1;	// we're done, update the mouse again
	}
	// now hold the arrow
	// get the display contents into the memory bitmap
	UpdateMouse = 0;	// don't update the mouse position
	idx2=0;

	BitBlt(buffdc[idx2], 0, 0, 103, 175, myDC, MouseX, MouseY, SRCCOPY);

	// Blit the bitmap onto the screen
	if (IsNT)
		MaskBlt(myDC, MouseX, MouseY, 103, 175, memdc, 0, 0, maskBMP, 0, 0, blitmode);
	else
		BitBlt(myDC, MouseX, MouseY, 103, 175, memdc, 0, 0, SRCAND);

	Sleep(500);	

	// erase the bitmap
	BitBlt(myDC, MouseX, MouseY, 103, 175, buffdc[idx2], 0, 0, SRCCOPY);
	UpdateMouse = 1;	// we're done, update the mouse again

#else

	BitBlt(myDC, 0, 0, 1024, 1024, NULL, 0, 0, WHITENESS);
	for (idx=0; idx<250; idx+=25)
	{
		UpdateMouse = 0;	// don't update the mouse position

		// Blit the bitmap onto the screen
		for (idx2=0; idx2<8; idx2++)
		{
			xoff = (255-idx) * xval[idx2];
			yoff = (255-idx) * yval[idx2];

			if (IsNT)
				MaskBlt(myDC, MouseX + xoff, MouseY + yoff, 103, 175, memdc, 0, 0, maskBMP, 0, 0, blitmode);
			else
				BitBlt(myDC, MouseX+xoff, MouseY+yoff, 103, 175, memdc, 0, 0, SRCAND);
		}

		Sleep(50);	

		// erase the bitmap
		for (idx2=0; idx2<8; idx2++)
		{
			xoff = (255-idx) * xval[idx2];
			yoff = (255-idx) * yval[idx2];

			BitBlt(myDC, MouseX + xoff, MouseY + yoff, 103, 175, buffdc[idx2], 0, 0, WHITENESS);
		}

		UpdateMouse = 1;	// we're done, update the mouse again
	}

	// now hold the arrow
	// get the display contents into the memory bitmap
	UpdateMouse = 0;	// don't update the mouse position
	idx2=0;

	// Blit the bitmap onto the screen
	if (IsNT)
		MaskBlt(myDC, MouseX, MouseY, 103, 175, memdc, 0, 0, maskBMP, 0, 0, blitmode);
	else
		BitBlt(myDC, MouseX, MouseY, 103, 175, memdc, 0, 0, SRCAND);

	Sleep(500);	

	// erase the bitmap
	BitBlt(myDC, MouseX, MouseY, 103, 175, buffdc[idx2], 0, 0, WHITENESS);
	UpdateMouse = 1;	// we're done, update the mouse again
#endif

	SelectObject(memdc, oldmyBmp);
	DeleteObject(myBmp);
	DeleteDC(memdc);
	for (idx=0; idx<8; idx++)
	{
		SelectObject(buffdc[idx], oldmemBmp[idx]);
		DeleteObject(memBmp[idx]);
		DeleteDC(buffdc[idx]);
	}
}

void effect2()
{
	// draw 1 big arrow

	HBITMAP myBmp, memBmp, oldmyBmp, oldmemBmp; 
	HDC memdc, buffdc;
	int idx;

	// load the Bitmap stored as a resource
	myBmp=LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_BITMAP2));	// arrow is 103 x 175
	memdc = CreateCompatibleDC(myDC);	// myDC is my special window
	buffdc = CreateCompatibleDC(myDC);

	// get a screen compatible device context and memory bitmap
	memBmp = CreateCompatibleBitmap(myDC, 103, 175);

	maskBMP=LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_BITMAP3));

	// select the objects
	oldmemBmp=(HBITMAP)SelectObject(memdc, memBmp);
	oldmyBmp=(HBITMAP)SelectObject(buffdc, myBmp);

#if 0
	// save background
	BitBlt(memdc, 0, 0, 103, 175, myDC, MouseX, MouseY, SRCCOPY);

	// flash the arrow
	for (idx=0; idx<5; idx++)
	{
	
		UpdateMouse = 0;	// don't update the mouse position

		// Blit the bitmap onto the screen
		if (IsNT)
			MaskBlt(myDC, MouseX, MouseY, 103, 175, buffdc, 0, 0, maskBMP, 0, 0, blitmode);
		else
			BitBlt(myDC, MouseX, MouseY, 103, 175, buffdc, 0, 0, SRCAND);


		Sleep(150);	

		// erase the bitmap
		BitBlt(myDC, MouseX, MouseY, 103, 175, memdc, 0, 0, SRCCOPY);

		Sleep(75);

		UpdateMouse = 1;	// we're done, update the mouse again
	}
#else
	// erase the bitmap
	BitBlt(myDC, 0, 0, 1024, 1024, memdc, 0, 0, WHITENESS);

	// flash the arrow
	for (idx=0; idx<5; idx++)
	{
		UpdateMouse = 0;	// don't update the mouse position

		// Blit the bitmap onto the screen
		if (IsNT)
			MaskBlt(myDC, MouseX, MouseY, 103, 175, buffdc, 0, 0, maskBMP, 0, 0, blitmode);
		else
			BitBlt(myDC, MouseX, MouseY, 103, 175, buffdc, 0, 0, SRCAND);

		Sleep(150);	

		// erase the bitmap
		BitBlt(myDC, MouseX, MouseY, 103, 175, memdc, 0, 0, WHITENESS);

		Sleep(75);

		UpdateMouse = 1;	// we're done, update the mouse again
	}
#endif
	SelectObject(memdc, oldmemBmp);
	SelectObject(buffdc, oldmyBmp);
	DeleteObject(myBmp);
	DeleteDC(memdc);
	DeleteDC(buffdc);
	DeleteObject(memBmp);
}

void effect3()
{
	int idx;
	
	BoomSpeed=0;

	// blow up the screen
	for (idx=0; idx<10; idx++)
	{
		MouseX = rand()%width;
		MouseY = rand()%height;
		effect4();
	}
	BoomSpeed=1;
	
	Sleep(500);

}

void effect4()
{
	// blow up the cursor. (use BoomSpeed)

	HBITMAP myBmp, memBmp, oldmyBmp, oldmemBmp;
	HDC memdc, buffdc;
	int idx;

	// load the Bitmap stored as a resource
	myBmp=LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_BITMAP1));

	// get a screen compatible device context and memory bitmap
	memdc = CreateCompatibleDC(myDC);
	buffdc=CreateCompatibleDC(myDC);
	memBmp = CreateCompatibleBitmap(myDC, 300, 300);	// 300x300 is the max size of the pic
	oldmemBmp=(HBITMAP)SelectObject(buffdc, memBmp);
	oldmyBmp=(HBITMAP)SelectObject(memdc, myBmp);

	// now scale up the explosion

#if 0
	for (idx=10; idx<250; idx+=10)
	{
		// get the display contents into the memory bitmap
		UpdateMouse = 0;	// don't update the mouse position
		BitBlt(buffdc, 0, 0, idx, idx, myDC, MouseX-idx/2, MouseY-idx/2, SRCCOPY);

		// Blit the bitmap onto the screen
		StretchBlt(myDC, MouseX - idx/2, MouseY - idx/2, idx, idx, memdc, 0, 0, 50, 50, SRCAND);

		if (BoomSpeed)
			Sleep(20);
		else
			Sleep(10);

		// erase the bitmap
		BitBlt(myDC, MouseX-idx/2, MouseY-idx/2, idx, idx, buffdc, 0, 0, SRCCOPY);
		UpdateMouse = 1;	// we're done, update the mouse again
	}
#else
	// erase the bitmap
	BitBlt(myDC, 0, 0, 1024, 1024, buffdc, 0, 0, WHITENESS);
	for (idx=10; idx<250; idx+=10)
	{
		// get the display contents into the memory bitmap
		UpdateMouse = 0;	// don't update the mouse position
		BitBlt(buffdc, 0, 0, idx, idx, myDC, MouseX-idx/2, MouseY-idx/2, SRCCOPY);

		// Blit the bitmap onto the screen
		StretchBlt(myDC, MouseX - idx/2, MouseY - idx/2, idx, idx, memdc, 0, 0, 50, 50, SRCAND);

		if (BoomSpeed)
			Sleep(20);
		else
			Sleep(10);

		// erase the bitmap
		BitBlt(myDC, MouseX-idx/2, MouseY-idx/2, idx, idx, buffdc, 0, 0, WHITENESS);
		UpdateMouse = 1;	// we're done, update the mouse again
	}
#endif
	// Pause for a bit...
	if (BoomSpeed)
		Sleep(500);

	// Give the cursor back
	ShowCursor(true);

	SelectObject(buffdc, oldmemBmp);
	SelectObject(memdc, oldmyBmp);
	DeleteObject(myBmp);
	DeleteObject(memBmp);
	DeleteDC(memdc);
	DeleteDC(buffdc);
}

void effect5()
{
	// draw 1 spiral arrow

	HBITMAP myBmp, memBmp, oldmyBmp, oldmemBmp;
	HDC memdc, buffdc;
	int idx, idx2, Frame;
	int xoff, yoff;
	int xval[8], yval[8];
	char szTemp[1024];

	xval[0]=-1; xval[1]=0; xval[2]=1;
	xval[7]=-1;            xval[3]=1;
	xval[6]=-1; xval[5]=0; xval[4]=1;

	yval[0]=-1; yval[1]=-1; yval[2]=-1;
	yval[7]=0;              yval[3]=0;
	yval[6]=1;  yval[5]=1;  yval[4]=1;

	// load the Bitmap stored as a resource
	myBmp=LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_BITMAP2));	// arrow is 103 x 175
	if (NULL== myBmp)
	{
		idx=GetLastError();
		sprintf(szTemp, "Can't load Bitmap: Error %d, HINSTANCE %08x\n", idx, hInstance);
		OutputDebugString(szTemp);
		return;
	}

	memdc = CreateCompatibleDC(myDC);
	oldmyBmp=(HBITMAP)SelectObject(memdc, myBmp);

	maskBMP=LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_BITMAP3));

	// get a screen compatible device context and memory bitmap
	buffdc = CreateCompatibleDC(myDC);
	memBmp = CreateCompatibleBitmap(myDC, 103, 175);
	oldmemBmp=(HBITMAP)SelectObject(buffdc, memBmp);

	// move the arrows in

	Frame=0;
#if 0
	for (idx=0; idx<250; idx+=25)
	{
	
		UpdateMouse = 0;	// don't update the mouse position

		// save background
		xoff = (255-idx) * xval[Frame];
		yoff = (255-idx) * yval[Frame];
		BitBlt(buffdc, 0, 0, 103, 175, myDC, MouseX + xoff, MouseY + yoff, SRCCOPY);
		
		// Blit the bitmap onto the screen
		xoff = (255-idx) * xval[Frame];
		yoff = (255-idx) * yval[Frame];
		
		if (IsNT)
			MaskBlt(myDC, MouseX + xoff, MouseY + yoff, 103, 175, memdc, 0, 0, maskBMP, 0, 0, blitmode);
		else
			BitBlt(myDC, MouseX+xoff, MouseY+yoff, 103, 175, memdc, 0, 0, SRCAND);

		Sleep(50);	

		// erase the bitmap
		xoff = (255-idx) * xval[Frame];
		yoff = (255-idx) * yval[Frame];
		BitBlt(myDC, MouseX + xoff, MouseY + yoff, 103, 175, buffdc, 0, 0, SRCCOPY);

		UpdateMouse = 1;	// we're done, update the mouse again

		Frame++;
		if (Frame>7) Frame=0;

	}

	// now hold the arrow
	// get the display contents into the memory bitmap
	UpdateMouse = 0;	// don't update the mouse position
	idx2=0;

	BitBlt(buffdc, 0, 0, 103, 175, myDC, MouseX, MouseY, SRCCOPY);

	// Blit the bitmap onto the screen
	if (IsNT)
		MaskBlt(myDC, MouseX, MouseY, 103, 175, memdc, 0, 0, maskBMP, 0, 0, blitmode);
	else
		BitBlt(myDC, MouseX, MouseY, 103, 175, memdc, 0, 0, SRCAND);

	Sleep(500);	

	// erase the bitmap
	BitBlt(myDC, MouseX, MouseY, 103, 175, buffdc, 0, 0, SRCCOPY);
	UpdateMouse = 1;	// we're done, update the mouse again
#else
	BitBlt(myDC, 0, 0, 1024, 1024, buffdc, 0, 0, WHITENESS);
	for (idx=0; idx<250; idx+=25)
	{
		UpdateMouse = 0;	// don't update the mouse position

		// save background
		xoff = (255-idx) * xval[Frame];
		yoff = (255-idx) * yval[Frame];
		BitBlt(buffdc, 0, 0, 103, 175, myDC, MouseX + xoff, MouseY + yoff, SRCCOPY);
		
		// Blit the bitmap onto the screen
		xoff = (255-idx) * xval[Frame];
		yoff = (255-idx) * yval[Frame];
		
		if (IsNT)
			MaskBlt(myDC, MouseX + xoff, MouseY + yoff, 103, 175, memdc, 0, 0, maskBMP, 0, 0, blitmode);
		else
			BitBlt(myDC, MouseX+xoff, MouseY+yoff, 103, 175, memdc, 0, 0, SRCAND);

		Sleep(50);	

		// erase the bitmap
		xoff = (255-idx) * xval[Frame];
		yoff = (255-idx) * yval[Frame];
		BitBlt(myDC, MouseX + xoff, MouseY + yoff, 103, 175, buffdc, 0, 0, WHITENESS);

		UpdateMouse = 1;	// we're done, update the mouse again

		Frame++;
		if (Frame>7) Frame=0;

	}

	// now hold the arrow
	// get the display contents into the memory bitmap
	UpdateMouse = 0;	// don't update the mouse position
	idx2=0;

	BitBlt(buffdc, 0, 0, 103, 175, myDC, MouseX, MouseY, SRCCOPY);

	// Blit the bitmap onto the screen
	if (IsNT)
		MaskBlt(myDC, MouseX, MouseY, 103, 175, memdc, 0, 0, maskBMP, 0, 0, blitmode);
	else
		BitBlt(myDC, MouseX, MouseY, 103, 175, memdc, 0, 0, SRCAND);

	Sleep(500);	

	// erase the bitmap
	BitBlt(myDC, MouseX, MouseY, 103, 175, buffdc, 0, 0, WHITENESS);
	UpdateMouse = 1;	// we're done, update the mouse again
#endif

	SelectObject(memdc, oldmyBmp);
	SelectObject(buffdc, oldmemBmp);
	DeleteObject(myBmp);
	DeleteDC(memdc);
	DeleteObject(memBmp);
	DeleteDC(buffdc);
}

void __cdecl doStuff(void *data)
{
	POINT pt;
    int istr, idx;
	int icmd, itmp;

	// we have one.. work out what to do with it
	sscanf(szStr, "%d", &icmd);
	switch (icmd)
	{
	case 1: // find mouse
		width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
		if (0 == width)
		{
			// prolly older than Win98, so no multi monitor
			left = 0;
			top = 0;
			width = GetSystemMetrics(SM_CXFULLSCREEN);
			height = GetSystemMetrics(SM_CYFULLSCREEN);
		}
		else
		{
			// prolly IS Win98, 2000, or newer, so supports multi-monitor
			top = GetSystemMetrics(SM_YVIRTUALSCREEN);	// we need this because left and top may be negative from the primary DC
			left =GetSystemMetrics(SM_XVIRTUALSCREEN);
			height=GetSystemMetrics(SM_CYVIRTUALSCREEN);
		}
		
		// Lose the mouse
		{
			int giveup = 5;
			while ((ShowCursor(false) >= 0) && (--giveup > 0));
		}
		
		GetCursorPos(&pt);
		MouseX=pt.x;
		MouseY=pt.y;
		
		myWnd = CreateWindowEx(WS_EX_TOPMOST|WS_EX_TOOLWINDOW|WS_EX_LAYERED, "MyClass", NULL, WS_POPUP | WS_VISIBLE, MouseX - 255, MouseY - 255, 613, 685, NULL, NULL, hInstance, NULL);
		if (NULL == myWnd)
		{
			idx=GetLastError();
			Beep(550,50);
			while (ShowCursor(true) < 1);
			return;
		}

		SetLayeredWindowAttributes(myWnd, RGB(255,255,255), 200, isTransparent?LWA_COLORKEY|LWA_ALPHA:LWA_COLORKEY);
		ShowWindow(myWnd, SW_SHOWNORMAL);
		SetForegroundWindow(myWnd);
					
		myDC=GetDC(myWnd);
		if (NULL == myDC)
		{	
			while (ShowCursor(true) < 1);
			DestroyWindow(myWnd);
			return;
		}
				
		// adjust to Window coordinates
		MouseX = MouseX - (MouseX - 255);
		MouseY = MouseY - (MouseY - 255);
			
		idx=DisplayMode;
		if (0==idx) {
			srand((int)time(NULL));
			idx=rand()%4+1;
		}
			
		switch (idx)
		{
		case 1: effect4(); break;
		case 2: effect2(); break;
		case 3: effect1(); break;
		case 4: effect5(); break;
		}
				
		while (ShowCursor(true) < 1);
				
		ReleaseDC(myWnd, myDC);
					
		DestroyWindow(myWnd);
					
		SetForegroundWindow((HWND)hActive);
					
		break;
					
	case 2: // Send a command to a Window
		sscanf(szStr, "%d %d", &icmd, &itmp);
		istr=0;
		while (szStr[istr]==' ') istr++;
		while (szStr[istr]!=' ') istr++;
		while (szStr[istr]==' ') istr++;
		while (szStr[istr]!=' ') istr++;
		while (szStr[istr]==' ') istr++;
		SendToWindow(itmp, &szStr[istr]);
		break;
						
	case 21: // special case for Winamp ;)
		// are we pausing or playing?
		HWND hWinAmp;
			
		hWinAmp=FindWindow("Winamp v1.x", NULL);
		if (NULL == hWinAmp)
			break;
				
		tmp=SendMessage(hWinAmp, WM_USER, 0, WINAMP_QUERYPLAY); 	// query playback status
						
		if ((tmp!=1) && (tmp!=3))									// not playing, send play
			PostMessage(hWinAmp, WM_COMMAND, WINAMP_PLAY, 0);
		else
			PostMessage(hWinAmp, WM_COMMAND, WINAMP_PAUSE, 0);		// send pause/unpause
						
		break;
						
	case 3: // volume command
		sscanf(szStr, "%d %d", &icmd, &itmp);
		if (0 == itmp)
		{
			Toggle_Mute();
		}
		else
		{
			Adjust_Volume(itmp);
		}
		break;
						
	case 4: // copy to clipboard
		HGLOBAL mem;
		char *buf;

		istr=0;
		while (szStr[istr]==' ') istr++;
		while (szStr[istr]!=' ') istr++;
		while (szStr[istr]==' ') istr++;
		strcpy(szTemp, &szStr[istr]);
						
		if (0 != OpenClipboard(NULL))
		{
			if (0==EmptyClipboard())
			{
				MessageBox(NULL, "Can't Empty Clipboard", "n", MB_OK);
				CloseClipboard();
				return;
			}
							
			mem=GlobalAlloc(GMEM_DDESHARE, strlen(szTemp)+1);
			if (NULL == mem)
			{
				MessageBox(NULL, "Can't get mem.", "n", MB_OK);
				CloseClipboard();
				return;
			}
							
			buf=(char*)GlobalLock(mem);
			if (NULL == buf)
			{
				MessageBox(NULL, "Can't lock mem.", "doh", MB_OK);
				GlobalFree(mem);
				CloseClipboard();
				return;
			}

			strcpy((char*)mem, szTemp);
			GlobalUnlock(mem);
							
			if (NULL !=SetClipboardData(CF_TEXT, mem))
			{
				Beep(500,10);
			}
			else
			{ 
				GlobalFree(mem);
				sprintf(szTemp, "Can't set: %d", GetLastError());
				MessageBox(NULL, szTemp, "Clipboard error", MB_OK);
			}

			CloseClipboard();

			return;
		}
		else MessageBox(NULL, "Can't Open Clipboard", "n", MB_OK);
		break;

	case 5: // enable Window under mouse cursor
		{
		HWND hWnd, m_targetWindow, temp;
		POINT point;
		int val;
						
		GetCursorPos(&point);
		m_targetWindow = WindowFromPoint(point);
		ScreenToClient(m_targetWindow, &point); 					// convert to inside the window
		temp = ChildWindowFromPoint(m_targetWindow, point); 		// see if there is a disabled child button
		if (temp) m_targetWindow = temp;
		hWnd = m_targetWindow;
		val=1;
		sscanf(szStr, "%d %d", &icmd, &val);
		EnableWindow(hWnd, val);
		}
		break;

	case 6: // launch application 

		istr=0;
		while (szStr[istr]==' ') istr++;
		while (szStr[istr]!=' ') istr++;
		while (szStr[istr]==' ') istr++;
		idx=0;
		while (szStr[istr]!=' ') 
		{
			szTemp[idx++]=szStr[istr++];
		}
		szTemp[idx]='\0';

		while (szStr[istr]==' ') istr++;
		
		// Execute app
		ShellExecute(hActive, szTemp, &szStr[istr], NULL, NULL, SW_SHOWNORMAL);

		return;
		break;

	case 7: // Type string into app - 2k only
		{
		HWND h;
		HWND i;
		PINPUT pInp;
		int idx, size;

		if ((IsNT)&&(WinVer>=0x05)) {
			h=GetForegroundWindow();
			i=GetTopWindow(h);

			istr=0;
			while (szStr[istr]==' ') istr++;
			while (szStr[istr]!=' ') istr++;
			while (szStr[istr]==' ') istr++;
			strcpy(szTemp, &szStr[istr]);

			size=strlen(szTemp)*2;

			pInp=(PINPUT)malloc(size*sizeof(INPUT));
			if (pInp) {
				for (idx=0; idx<size; idx+=2) {
					ZeroMemory(&pInp[idx], sizeof(INPUT));
					pInp[idx].type=INPUT_KEYBOARD;
					pInp[idx].ki.wVk=0;
					pInp[idx].ki.wScan=szTemp[idx/2];
					pInp[idx].ki.dwFlags=KEYEVENTF_UNICODE;
					pInp[idx].ki.time=0;
					pInp[idx].ki.dwExtraInfo=0;

					pInp[idx+1].type=INPUT_KEYBOARD;
					pInp[idx+1].ki.wVk=0;
					pInp[idx+1].ki.wScan=szTemp[idx/2];
					pInp[idx+1].ki.dwFlags=KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
					pInp[idx+1].ki.time=0;
					pInp[idx+1].ki.dwExtraInfo=0;
				}

				idx=SendInput(size, pInp, sizeof(INPUT));

				free(pInp);
			}
		} else {
			Beep(200,10);
		}
		}
		break;

	case 8:	// Make translucent - 2k Only
		{
		HWND m_targetWindow;
		POINT point;
		int icmd, val;

		if ((IsNT)&&(WinVer>=0x05)) {
			DWORD fTrans;

			val=255;
			fTrans=0;
			sscanf(szStr, "%d %d %d", &icmd, &val, &fTrans);
			GetCursorPos(&point);
			m_targetWindow = WindowFromPoint(point);
			if (fTrans) {
				fTrans=WS_EX_TRANSPARENT;
				SetWindowPos(m_targetWindow, HWND_TOPMOST, 0, 0, 0, 0, SWP_DEFERERASE|SWP_NOACTIVATE|SWP_NOCOPYBITS|SWP_NOMOVE|SWP_NOREDRAW|SWP_NOSIZE);
			}
			if (0 == SetWindowLong(m_targetWindow, GWL_EXSTYLE, GetWindowLong(m_targetWindow, GWL_EXSTYLE) | WS_EX_LAYERED | fTrans)) {
				OutputDebugString("Failed to change window to layered.\n");
				break;
			}
			
			if (false == SetLayeredWindowAttributes(m_targetWindow, RGB(0,0,0), val, LWA_ALPHA)) {
				char buf[1024];

				sprintf(buf, "Failed to set layered window on %08x: %d\n", m_targetWindow, GetLastError());
				OutputDebugString(buf);
				Beep(300,10);
			}
		} else {
			Beep(200,10);
		}
		}
		break;
	}
}

void SendToWindow(int cmd, char *clsName)
{
	HWND hWin;

	hWin=FindWindow(clsName, NULL);
	if (NULL == hWin)
		return;

	PostMessage(hWin, WM_COMMAND, cmd, 0);
}

int InstallHook(HANDLE inProgram, LPTHREAD_START_ROUTINE inDestroy)
{
	char buf[128];
	sprintf(buf, "InstallHook says the DLL module is 0x%p\n", hInstance);
	OutputDebugString(buf);

	hRunHook=inProgram;			// store RunHook's program handle
	AddrDoDestroy=inDestroy;	// Address of DoDestroy in RunHook's memory space

	// install the hook.
	if (IsNT)
	{
		Keyhhk = SetWindowsHookEx(WH_KEYBOARD_LL, (HOOKPROC)KeyHook, hInstance, 0);
	} else
	{
		Keyhhk = SetWindowsHookEx(WH_KEYBOARD, (HOOKPROC)KeyHook, hInstance, 0);
	}
	if (NULL == Keyhhk)
	{
		MessageBox(NULL, "Unable to hook keyboard", "Prepare to be Executed", MB_OK);
		return false;
	}

//	if ((IsNT)&&(WinVer>=0x05)) {
//		// Hook the window messages so we can trap the close message
//		Cbthhk = SetWindowsHookEx(WH_CALLWNDPROC, (HOOKPROC)CbtHook, hInstance, 0);
//		if (NULL == Cbthhk) {
//			OutputDebugString("Couldn't hook the CBT\n");
//		}
//	}

	return true;
}

int RemoveHook()
{
	// remove the hooks
	if ((IsNT)&&(WinVer>=0x05)&&(Cbthhk)) {
		UnhookWindowsHookEx(Cbthhk);
	}

	if (UnhookWindowsHookEx(Keyhhk)) 
	{
		return true;
	}
	else
	{
		MessageBox(NULL, "The hook can not be removed", "Have a nice day!", MB_OK);
		return false;
	}
}

