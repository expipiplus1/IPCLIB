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

#include <Windows.h>
#include <memory.h>

#ifdef _DEBUG
#	include <assert.h>
#endif

#include "IPCLib.h"

#define IPC_IO_GRANULARITY 256
#define IPC_SPINLOCK_COUNT 10000

typedef struct _IPC_RING
{
    volatile UINT64 WriteCursor;
    volatile UINT64 ReadCursor;
    volatile UINT   RingBufferSize;
} IPC_RING;

struct _IPC_STREAM
{
    LPWSTR			MappedFileName;
    LPWSTR			WriteLockName;
    LPWSTR			WriteEventName;
    LPWSTR			ReadLockName;
    LPWSTR			ReadEventName;
    IPC_RING*		pRing;
    BYTE*			pBuffer;
    HANDLE			hWriteLock;
    HANDLE			hWriteEvent;
    HANDLE			hReadLock;
    HANDLE			hReadEvent;
    HANDLE			hMappedFile;
    UINT			MappedFileSize;
    UINT			RingBufferSize;
    UINT			IOGranularity;
    BOOL			bIsServer;
};

LPWSTR JoinString(
    LPCWSTR szPrefix,
    LPCWSTR szSuffix )
{
    SIZE_T aLen = wcslen( szPrefix );
    SIZE_T bLen = wcslen( szSuffix );
    SIZE_T totalLen = aLen + bLen + 1;

    WCHAR* newStr = (WCHAR*) malloc(sizeof(WCHAR) * totalLen);
    if ( newStr == NULL )
        return NULL;
    
    wcscpy_s( newStr, totalLen, szPrefix );
    wcscat_s( newStr, totalLen, szSuffix );

    return newStr;
}

HRESULT CreateInterprocessStream(
    LPCWSTR szName,
    UINT uRingBufferSize,
    IPC_STREAM** ppIPC )
{
	SECURITY_ATTRIBUTES sa;
	IPC_STREAM* pIPC;
	UINT uTotalBufferSize;

    if ( ppIPC == NULL ) 
        return E_INVALIDARG;
    if ( uRingBufferSize <= sizeof(IPC_RING) )
        return E_INVALIDARG;
    if ( szName == NULL || *szName == 0 )
        return E_INVALIDARG;

	// Make sure we can do at least two writes to the buffer
	uRingBufferSize = max( uRingBufferSize, IPC_IO_GRANULARITY * 2 );

    pIPC = (IPC_STREAM*) malloc( sizeof(IPC_STREAM) );
    ZeroMemory( pIPC, sizeof(pIPC) );

    pIPC->WriteLockName = JoinString( szName, L"_writelock" );
    pIPC->WriteEventName = JoinString( szName, L"_write" );
    pIPC->ReadLockName = JoinString( szName, L"_readlock" );
    pIPC->ReadEventName = JoinString( szName, L"_read" );
    pIPC->MappedFileName = JoinString( szName, L"_mmap" );

	sa.bInheritHandle = FALSE;
	sa.lpSecurityDescriptor = NULL;
	sa.nLength = sizeof( sa );

	pIPC->hWriteLock = CreateMutex(
		&sa,
		FALSE,
    	pIPC->WriteLockName );
	if ( !pIPC->hWriteLock || 
         GetLastError() == ERROR_ALREADY_EXISTS || 
         GetLastError() == ERROR_ACCESS_DENIED )
    {
        CloseInterprocessStream( pIPC );
		return HRESULT_FROM_WIN32( GetLastError() );
    }

	pIPC->hWriteEvent = CreateEvent(
		&sa,
		FALSE,
        FALSE,
    	pIPC->WriteEventName );
	if ( !pIPC->hWriteEvent )
    {
        CloseInterprocessStream( pIPC );
		return HRESULT_FROM_WIN32( GetLastError() );
    }

	pIPC->hReadLock = CreateMutex(
		&sa,
		FALSE,
    	pIPC->ReadLockName );
	if ( !pIPC->hReadLock || 
         GetLastError() == ERROR_ALREADY_EXISTS || 
         GetLastError() == ERROR_ACCESS_DENIED )
    {
        CloseInterprocessStream( pIPC );
		return HRESULT_FROM_WIN32( GetLastError() );
    }

	pIPC->hReadEvent = CreateEvent(
		&sa,
		FALSE,
        FALSE,
    	pIPC->ReadEventName );
	if ( !pIPC->hReadEvent )
    {
        CloseInterprocessStream( pIPC );
		return HRESULT_FROM_WIN32( GetLastError() );
    }

    uTotalBufferSize = uRingBufferSize + sizeof(IPC_RING);

    pIPC->hMappedFile = CreateFileMapping(
		(HANDLE) -1,
		NULL,
		PAGE_READWRITE,
		0,
		uTotalBufferSize,
		pIPC->MappedFileName );
	if ( !pIPC->hMappedFile )
	{
        CloseInterprocessStream( pIPC );
		return HRESULT_FROM_WIN32( GetLastError() );
	}

	pIPC->pRing = (IPC_RING*) MapViewOfFile(
		pIPC->hMappedFile,
		FILE_MAP_WRITE | FILE_MAP_READ,
		0, 0,
		uTotalBufferSize );
    if ( pIPC->pRing == NULL )
    {
        CloseInterprocessStream( pIPC );
		return HRESULT_FROM_WIN32( GetLastError() );
	}

    ZeroMemory( pIPC->pRing, uTotalBufferSize );
    pIPC->pRing->RingBufferSize = uRingBufferSize;
    pIPC->pBuffer = ( (BYTE*) pIPC->pRing ) + sizeof(IPC_RING);
    pIPC->IOGranularity = IPC_IO_GRANULARITY;
    pIPC->MappedFileSize = uTotalBufferSize;
    pIPC->RingBufferSize = uRingBufferSize;
    pIPC->bIsServer = TRUE;

    *ppIPC = pIPC;
    return S_OK;
}

