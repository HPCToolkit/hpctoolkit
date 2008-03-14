#rem Script for building Paradyn & DynInstAPI on WinNT platform
#@echo off
#rem $Id: make-nt.sh,v 1.24 2005/10/12 19:20:52 legendre Exp $
#@echo on

set PLATFORM=i386-unknown-nt4.0
#set DEST_DIR="..\%PLATFORM%"
#set LIBRARY_DEST="..\%PLATFORM%\lib"
#set PROGRAM_DEST="..\%PLATFORM%\bin"

#if not exist %DEST_DIR% mkdir %DEST_DIR%
#if not exist %LIBRARY_DEST% mkdir %LIBRARY_DEST%
#if not exist %PROGRAM_DEST% mkdir %PROGRAM_DEST%
pwd

PDROOT=`pwd`

cd dyninstAPI_RT/$PLATFORM; nmake clean; nmake install
cd $PDROOT
cd dyninstAPI/$PLATFORM; nmake clean; nmake install
cd $PDROOT
cd dyninstAPI/tests/$PLATFORM; nmake clean; nmake install
cd $PDROOT
cd dyner/$PLATFORM; nmake clean; nmake install
cd $PDROOT

cd igen/$PLATFORM; nmake clean; nmake install
cd $PDROOT
cd mrnet/$PLATFORM; nmake clean; nmake install
cd $PDROOT
cd pdutil/$PLATFORM; nmake clean; nmake install
cd $PDROOT
cd rtinst/$PLATFORM; nmake clean; nmake install
cd $PDROOT
cd paradynd/$PLATFORM; nmake clean; nmake install
cd $PDROOT
cd pdthread/$PLATFORM; nmake clean; nmake install
cd $PDROOT
cd paradyn/$PLATFORM; nmake clean; nmake install
cd $PDROOT
cd visi/$PLATFORM; nmake clean; nmake install
cd $PDROOT
cd visiClients/histVisi/$PLATFORM; nmake clean; nmake install
cd $PDROOT
cd visiClients/barchart/$PLATFORM; nmake clean; nmake install
cd $PDROOT
cd visiClients/tableVisi/$PLATFORM; nmake clean; nmake install
cd $PDROOT
cd visiClients/phaseTable/$PLATFORM; nmake clean; nmake install
cd $PDROOT
cd visiClients/tclVisi/$PLATFORM; nmake clean; nmake install
cd $PDROOT
#rem No visiClients/terrain to build for WinNT!

cd sharedMem/$PLATFORM; nmake clean; nmake install
cd $PDROOT




