/*BEGIN_LEGAL 
Intel Open Source License 

Copyright (c) 2002-2005 Intel Corporation 
All rights reserved. 
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.  Redistributions
in binary form must reproduce the above copyright notice, this list of
conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.  Neither the name of
the Intel Corporation nor the names of its contributors may be used to
endorse or promote products derived from this software without
specific prior written permission.
 
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
END_LEGAL */
#if !defined(REPLACEMENT_ROUTINES_H)

#define REPLACEMENT_ROUTINES_H
/*
Contains data structures anf replacement routines used in Probes\probemalloctrace.cpp
and ToolUnitTests\jitmalloctrace.cpp
The replacement functions call the corresponding original functions in the image
*/
#define MAX_MALLOCS_REPLACED 10
#define MAX_EXITS_REPLACED 10
static void * (*pf_malloc[MAX_MALLOCS_REPLACED])(size_t size);
static void (*pf_free[MAX_MALLOCS_REPLACED])(void * ptr);
static void (*pf_exit[MAX_EXITS_REPLACED])(int status);
static void * (*my_malloc[MAX_MALLOCS_REPLACED])(size_t size);
static void (*my_free[MAX_MALLOCS_REPLACED])(void * ptr);
static void (*my_exit[MAX_EXITS_REPLACED])(int status);
int numMallocsReplaced = 0;
int numExitsReplaced = 0;

ofstream TraceFile;
void * ReplacementMalloc0(size_t size)
{
    // calls the original function that was saved in pf_malloc[0] at the time of replacement
    if (pf_malloc[0])
    {
        void * ptr = (pf_malloc[0])(size);

        TraceFile << "malloc0(" << size << ") returns " << ptr << endl;
        return ptr;
    }
    return 0;
}

void ReplacementFree0(void *p)
{
    // calls the original function that was saved in pf_free[0] at the time of replacement
    if (pf_free[0])
    {
        (pf_free[0])(p);
        
        
        TraceFile << "free0(" << p << ")" << endl;
    }
}

void ReplacementExit0(int code)
{
    if (TraceFile.is_open())
    {
    TraceFile << "## eof" << endl << flush;
    TraceFile.close();
    }
    // calls the original function that was saved in pf_exit[0] at the time of replacement
    if (pf_exit[0])
        (pf_exit[0])(code);
}

void * ReplacementMalloc1(size_t size)
{
    if (pf_malloc[1])
    {
        void * ptr = (pf_malloc[1])(size);

        TraceFile << "malloc1(" << size << ") returns " << ptr << endl;
        return ptr;
    }
    return 0;
}

void ReplacementFree1(void *p)
{
    if (pf_free[1])
    {
        (pf_free[1])(p);
        
        
        TraceFile << "free1(" << p << ")" << endl;
    }
}

void ReplacementExit1(int code)
{
    if (TraceFile.is_open())
    {
    TraceFile << "## eof" << endl << flush;
    TraceFile.close();
    }
    
    if (pf_exit[1])
        (pf_exit[1])(code);
}

void * ReplacementMalloc2(size_t size)
{
    if (pf_malloc[2])
    {
        void * ptr = (pf_malloc[2])(size);

        TraceFile << "malloc2(" << size << ") returns " << ptr << endl;
        return ptr;
    }
    return 0;
}

void ReplacementFree2(void *p)
{
    if (pf_free[2])
    {
        (pf_free[2])(p);
        
        
        TraceFile << "free2(" << p << ")" << endl;
    }
}

void ReplacementExit2(int code)
{
    if (TraceFile.is_open())
    {
    TraceFile << "## eof" << endl << flush;
    TraceFile.close();
    }
    
    if (pf_exit[2])
        (pf_exit[2])(code);
}

void * ReplacementMalloc3(size_t size)
{
    if (pf_malloc[3])
    {
        void * ptr = (pf_malloc[3])(size);

        TraceFile << "malloc3(" << size << ") returns " << ptr << endl;
        return ptr;
    }
    return 0;
}

void ReplacementFree3(void *p)
{
    if (pf_free[3])
    {
        (pf_free[3])(p);
        
        
        TraceFile << "free3(" << p << ")" << endl;
    }
}

void ReplacementExit3(int code)
{
    if (TraceFile.is_open())
    {
    TraceFile << "## eof" << endl << flush;
    TraceFile.close();
    }
    
    if (pf_exit[3])
        (pf_exit[3])(code);
}

