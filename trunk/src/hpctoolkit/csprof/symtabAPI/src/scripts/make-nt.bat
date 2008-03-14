rem Script for building Paradyn & DynInstAPI on WinNT platform
@echo off
rem $Id: make-nt.bat,v 1.24 2005/10/12 19:20:51 legendre Exp $
@echo on

rem enable the following to build with symbols
rem set DBG="NO_OPT_FLAG=true"

set DEST_DIR="..\%PLATFORM%"
set LIBRARY_DEST="..\%PLATFORM%\lib"
set PROGRAM_DEST="..\%PLATFORM%\bin"
set PLATFORM=i386-unknown-nt4.0

if not exist %DEST_DIR% mkdir %DEST_DIR%
if not exist %LIBRARY_DEST% mkdir %LIBRARY_DEST%
if not exist %PROGRAM_DEST% mkdir %PROGRAM_DEST%

cmd /c "cd dyninstAPI_RT\%PLATFORM% && nmake clean && nmake %DBG% install"
cmd /c "cd dyninstAPI\%PLATFORM% && nmake clean && nmake %DBG% install"
cmd /c "cd dyninstAPI\tests\%PLATFORM% && nmake clean && nmake %DBG% install"
cmd /c "cd dyner\%PLATFORM% && nmake clean && nmake %DBG% install"

cmd /c "cd igen\%PLATFORM% && nmake clean && nmake %DBG% install"
cmd /c "cd mrnet\%PLATFORM% && nmake clean && nmake %DBG% install"
cmd /c "cd pdutil\%PLATFORM% && nmake clean && nmake %DBG% install"
cmd /c "cd rtinst\%PLATFORM% && nmake clean && nmake %DBG% install"
cmd /c "cd paradynd\%PLATFORM% && nmake clean && nmake %DBG% install"
cmd /c "cd pdthread\%PLATFORM% && nmake clean && nmake %DBG% install"
cmd /c "cd paradyn\%PLATFORM% && nmake clean && nmake %DBG% install"
cmd /c "cd visi\%PLATFORM% && nmake clean && nmake %DBG% install"
cmd /c "cd visiClients\histVisi\%PLATFORM% && nmake clean && nmake %DBG% install"
cmd /c "cd visiClients\barchart\%PLATFORM% && nmake clean && nmake %DBG% install"
cmd /c "cd visiClients\tableVisi\%PLATFORM% && nmake clean && nmake %DBG% install"
cmd /c "cd visiClients\phaseTable\%PLATFORM% && nmake clean && nmake %DBG% install"
cmd /c "cd visiClients\tclVisi\%PLATFORM% && nmake clean && nmake %DBG% install"
rem No visiClients/terrain to build for WinNT!

cmd /c "cd sharedMem\%PLATFORM% && nmake clean && nmake %DBG% install"
