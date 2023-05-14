@echo off
REM
REM winLAME - a frontend for the LAME encoding engine
REM Copyright (c) 2000-2019 Michael Fink
REM
REM Creates a winLAME build and installer
REM

REM -{ config here }-----------------------------

set MSVC_PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community

REM -{ config end }------------------------------

REM set up build environment
call "%MSVC_PATH%\Common7\Tools\VsDevCmd.bat"

pushd source\libraries
call CopyLibraries.cmd Release
popd

REM build solution
msbuild /m winlame.sln /property:Configuration=Release,Platform=Win32 /target:Rebuild


set ZIP="%ProgramFiles%\7-Zip\7z.exe"
%ZIP% a bin\winLAME-pdbs.zip bin\Release\pdb\winLAME.pdb

REM build winLAME Portable
call BuildPortable.cmd

REM finished
echo Finished!

set MSVC_PATH=

pause
