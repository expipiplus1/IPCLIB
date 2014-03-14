/*
	Copyright (C) 2013 Peter J. B. Lewis

    Permission is hereby granted, free of charge, to any person obtaining a copy of this software 
    and associated documentation files (the "Software"), to deal in the Software without restriction,
    including without limitation the rights to use, copy, modify, merge, publish, distribute, 
    sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is 
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all copies or 
    substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING 
    BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND 
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, 
    DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.



    This IPC implementation works well for _local_ interprocess communication. If you want to do it
    over a network you should probably look at rolling your own.
*/

#pragma once

#include <sal.h>

typedef struct IPC_INTERNAL* IPC_HANDLE;

/*
    Functions for managing IPC handles
*/
BOOL IPC_Exists(
    _In_z_ LPCTSTR szIpcName);

BOOL IPC_Create(
    _In_z_ LPCTSTR szIpcName, 
    _Out_ IPC_HANDLE* ppIpcHandle);

BOOL IPC_Open(
    _In_z_ LPCTSTR szIpcName, 
    _Out_ IPC_HANDLE* ppIpcHandle);

void IPC_Close(
    _In_ IPC_HANDLE pIpcHandle);

/*
    Functions for managing the data in an IPC handle
*/

LPVOID IPC_MapMemory(
    _In_ IPC_HANDLE pIpcHandle,
    _In_ DWORD dwAccess, /* See MSDN FILE_MAP enum: http://msdn.microsoft.com/en-us/library/windows/desktop/aa366559(v=vs.85).aspx */
    _In_ SIZE_T nSize );

void IPC_UnmapMemory(
    _In_ LPVOID pMemory );

/* Call this to notify waiting clients that something changed. */
BOOL IPC_SyncClients(
    _In_ IPC_HANDLE pIpcHandle );