void * ReplacementMalloc4(size_t size)
{
    if (pf_malloc[4])
    {
        void * ptr = (pf_malloc[4])(size);

        TraceFile << "malloc4(" << size << ") returns " << ptr << endl;
        return ptr;
    }
    return 0;
}

void ReplacementFree4(void *p)
{
    if (pf_free[4])
    {
        (pf_free[4])(p);
        
        
        TraceFile << "free4(" << p << ")" << endl;
    }
}

void ReplacementExit4(int code)
{
    if (TraceFile.is_open())
    {
    TraceFile << "## eof" << endl << flush;
    TraceFile.close();
    }
    
    if (pf_exit[4])
        (pf_exit[4])(code);
}

void * ReplacementMalloc5(size_t size)
{
    if (pf_malloc[5])
    {
        void * ptr = (pf_malloc[5])(size);

        TraceFile << "malloc5(" << size << ") returns " << ptr << endl;
        return ptr;
    }
    return 0;
}

void ReplacementFree5(void *p)
{
    if (pf_free[5])
    {
        (pf_free[5])(p);
        
        
        TraceFile << "free5(" << p << ")" << endl;
    }
}

void ReplacementExit5(int code)
{
    if (TraceFile.is_open())
    {
    TraceFile << "## eof" << endl << flush;
    TraceFile.close();
    }
    
    if (pf_exit[5])
        (pf_exit[5])(code);
}

void * ReplacementMalloc6(size_t size)
{
    if (pf_malloc[6])
    {
        void * ptr = (pf_malloc[6])(size);

        TraceFile << "malloc6(" << size << ") returns " << ptr << endl;
        return ptr;
    }
    return 0;
}

void ReplacementFree6(void *p)
{
    if (pf_free[6])
    {
        (pf_free[6])(p);
        
        
        TraceFile << "free6(" << p << ")" << endl;
    }
}

void ReplacementExit6(int code)
{
    if (TraceFile.is_open())
    {
    TraceFile << "## eof" << endl << flush;
    TraceFile.close();
    }
    
    if (pf_exit[6])
        (pf_exit[6])(code);
}

void * ReplacementMalloc7(size_t size)
{
    if (pf_malloc[7])
    {
        void * ptr = (pf_malloc[7])(size);

        TraceFile << "malloc7(" << size << ") returns " << ptr << endl;
        return ptr;
    }
    return 0;
}

void ReplacementFree7(void *p)
{
    if (pf_free[7])
    {
        (pf_free[7])(p);
        
        
        TraceFile << "free7(" << p << ")" << endl;
    }
}

void ReplacementExit7(int code)
{
    if (TraceFile.is_open())
    {
    TraceFile << "## eof" << endl << flush;
    TraceFile.close();
    }
    
    if (pf_exit[7])
        (pf_exit[7])(code);
}

void * ReplacementMalloc8(size_t size)
{
    if (pf_malloc[8])
    {
        void * ptr = (pf_malloc[8])(size);

        TraceFile << "malloc8(" << size << ") returns " << ptr << endl;
        return ptr;
    }
    return 0;
}

void ReplacementFree8(void *p)
{
    if (pf_free[8])
    {
        (pf_free[8])(p);
        
        
        TraceFile << "free8(" << p << ")" << endl;
    }
}

void ReplacementExit8(int code)
{
    if (TraceFile.is_open())
    {
    TraceFile << "## eof" << endl << flush;
    TraceFile.close();
    }
    
    if (pf_exit[8])
        (pf_exit[8])(code);
}

void * ReplacementMalloc9(size_t size)
{
    if (pf_malloc[9])
    {
        void * ptr = (pf_malloc[9])(size);

        TraceFile << "malloc9(" << size << ") returns " << ptr << endl;
        return ptr;
    }
    return 0;
}

void ReplacementFree9(void *p)
{
    if (pf_free[9])
    {
        (pf_free[9])(p);
        
        
        TraceFile << "free9(" << p << ")" << endl;
    }
}

void ReplacementExit9(int code)
{
    if (TraceFile.is_open())
    {
    TraceFile << "## eof" << endl << flush;
    TraceFile.close();
    }
    
    if (pf_exit[9])
        (pf_exit[9])(code);
}

#endif
