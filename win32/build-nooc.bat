@rem ------------------------------------------------------
@rem batch file to build nooc using mingw, msvc or nooc itself
@rem ------------------------------------------------------

@echo off
setlocal
if (%1)==(-clean) goto :cleanup
set CC=gcc
set /p VERSION= < ..\VERSION
set NOOCDIR=
set BINDIR=
set DOC=no
set EXES_ONLY=no
goto :a0
:a2
shift
:a3
shift
:a0
if not (%1)==(-c) goto :a1
set CC=%~2
if (%2)==(cl) set CC=@call :cl
goto :a2
:a1
if (%1)==(-t) set T=%2&& goto :a2
if (%1)==(-v) set VERSION=%~2&& goto :a2
if (%1)==(-i) set NOOCDIR=%2&& goto :a2
if (%1)==(-b) set BINDIR=%2&& goto :a2
if (%1)==(-d) set DOC=yes&& goto :a3
if (%1)==(-x) set EXES_ONLY=yes&& goto :a3
if (%1)==() goto :p1
:usage
echo usage: build-nooc.bat [ options ... ]
echo options:
echo   -c prog              use prog (gcc/nooc/cl) to compile nooc
echo   -c "prog options"    use prog with options to compile nooc
echo   -t 32/64             force 32/64 bit default target
echo   -v "version"         set nooc version
echo   -i noocdir            install nooc into noocdir
echo   -b bindir            but install nooc.exe and libnooc.dll into bindir
echo   -d                   create nooc-doc.html too (needs makeinfo)
echo   -x                   just create the executables
echo   -clean               delete all previously produced files and directories
exit /B 1

@rem ------------------------------------------------------
@rem sub-routines

:cleanup
set LOG=echo
%LOG% removing files:
for %%f in (*nooc.exe libnooc.dll lib\*.a) do call :del_file %%f
for %%f in (..\config.h ..\config.texi) do call :del_file %%f
for %%f in (include\*.h) do @if exist ..\%%f call :del_file %%f
for %%f in (include\nooclib.h examples\libnooc_test.c) do call :del_file %%f
for %%f in (lib\*.o *.o *.obj *.def *.pdb *.lib *.exp *.ilk) do call :del_file %%f
%LOG% removing directories:
for %%f in (doc libnooc) do call :del_dir %%f
%LOG% done.
exit /B 0
:del_file
if exist %1 del %1 && %LOG%   %1
exit /B 0
:del_dir
if exist %1 rmdir /Q/S %1 && %LOG%   %1
exit /B 0

:cl
@echo off
set CMD=cl
:c0
set ARG=%1
set ARG=%ARG:.dll=.lib%
if (%1)==(-shared) set ARG=-LD
if (%1)==(-o) shift && set ARG=-Fe%2
set CMD=%CMD% %ARG%
shift
if not (%1)==() goto :c0
echo on
%CMD% -O2 -W2 -Zi -MT -GS- -nologo %DEF_GITHASH% -link -opt:ref,icf
@exit /B %ERRORLEVEL%

@rem ------------------------------------------------------
@rem main program

:p1
if not %T%_==_ goto :p2
set T=32
if %PROCESSOR_ARCHITECTURE%_==AMD64_ set T=64
if %PROCESSOR_ARCHITEW6432%_==AMD64_ set T=64
:p2
if "%CC:~-3%"=="gcc" set CC=%CC% -O2 -s -static
if (%BINDIR%)==() set BINDIR=%NOOCDIR%

set D32=-DNOOC_TARGET_PE -DNOOC_TARGET_I386
set D64=-DNOOC_TARGET_PE -DNOOC_TARGET_X86_64
set P32=i386-win32
set P64=x86_64-win32

if %T%==64 goto :t64
set D=%D32%
set P=%P32%
set DX=%D64%
set PX=%P64%
set TX=64
goto :p3

:t64
set D=%D64%
set P=%P64%
set DX=%D32%
set PX=%P32%
set TX=32
goto :p3

:p3
git.exe --version 2>nul
if not %ERRORLEVEL%==0 goto :git_done
for /f %%b in ('git.exe rev-parse --abbrev-ref HEAD') do set GITHASH=%%b
for /f %%b in ('git.exe log -1 "--pretty=format:%%cs_%GITHASH%@%%h"') do set GITHASH=%%b
git.exe diff --quiet
if %ERRORLEVEL%==1 set GITHASH=%GITHASH%*
:git_done
@echo on

