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
#include <assert.h>

#include "IPCLib.h"

#define NUM_TESTS 1048576
#define MAX_STRING_LEN 1024
static const WCHAR TESTCHARS[] = L"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

int test(void* poo)
{
    WCHAR tmp[MAX_STRING_LEN];
    IPC_STREAM* pIPC = NULL;
	UINT i, j;

    OpenInterprocessStream( L"AWHKTEST", &pIPC);
    
    for (i = 0; i < NUM_TESTS; ++i)
    {
		UINT checksum = 0;
		UINT len = 12 + rand() % (_countof(tmp) - 12);
        UINT offset = swprintf_s( tmp, _countof(tmp), L" [%d] ", i );
		for (j = offset; j < len; ++j) 
		{
			tmp[j] = TESTCHARS[rand() % _countof(TESTCHARS)];
		}

		for (j = 0; j < len; ++j) 
		{
			checksum += tmp[j];
		}

        WriteInterprocessStream( pIPC, &len, sizeof(UINT) );
        WriteInterprocessStream( pIPC, &checksum, sizeof(UINT) );
        WriteInterprocessStream( pIPC, tmp, len * sizeof(WCHAR) );
    }

    CloseInterprocessStream(pIPC);
    return 0;
}

int main(int argc, char** argv)
{
    IPC_STREAM* pIPC = NULL;
	UINT i, j, len, checksum;
    WCHAR t[MAX_STRING_LEN];

    CreateInterprocessStream( L"AWHKTEST", 1024, &pIPC );
    CreateThread( NULL, 0, (LPTHREAD_START_ROUTINE) test, NULL, 0, NULL );

    for (i = 0; i < NUM_TESTS; ++i)
    {
        ReadInterprocessStream( pIPC, &len, sizeof(len) );
        ReadInterprocessStream( pIPC, &checksum, sizeof(checksum) );
        ReadInterprocessStream( pIPC, t, len * sizeof(WCHAR) );

 		for (j = 0; j < len; ++j) 
		{
			checksum -= t[j];
		}

		assert(checksum == 0);

        t[len] = 0;
        wprintf( t );
    }

    CloseInterprocessStream(pIPC);
	
	return 0;
}

