rem Script for building Paradyn & DynInstAPI on WinNT platform
@echo off
rem $Id: clean-win.bat,v 1.2 2004/03/02 07:06:08 rchen Exp $
@echo on

set DEST_DIR="..\%PLATFORM%"
set LIBRARY_DEST="..\%PLATFORM%\lib"
set PROGRAM_DEST="..\%PLATFORM%\bin"

if not exist %DEST_DIR% mkdir %DEST_DIR%
if not exist %LIBRARY_DEST% mkdir %LIBRARY_DEST%
if not exist %PROGRAM_DEST% mkdir %PROGRAM_DEST%

cmd /c "cd igen\%PLATFORM% && nmake clean"
cmd /c "cd pdutil\%PLATFORM% && nmake clean"
cmd /c "cd mdl\%PLATFORM% && nmake clean"
cmd /c "cd rtinst\%PLATFORM% && nmake clean"
cmd /c "cd paradynd\%PLATFORM% && nmake clean"
cmd /c "cd thread\%PLATFORM% && nmake clean"
cmd /c "cd paradyn\%PLATFORM% && nmake clean"
cmd /c "cd visi\%PLATFORM% && nmake clean"
cmd /c "cd visiClients\histVisi\%PLATFORM% && nmake clean"
cmd /c "cd visiClients\barchart\%PLATFORM% && nmake clean"
cmd /c "cd visiClients\tableVisi\%PLATFORM% && nmake clean"
cmd /c "cd visiClients\phaseTable\%PLATFORM% && nmake clean"
cmd /c "cd visiClients\tclVisi\%PLATFORM% && nmake clean"
rem No visiClients/terrain to build for WinNT!

cmd /c "cd dyninstAPI_RT\%PLATFORM% && nmake clean"
cmd /c "cd dyninstAPI\%PLATFORM% && nmake clean"
cmd /c "cd dyninstAPI\tests\%PLATFORM% && nmake clean"
cmd /c "cd dyner\%PLATFORM% && nmake clean"
