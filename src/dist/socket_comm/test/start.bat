@ECHO OFF
@rem ********************************************************************************
@rem  Copyright (c) 2007-2014 Intel Corporation. All rights reserved.              **
@rem                                                                               **
@rem  Redistribution and use in source and binary forms, with or without           **
@rem  modification, are permitted provided that the following conditions are met:  **
@rem    * Redistributions of source code must retain the above copyright notice,   **
@rem      this list of conditions and the following disclaimer.                    **
@rem    * Redistributions in binary form must reproduce the above copyright        **
@rem      notice, this list of conditions and the following disclaimer in the      **
@rem      documentation and/or other materials provided with the distribution.     **
@rem    * Neither the name of Intel Corporation nor the names of its contributors  **
@rem      may be used to endorse or promote products derived from this software    **
@rem      without specific prior written permission.                               **
@rem                                                                               **
@rem  This software is provided by the copyright holders and contributors "as is"  **
@rem  and any express or implied warranties, including, but not limited to, the    **
@rem  implied warranties of merchantability and fitness for a particular purpose   **
@rem  are disclaimed. In no event shall the copyright owner or contributors be     **
@rem  liable for any direct, indirect, incidental, special, exemplary, or          **
@rem  consequential damages (including, but not limited to, procurement of         **
@rem  substitute goods or services; loss of use, data, or profits; or business     **
@rem  interruption) however caused and on any theory of liability, whether in      **
@rem  contract, strict liability, or tort (including negligence or otherwise)      **
@rem  arising in any way out of the use of this software, even if advised of       **
@rem  the possibility of such damage.                                              **
@rem ********************************************************************************
@rem 

@rem
@rem start.bat, this is the script file to perform startup activities
@rem when running with distCnC over sockets. More information can be
@rem found in the runtime_api documentation: CnC for distributed memory.
setlocal

set mode=%1
set contactString=%2

REM !!! Adjust the number of expected clients here !!!
set _N_CLIENTS_=3

REM !!! Adjust path names here !!!
rem set __my_debugger="%VS90COMNTOOLS%..\IDE\devenv" /debugexe
set __my_debugger=
set __my_exe="%CNC_SOCKET_HOST_EXECUTABLE%"

REM !!! Client startup command !!!
REM !!! There must be one for each client 1,2,...,_N_CLIENTS_ !!!
rem set _CLIENT_CMD1_=%__my_debugger% /debugexe %__my_exe%
set _CLIENT_CMD1_=%__my_debugger% %__my_exe%
set _CLIENT_CMD2_=%__my_debugger% %__my_exe%
set _CLIENT_CMD3_=%__my_debugger% %__my_exe%
set _CLIENT_CMD4_=%__my_debugger% %__my_exe%
set _CLIENT_CMD5_=%__my_debugger% %__my_exe%
set _CLIENT_CMD6_=%__my_debugger% %__my_exe%
set _CLIENT_CMD7_=%__my_debugger% %__my_exe%
set _CLIENT_CMD8_=%__my_debugger% %__my_exe%
set _CLIENT_CMD9_=%__my_debugger% %__my_exe%

REM -- NOTE --
REM When debugging on Windows, you may want to start the client directly
REM in the Visual Studio debugger. This is possible in the following way:
REM at the beginning of _CLIENT_CMD<id>_, add
REM     "%VS80COMNTOOLS%..\IDE\devenv" /debugexe
REM (for Visual Studio 2005) resp.
REM     "%VS90COMNTOOLS%..\IDE\devenv" /debugexe
REM (for Visual Studio 2008).
REM Example:
REM   set _CLIENT_CMD1_="%VS80COMNTOOLS%..\IDE\devenv" /debugexe "C:\my_proj\Release\my_exe.exe"
REM This starts the Visual Studio with the client executable loaded,
REM but does not yet run the executable.
REM You may want to load a source file of the client executable in the
REM Visual Studio and set some breakpoints. Then start debugging the client
REM executable (Debug->Start Debugging). It starts running and will
REM be stopped in the first breakpoint.
REM When closing this Visual Studio session, you may want to save it
REM under the suggested default name, so that you'll have all breakpoints
REM already set during a subsequent debugging session.
REM -- END NOTE --

REM========================================================================

REM Special mode: print number of expected clients
if "%mode%" == "-n" (
    echo %_N_CLIENTS_%
    goto LABEL_END
)

REM Normal mode: start the client with given id (1,2,...,_N_CLIENTS_).
REM First determine relevant client command (from _CLIENT_CMD<clientId>_).
set clientId=%mode%
for /f "usebackq delims== tokens=1" %%i in (`set _CLIENT_CMD%clientId%_`) do set clientCmdSpecified=%%i
for /f "usebackq delims== tokens=2" %%i in (`set _CLIENT_CMD%clientId%_`) do set clientCmd=%%i
@if "%clientCmdSpecified%" == "" (
    echo *** ERROR %0: no command _CLIENT_CMD%clientId%_ specified
) else (
    set CNC_SOCKET_CLIENT=%contactString%
    set CNC_SOCKET_CLIENT_ID=%clientId%
    %clientCmd%
)

REM Cleanup:
:LABEL_END
endlocal
