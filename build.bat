@ECHO OFF
setlocal
set VARS_SET=0
if not "%VS100COMNTOOLS%x"=="x" ((call "%VS100COMNTOOLS%\vsvars32.bat") && (set VARS_SET=1))
if "%VARS_SET%"=="0" (
	if not "%VS90COMNTOOLS%x"=="x" ((call "%VS90COMNTOOLS%\vsvars32.bat") && (set VARS_SET=1))
)
if "%VARS_SET%"=="0" (
	if not "%VS80COMNTOOLS%x"=="x" ((call "%VS80COMNTOOLS%\vsvars32.bat") && (set VARS_SET=1))
)
if not exist "%~dp0obj" mkdir "%~dp0obj"
cl.exe %CFLAGS% -Foobj\compile.obj -Isrc -Iinclude -Isrc src\compile.c -link -LTCG -OPT:REF -OPT:ICF -LIBPATH:lib
if errorlevel 1 (
	echo Failed to compile the compilation automation utility.
	goto CleanUp
)
"%~dp0compile.exe"
if errorlevel 1 (
	echo Failed to compile the bat2exe/stub.exe
	goto CleanUp
)
"%~dp0bat2exe.exe" -l "%~nx0" "%~n0.exe"
goto CleanUp
:CleanUp
endlocal
goto :EOF