:config.h
echo>..\config.h #define NOOC_VERSION "%VERSION%"
if not (%GITHASH%)==() echo>> ..\config.h #define NOOC_GITHASH "%GITHASH%"
@if not (%BINDIR%)==(%NOOCDIR%) echo>> ..\config.h #define CONFIG_NOOCDIR "%NOOCDIR:\=/%"
if %TX%==64 echo>> ..\config.h #ifdef NOOC_TARGET_X86_64
if %TX%==32 echo>> ..\config.h #ifdef NOOC_TARGET_I386
echo>> ..\config.h #define CONFIG_NOOC_CROSSPREFIX "%PX%-"
echo>> ..\config.h #endif

for %%f in (*nooc.exe *nooc.dll) do @del %%f

@if _%NOOC_C%_==__ goto compiler_2parts
@rem if NOOC_C was defined then build only nooc.exe
%CC% -o nooc.exe %NOOC_C% %D%
@goto :compiler_done

:compiler_2parts
@if _%LIBNOOC_C%_==__ set LIBNOOC_C=..\libnooc.c
%CC% -o libnooc.dll -shared %LIBNOOC_C% %D% -DLIBNOOC_AS_DLL
@if errorlevel 1 goto :the_end
%CC% -o nooc.exe ..\nooc.c libnooc.dll %D% -DONE_SOURCE"=0"
%CC% -o %PX%-nooc.exe ..\nooc.c %DX%
:compiler_done
@if (%EXES_ONLY%)==(yes) goto :files_done

if not exist libnooc mkdir libnooc
if not exist doc mkdir doc
copy>nul ..\include\*.h include
copy>nul ..\nooclib.h include
copy>nul ..\libnooc.h libnooc
copy>nul ..\tests\libnooc_test.c examples
copy>nul nooc-win32.txt doc

if exist libnooc.dll .\nooc -impdef libnooc.dll -o libnooc\libnooc.def
@if errorlevel 1 goto :the_end

:lib
call :make_lib %T% || goto :the_end
@if exist %PX%-nooc.exe call :make_lib %TX% %PX%- || goto :the_end

:nooc-doc.html
@if not (%DOC%)==(yes) goto :doc-done
echo>..\config.texi @set VERSION %VERSION%
cmd /c makeinfo --html --no-split ../nooc-doc.texi -o doc/nooc-doc.html
:doc-done

:files_done
for %%f in (*.o *.def) do @del %%f

:copy-install
@if (%NOOCDIR%)==() goto :the_end
if not exist %BINDIR% mkdir %BINDIR%
for %%f in (*nooc.exe *nooc.dll) do @copy>nul %%f %BINDIR%\%%f
if not exist %NOOCDIR% mkdir %NOOCDIR%
@if not exist %NOOCDIR%\lib mkdir %NOOCDIR%\lib
for %%f in (lib\*.a lib\*.o lib\*.def) do @copy>nul %%f %NOOCDIR%\%%f
for %%f in (include examples libnooc doc) do @xcopy>nul /s/i/q/y %%f %NOOCDIR%\%%f

:the_end
exit /B %ERRORLEVEL%

:make_lib
.\nooc -B. -m%1 -c ../lib/libnooc1.c
.\nooc -B. -m%1 -c lib/crt1.c
.\nooc -B. -m%1 -c lib/crt1w.c
.\nooc -B. -m%1 -c lib/wincrt1.c
.\nooc -B. -m%1 -c lib/wincrt1w.c
.\nooc -B. -m%1 -c lib/dllcrt1.c
.\nooc -B. -m%1 -c lib/dllmain.c
.\nooc -B. -m%1 -c lib/chkstk.S
.\nooc -B. -m%1 -c ../lib/alloca.S
.\nooc -B. -m%1 -c ../lib/alloca-bt.S
.\nooc -B. -m%1 -c ../lib/stdatomic.c
.\nooc -B. -m%1 -ar lib/%2libnooc1.a libnooc1.o crt1.o crt1w.o wincrt1.o wincrt1w.o dllcrt1.o dllmain.o chkstk.o alloca.o alloca-bt.o stdatomic.o
.\nooc -B. -m%1 -c ../lib/bcheck.c -o lib/%2bcheck.o -bt
.\nooc -B. -m%1 -c ../lib/bt-exe.c -o lib/%2bt-exe.o
.\nooc -B. -m%1 -c ../lib/bt-log.c -o lib/%2bt-log.o
.\nooc -B. -m%1 -c ../lib/bt-dll.c -o lib/%2bt-dll.o
exit /B %ERRORLEVEL%
