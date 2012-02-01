@ECHO OFF
setlocal
echo This is a generic stub.
set
set /A ARGN=0
goto Loop
:Loop
if "%0x"=="x" goto EndLoop
echo %%%ARGN%=%0
set /A ARGN+=1
shift /0
goto Loop
:EndLoop
endlocal
