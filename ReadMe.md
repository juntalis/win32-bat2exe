#Bat2Exe

Wanted to try my hand at making one really quick, and I figured I'd share for anyone who didn't know how existing compilers work.

Run build.bat, and it should compile. If not, let me know. The current source uses the [Snappy compression lib](http://code.google.com/p/snappy/) which I've added a compiled lib for.

The compiled stub.exe uncompresses an embedded bat file from its resources at runtime, then executes it in the current console before deleting the temporary file created.

The compiled bat2exe.exe file has a copy of stub.exe embedded in its resource file. On runtime, it extracts stub.exe, then updates the existing embedded .bat script with the new script that the user specified. 

Sorry for the crappy code and lack of documentation.