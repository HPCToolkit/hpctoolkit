@echo off
rem $Id: buildstamp.bat,v 1.2 1998/04/03 13:31:01 wylie Exp $
rem build version identification voucher
setlocal
rem Since I can't find a way to catch date and time output in a variable, it
rem   looks like we'll have to live with environmentally-defined DATE and TIME
rem set DATE="date /t"
rem set TIME="time /t"
set BUILDER=%USERNAME%@%COMPUTERNAME%
set SUITE=Suite
set VERSION=2.1

if not "%1"=="-s" goto Error
set SUITE=%2
shift
shift
if not "%1"=="-v" goto Error
set REL=%2
shift
shift
if "%1" == "" goto Error
set COMP=%1

if "%DATE%"=="" set DATE=%%DATE%%
if "%TIME%"=="" set TIME=%%TIME%%
if "%USERNAME%"=="paradyn" set BUILDER=paradyn@cs.wisc.edu

set V_FILE=V_%COMP%.c

echo /* Automated build version identification */ >%V_FILE%
echo extern const char V_%COMP%[]; >>%V_FILE%
echo        const char V_%COMP%[] = >>%V_FILE%
echo     "$%SUITE%: v%REL% %COMP% #0 %DATE% %TIME% %BUILDER% $"; >>%V_FILE%
echo     "$%SUITE%: v%REL% %COMP% #0 %DATE% %TIME% %BUILDER% $";
rem    "$Paradyn: v2.1b-010 paradynd #6 1998/03/26 17:16 paradyn@cs.wisc.edu $";

endlocal
goto Exit

:Error
echo "Bad parameters"

:Exit

