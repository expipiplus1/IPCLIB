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
#include <stdio.h>

#ifdef _DEBUG
#	include <assert.h>
#endif

#include "IPCLib.h"

#define NUM_TESTS 1048576
#define MAX_STRING_LEN 1024
static const WCHAR TESTCHARS[] = L"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

#ifndef assert
#	define assert(x) { if (!(x)) DebugBreak(); }
#endif

typedef struct _PRODUCER_PACKET
{
	DWORD dwLength;
	DWORD dwCheckSum;
} PRODUCER_PACKET;

int ProducerThread( DWORD_PTR index )
{
	WCHAR debug[256 + MAX_STRING_LEN];
    WCHAR _tmp[sizeof(PRODUCER_PACKET) + MAX_STRING_LEN + 256];
	PRODUCER_PACKET* pPacket = (PRODUCER_PACKET*) _tmp;
	WCHAR* pPayload = (WCHAR*)( pPacket + 1 );
    IPC_STREAM* pIPC = NULL;
	DWORD i, j, offset;

	SetThreadAffinityMask( GetCurrentThread(), (DWORD_PTR) ( 1UL << index ) );

    OpenInterprocessStream( L"AWHKTEST", &pIPC);
    
    for (i = 0; i < NUM_TESTS; ++i)
    {
		UINT len = rand() % MAX_STRING_LEN;
        offset = swprintf_s( pPayload, MAX_STRING_LEN, L" [%d, %d, %d] ", index, i, len );
		for (j = offset; j < offset+len; ++j) 
		{
			pPayload[j] = TESTCHARS[rand() % (_countof(TESTCHARS)-1)];
			assert(isprint(pPayload[j]));
		}

		pPayload[j] = 0;
		pPacket->dwLength = j;

		pPacket->dwCheckSum = 0;
		for (j = 0; j < offset+len; ++j) 
		{
			pPacket->dwCheckSum += pPayload[j];
		}

        WriteInterprocessStream( pIPC, pPacket, sizeof(PRODUCER_PACKET) + pPacket->dwLength * sizeof(WCHAR) );

		swprintf_s( debug, _countof(debug), L"Thread %d producing %d characters (checksum %X): %s\n", index, pPacket->dwLength, pPacket->dwCheckSum, pPayload );
		OutputDebugStringW( debug );
    }

    CloseInterprocessStream(pIPC);
    return 0;
}

HANDLE StartProducerThread(UINT index)
{
    return CreateThread( NULL, 0, (LPTHREAD_START_ROUTINE) ProducerThread, (LPVOID) index, 0, NULL );
}

int ConsumerThread( DWORD_PTR index )
{
    IPC_STREAM* pIPC = NULL;
	UINT i, j, len, checksum;
	WCHAR debug[256 + MAX_STRING_LEN];
    WCHAR t[256 + MAX_STRING_LEN];

	SetThreadAffinityMask( GetCurrentThread(), (DWORD_PTR) ( 1UL << index ) );

    OpenInterprocessStream( L"AWHKTEST", &pIPC);
    
    for (i = 0; i < NUM_TESTS; ++i)
    {
        ReadInterprocessStream( pIPC, &len, sizeof(len) );
		assert( len <= _countof(t) );

        ReadInterprocessStream( pIPC, &checksum, sizeof(checksum) );
        ReadInterprocessStream( pIPC, t, len * sizeof(WCHAR) );

 		for (j = 0; j < len; ++j) 
		{
			assert(isprint(t[j]));
			checksum -= t[j];
		}

        t[len] = 0;
		swprintf_s( debug, _countof(debug), L"Consuming %d characters (checksum %X): %s\n", len, checksum, t );
		OutputDebugStringW( debug );

		assert(checksum == 0);

        wprintf( t );
    }

    CloseInterprocessStream(pIPC);
	
	return 0;
}

HANDLE StartConsumerThread(UINT index)
{
    return CreateThread( NULL, 0, (LPTHREAD_START_ROUTINE) ConsumerThread, (LPVOID) index, 0, NULL );
}

int main(int argc, char** argv)
{
    IPC_STREAM* pIPC = NULL;
    CreateInterprocessStream( L"AWHKTEST", 1024, &pIPC );

	{
		HANDLE hThreads[] = { 
			StartProducerThread( 0 ),
			StartProducerThread( 1 ),
			StartProducerThread( 2 ),
			StartProducerThread( 3 ),

			StartConsumerThread( 4 )
		};

		WaitForMultipleObjects( _countof(hThreads), hThreads, TRUE, INFINITE );
	}

    CloseInterprocessStream(pIPC);
	
	return 0;
}

