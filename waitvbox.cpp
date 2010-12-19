#include <windows.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <stdio.h>
#include <malloc.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")

#define EVENT_NAME "b24ff9b5-8a5c-4790-9b04-8f369bf3df7e"

DWORD
Exec(char* cmd, PROCESS_INFORMATION* pi, DWORD show)
{
    STARTUPINFO si = {0};
    
    ZeroMemory(pi, sizeof(*pi));
    si.cb = sizeof(STARTUPINFO);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = show;
    if (! CreateProcess(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, pi))
        return GetLastError();
    return NO_ERROR;
}

BOOL
Ping(const char* target, DWORD timeout = 1000)
{
    unsigned long ipaddr = inet_addr(target);
    if (ipaddr == INADDR_NONE)
        return FALSE;

    HANDLE icmp = IcmpCreateFile();
    if (icmp == INVALID_HANDLE_VALUE)
        return FALSE;

    char   data[32] = { 0 };
    DWORD  size = sizeof(ICMP_ECHO_REPLY) + sizeof(data);   
    char  *buf  = (char*)alloca(size);
    if (! buf)
        return FALSE;

    DWORD result = IcmpSendEcho(icmp, ipaddr, data, sizeof(data), NULL, buf, size, timeout);
    CloseHandle(icmp);
    return 0 != result;
}

int WINAPI
WinMain(HINSTANCE inst, HINSTANCE prevInst, LPSTR cmd, int cmdshow)
{
    PROCESS_INFORMATION pi;

    if (__argc < 5) {
        MessageBox(0, "usage: waitvbox <vbox-home> <vm-name> <client-command> <host>", 0, 0);
        return 1;
    }

    HANDLE handles[32] = { 0 };
    DWORD  nhandles    = 2;
    if (handles[0] = OpenEvent(EVENT_ALL_ACCESS, FALSE, EVENT_NAME)) {
        SetEvent(handles[0]);
        return 0;
    } else {
        handles[0] = CreateEvent(NULL, TRUE, FALSE, EVENT_NAME);
    }

    char initcmd[1024], termcmd[1024], clientcmd[1024], hostaddr[1024];
    wsprintf(initcmd,   "\"%s\\VBoxHeadless.exe\" -s %s -vrdp=off", __argv[1], __argv[2]);
    wsprintf(termcmd,   "\"%s\\VBoxManage.exe\" controlvm %s savestate", __argv[1], __argv[2]);
    wsprintf(hostaddr,  "%s", __argv[4]);
    wsprintf(clientcmd, "\"%s\" %s", __argv[3], hostaddr);

    if (Exec(initcmd, &pi, SW_HIDE) != NO_ERROR) {
        MessageBox(0, "error: init-cmd", 0, 0);
        return 1;
    }
    handles[1] = pi.hProcess;

    Sleep(5 * 1000);
    int nping = 0;
    for (int i = 0; i < 20; ++i) {
        if (Ping(hostaddr)) {
            ++nping;
        } else {
            Beep(262, 300);
            Sleep(5 * 1000);
        }
        if (nping == 3)
            break;
    }

    DWORD result = nping == 3 ? WAIT_OBJECT_0 : WAIT_TIMEOUT;
    while (1) {
        if (result == WAIT_OBJECT_0) {
            // new client
            if (Exec(clientcmd, &pi, SW_SHOW) != NO_ERROR) {
                MessageBox(0, "error: client-cmd", 0, 0);
            } else {
                handles[nhandles] = pi.hProcess;
                nhandles += 1;
            }
            ResetEvent(handles[0]);
        } else if (result > WAIT_OBJECT_0 + 1 && result < WAIT_OBJECT_0 + nhandles) {
            // stop client
            DWORD n = result - WAIT_OBJECT_0;
            CloseHandle(handles[n]);
            handles[n] = handles[nhandles-1];
            nhandles -= 1;
        } else {
            // stop vm/error
            break;
        }
        if (nhandles < 3)
            break;
        result = WaitForMultipleObjects(nhandles, handles, FALSE, INFINITE);
    }
    if (Exec(termcmd, &pi, SW_HIDE) != NO_ERROR) {
        MessageBox(0, "error: term-cmd", 0, 0);
        return 1;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);

    return 0;
}
