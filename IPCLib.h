/*
	Copyright (C) 2015 Peter J. B. Lewis

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
*/

#ifndef __IPCLIB_H__
#define __IPCLIB_H__

#ifdef __cplusplus
extern "C" {
#endif

#define IPCLIB_VERSION MAKELONG(1, 0)

typedef struct _IPC_STREAM IPC_STREAM;

HRESULT CreateInterprocessStream(
    _In_z_ LPCWSTR szName,
	_In_ DWORD dwVersion,
    _In_ UINT uRingBufferSize,
    _Out_ IPC_STREAM** ppIPC );

HRESULT OpenInterprocessStream(
    _In_z_ LPCWSTR szName,
	_In_ DWORD dwVersion,
    _Out_ IPC_STREAM** ppIPC );

BOOL QueryInterprocessStreamIsOpen(
	_In_z_ LPCWSTR szName,
	_In_ DWORD dwVersion );

HRESULT WriteInterprocessStream(
    _In_ IPC_STREAM* pIPC,
    _In_reads_(dataSize) LPCVOID pData,
    _In_ UINT dataSize );

HRESULT ReadInterprocessStream(
    _In_ IPC_STREAM* pIPC,
    _Out_writes_(*pDataSize) LPVOID pData,
    _Out_ UINT dataSize );

HRESULT CloseInterprocessStream(
    _In_ IPC_STREAM* pIPC );


#ifdef __cplusplus
}
#endif

#endif
