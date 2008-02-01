/* GKrellM
|  Copyright (C) 1999-2007 Bill Wilson
|
|  Author:  Bill Wilson    billw@gkrellm.net
|  Latest versions might be found at:  http://gkrellm.net
|
|
|  GKrellM is free software: you can redistribute it and/or modify it
|  under the terms of the GNU General Public License as published by
|  the Free Software Foundation, either version 3 of the License, or
|  (at your option) any later version.
|
|  GKrellM is distributed in the hope that it will be useful, but WITHOUT
|  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
|  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public
|  License for more details.
|
|  You should have received a copy of the GNU General Public License
|  along with this program. If not, see http://www.gnu.org/licenses/
*/

#include "win32-gui.h"
#include "resource.h"
#include <shellapi.h>

static UINT      s_gkrellmCallback;
static ATOM      s_wndAtom = 0;
static HINSTANCE s_hInst;
static HWND      s_hWnd = NULL;


LRESULT CALLBACK serverWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == s_gkrellmCallback && lParam == WM_RBUTTONUP)
    {
        PostQuitMessage(0);
        done = 1;
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}


int createServerWindow(HINSTANCE hInstance)
{
    NOTIFYICONDATA nid;
    WNDCLASSEX     wndClass;
    const char *   wndClassName = "GKrellMServer";
    
    done  = 0;
    s_hInst = hInstance;
        
    wndClass.cbSize        = sizeof(WNDCLASSEX);
    wndClass.style         = 0;// CS_HREDRAW | CS_VREDRAW;
    wndClass.lpfnWndProc   = serverWndProc;
    wndClass.cbClsExtra    = 0;
    wndClass.cbWndExtra    = 0;
    wndClass.hInstance     = s_hInst;
    wndClass.hIcon         = NULL;
    wndClass.hCursor       = NULL;
    wndClass.hbrBackground = 0;
    wndClass.lpszMenuName  = NULL;
    wndClass.lpszClassName = wndClassName;
    wndClass.hIconSm       = NULL;
    
    // Register window class for server window
    s_wndAtom = RegisterClassEx(&wndClass);
    if (s_wndAtom == 0)
        return 0;

    // Create invisible server window (needed to have a parent for the trayicon)    
    s_hWnd = CreateWindow(MAKEINTATOM(s_wndAtom), "GKrellMServer", SW_HIDE, 0, 0, 0, 0,
                          NULL, NULL, s_hInst, NULL);
    if (s_hWnd == NULL)
        return 0;

    s_gkrellmCallback = RegisterWindowMessage(TEXT("GKrellMCallback"));

    // Create system tray icon
    nid.cbSize           = sizeof(NOTIFYICONDATA);
    nid.hWnd             = s_hWnd;
    nid.uID              = 1;   
    nid.uFlags           = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    nid.uCallbackMessage = s_gkrellmCallback;
    strcpy(nid.szTip,"GKrellM Server for Windows"); // TODO: i18n
    nid.hIcon            = LoadIcon(GetModuleHandle(NULL),
                                    MAKEINTRESOURCE(IDI_ICON3));
    Shell_NotifyIcon(NIM_ADD, &nid);

    return 1;
}


void deleteServerWindow()
{
    if (s_hWnd != NULL)
    {
        NOTIFYICONDATA nid;

        // Remove system tray icon
        nid.cbSize           = sizeof(NOTIFYICONDATA);
        nid.hWnd             = s_hWnd;
        nid.uID              = 1;   
        Shell_NotifyIcon(NIM_DELETE, &nid);

        // Remove invisible server window
        DestroyWindow(s_hWnd);
    }

    if (s_wndAtom != 0)
    {
        // Unregister window class of server window
        UnregisterClass(MAKEINTRESOURCE(s_wndAtom), s_hInst);
    }
}