HRESULT OpenInterprocessStream(
    LPCWSTR szName,
    IPC_STREAM** ppIPC )
{
	IPC_STREAM* pIPC = NULL;
	IPC_RING* pTmpRing = NULL;

    if ( ppIPC == NULL ) 
        return E_INVALIDARG;
    if ( szName == NULL || *szName == 0 )
        return E_INVALIDARG;

    pIPC = (IPC_STREAM*) malloc( sizeof(IPC_STREAM) );
    ZeroMemory( pIPC, sizeof(pIPC) );

    pIPC->WriteLockName = JoinString( szName, L"_writelock" );
    pIPC->WriteEventName = JoinString( szName, L"_write" );
    pIPC->ReadLockName = JoinString( szName, L"_readlock" );
    pIPC->ReadEventName = JoinString( szName, L"_read" );
    pIPC->MappedFileName = JoinString( szName, L"_mmap" );

	pIPC->hWriteLock = OpenMutex(
		SYNCHRONIZE,
		FALSE,
    	pIPC->WriteLockName );
	if ( !pIPC->hWriteLock )
    {
        CloseInterprocessStream( pIPC );
		return HRESULT_FROM_WIN32( GetLastError() );
    }

	pIPC->hWriteEvent = OpenEvent(
		SYNCHRONIZE | EVENT_MODIFY_STATE,
		FALSE,
		pIPC->WriteEventName );
	if ( !pIPC->hWriteEvent )
    {
        CloseInterprocessStream( pIPC );
		return HRESULT_FROM_WIN32( GetLastError() );
    }

	pIPC->hReadLock = OpenMutex(
		SYNCHRONIZE,
		FALSE,
    	pIPC->ReadLockName );
	if ( !pIPC->hReadLock )
    {
        CloseInterprocessStream( pIPC );
		return HRESULT_FROM_WIN32( GetLastError() );
    }

	pIPC->hReadEvent = OpenEvent(
		SYNCHRONIZE | EVENT_MODIFY_STATE,
		FALSE,
		pIPC->ReadEventName );
	if ( !pIPC->hReadEvent )
    {
        CloseInterprocessStream( pIPC );
		return HRESULT_FROM_WIN32( GetLastError() );
    }

    pIPC->hMappedFile = OpenFileMapping(
		FILE_MAP_WRITE | FILE_MAP_READ,
		FALSE,
		pIPC->MappedFileName );
	if ( !pIPC->hMappedFile )
	{
        CloseInterprocessStream( pIPC );
		return HRESULT_FROM_WIN32( GetLastError() );
	}

    pTmpRing = (IPC_RING*) MapViewOfFile(
		pIPC->hMappedFile,
		FILE_MAP_READ,
		0, 0,
		sizeof(IPC_RING) );
    if ( pTmpRing == NULL )
    {
        CloseInterprocessStream( pIPC );
		return HRESULT_FROM_WIN32( GetLastError() );
	}

    // Cache some of the ringbuffer properties
	__try
	{
        pIPC->RingBufferSize = pTmpRing->RingBufferSize;
	}
	__except(GetExceptionCode()==EXCEPTION_IN_PAGE_ERROR ?
	EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
	{
        CloseInterprocessStream( pIPC );
		return E_FAIL;
	}

    UnmapViewOfFile( pTmpRing );

    pIPC->MappedFileSize = pIPC->RingBufferSize + sizeof(IPC_RING);

    pIPC->pRing = (IPC_RING*) MapViewOfFile(
        pIPC->hMappedFile,
        FILE_MAP_WRITE | FILE_MAP_READ,
        0, 0, 
        pIPC->MappedFileSize );
    if ( pIPC->pRing == NULL )
    {
        CloseInterprocessStream( pIPC );
		return HRESULT_FROM_WIN32( GetLastError() );
    }

    pIPC->pBuffer = ( (BYTE*) pIPC->pRing ) + sizeof(IPC_RING);
    pIPC->IOGranularity = IPC_IO_GRANULARITY;
    pIPC->bIsServer = FALSE;

    *ppIPC = pIPC;
    return S_OK;
}

BOOL QueryInterprocessStreamIsOpen( LPCWSTR szName )
{
    LPWSTR MappedFileName = JoinString( szName, L"_mmap" );

	HANDLE hMappedFile = OpenFileMapping(
		FILE_MAP_WRITE | FILE_MAP_READ,
		FALSE,
		MappedFileName );

    free( MappedFileName );

	if ( !hMappedFile )
	{
		return FALSE;
	}

	CloseHandle( hMappedFile );
	return TRUE;
}

HRESULT CloseInterprocessStream( IPC_STREAM* pIPC )
{
    if ( pIPC == NULL )
        return E_INVALIDARG;

    if ( pIPC->bIsServer && 
         pIPC->hWriteLock && 
         pIPC->hWriteEvent &&
         pIPC->hMappedFile )
    {
        // Wait for clients to release their write lock on the ringbuffer
        WaitForSingleObject( pIPC->hWriteLock, INFINITE );

        // Clear the mapping
        ZeroMemory( pIPC->pRing, pIPC->MappedFileSize );
        ReleaseMutex( pIPC->hWriteLock );

        // Notify listeners there's data there
        SetEvent( pIPC->hWriteEvent );
    }

    if ( pIPC->pRing )
        UnmapViewOfFile( pIPC->pRing );

    if ( pIPC->WriteLockName != NULL )
        free( pIPC->WriteLockName );
    if ( pIPC->WriteEventName != NULL )
        free( pIPC->WriteEventName );
    if ( pIPC->ReadLockName != NULL )
        free( pIPC->ReadLockName );
    if ( pIPC->ReadEventName != NULL )
        free( pIPC->ReadEventName );
    if ( pIPC->MappedFileName != NULL )
        free( pIPC->MappedFileName );

    if ( pIPC->hWriteEvent != NULL )
        CloseHandle( pIPC->hWriteEvent );
    if ( pIPC->hReadEvent != NULL )
        CloseHandle( pIPC->hReadEvent );
    if ( pIPC->hWriteLock != NULL )
        CloseHandle( pIPC->hWriteLock );
    if ( pIPC->hReadLock != NULL )
        CloseHandle( pIPC->hReadLock );
    if ( pIPC->hMappedFile != NULL )
        CloseHandle( pIPC->hMappedFile );

    free( pIPC );
    return S_OK;
}

static void WriteSpinlock( 
    IPC_STREAM* pIPC,
    UINT64 writeCursor )
{
    UINT spin = IPC_SPINLOCK_COUNT;
    UINT64 readCursor = pIPC->pRing->ReadCursor;
    UINT ringBufferSize = pIPC->RingBufferSize;

    // Spin while in case the data is going to come in very soon
    while ( writeCursor - readCursor > ringBufferSize && spin-- > 0 )
    {
        SwitchToThread(); // Give up our quantum
        readCursor = pIPC->pRing->ReadCursor;
    }

    // Switch to a very slow wait 
    while ( writeCursor - pIPC->pRing->ReadCursor > ringBufferSize )
    {
        WaitForSingleObject( pIPC->hReadEvent, INFINITE );
    }

#ifdef _DEBUG
	assert( writeCursor - pIPC->pRing->ReadCursor <= ringBufferSize );
#endif
}

HRESULT WriteInterprocessStream(
    _In_ IPC_STREAM* pIPC,
    _In_reads_(dataSize) LPCVOID pData,
    _In_ UINT dataSize )
{
	UINT numPackets;
	UINT ringBufferSize;
	const BYTE* pSource = NULL;
	const BYTE* pRingEnd = NULL;

    if ( dataSize == 0 )
    {
        // Just release the semaphore and quit
        SetEvent( pIPC->hWriteEvent );
        return S_OK;
    }

    numPackets = ( dataSize + pIPC->IOGranularity - 1 ) / pIPC->IOGranularity;
    ringBufferSize = pIPC->RingBufferSize;
    pSource = (const BYTE*) pData;
    pRingEnd = pIPC->pBuffer + pIPC->RingBufferSize;
    
    // Lock the ring
    WaitForSingleObject( pIPC->hWriteLock, INFINITE );

    // Extract the current ring properties
	__try
	{
		UINT64 writeCursor = pIPC->pRing->WriteCursor;
        BYTE* pDest = pIPC->pBuffer + ( writeCursor % pIPC->RingBufferSize );

        while ( dataSize > 0 )
        {
            UINT packetSize = min( dataSize, pIPC->IOGranularity );

            // Wait until the memory becomes available
            WriteSpinlock( pIPC, writeCursor + packetSize );

            // Check for wrap: if we do, split the write
            if ( pDest + packetSize > pRingEnd )
            {
                SIZE_T splitPoint = pRingEnd - pDest;
				SIZE_T remainder = packetSize - splitPoint;
                memcpy( pDest, pSource, splitPoint );
                memcpy( pIPC->pBuffer, pSource + splitPoint, remainder );
				pDest = pIPC->pBuffer + remainder;
            }
            else
            {
                memcpy( pDest, pSource, packetSize );
				pDest += packetSize;
            }

            pSource += packetSize;
            writeCursor += packetSize;
            dataSize -= packetSize;

            // Update the write position so reads can consume the data
            pIPC->pRing->WriteCursor = writeCursor;
			MemoryBarrier();
            SetEvent( pIPC->hWriteEvent );
        }
	}
	__except(GetExceptionCode() == EXCEPTION_IN_PAGE_ERROR ?
	    EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
	{
        ReleaseMutex( pIPC->hWriteLock );
		return E_FAIL;
	}

    // Release the lock
    ReleaseMutex( pIPC->hWriteLock );
    return S_OK;
}

static UINT64 ReadSpinlock( 
    IPC_STREAM* pIPC,
    UINT64 readCursor )
{
    UINT spin = IPC_SPINLOCK_COUNT;
    while ( readCursor >= pIPC->pRing->WriteCursor && spin-- > 0 )
    {
        SwitchToThread();
    }

    if ( readCursor >= pIPC->pRing->WriteCursor )
    {
        WaitForSingleObject( pIPC->hWriteEvent, INFINITE );
    }

#ifdef _DEBUG
	assert( pIPC->pRing->ReadCursor <= pIPC->pRing->WriteCursor );
#endif

    return pIPC->pRing->WriteCursor;
}

HRESULT ReadInterprocessStream(
    _In_ IPC_STREAM* pIPC,
    _Out_writes_(*pDataSize) LPVOID pData,
    _Out_ UINT dataSize )
{
    UINT ioGranularity = pIPC->IOGranularity;
    UINT ringBufferSize = pIPC->RingBufferSize;
    BYTE* pBuffer = pIPC->pBuffer;
    BYTE* pBufferEnd = pBuffer + ringBufferSize;

    // Secure the read lock
    WaitForSingleObject( pIPC->hReadLock, INFINITE );

	__try
	{
        UINT64 readCursor = pIPC->pRing->ReadCursor;

        const BYTE* pSrc = pBuffer + ( readCursor % ringBufferSize );
        BYTE* pDest = (BYTE*) pData;
        BYTE* pEnd = pDest + dataSize;

        while ( pDest < pEnd )
        {
            // Wait until the memory becomes available
            UINT64 writeCursor = ReadSpinlock( pIPC, readCursor );

            // How much memory is available?
            UINT available = min( dataSize, min( ioGranularity, (UINT) (writeCursor - readCursor) ) );

            // If we're about to overrun the buffer, split the read
            if ( pSrc + available > pBufferEnd )
            {
                SIZE_T splitPoint = pBufferEnd - pSrc;
				SIZE_T remainder = available - splitPoint;
                memcpy( pDest, pSrc, splitPoint );
                memcpy( pDest + splitPoint, pBuffer, remainder );
				pSrc = pBuffer + remainder;
            }
            else
            {
                memcpy( pDest, pSrc, available );
				pSrc += available;
            }

            readCursor += available;
            dataSize -= available;
            pDest += available;

            // Free it up so writes can resume
            pIPC->pRing->ReadCursor = readCursor;
			MemoryBarrier();
            SetEvent( pIPC->hReadEvent );
        }
	}
	__except(GetExceptionCode() == EXCEPTION_IN_PAGE_ERROR ?
	    EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
	{
        ReleaseMutex( pIPC->hReadLock );
		return E_FAIL;
	}

    ReleaseMutex( pIPC->hReadLock );
    return S_OK;
}

