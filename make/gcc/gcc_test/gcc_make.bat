@ECHO OFF

set USER_PATH=%cd%
set BUILD_LOG=build\gcc\build.log
set BUILD_CFG=build\gcc\app_image_bin.cfg
set LIB_CFG=libs\app_image_bin.cfg
set TEST_PATH=build\gcc\TEST
set SDK_LIB_SRC=build\gcc\obj\sdk_lib\src\
set BASIC_DIR=%TEST_PATH%\basic
set ADVANCE_DIR=%TEST_PATH%\advance
set USER_MAKEFILE=make\gcc\gcc_test\gcc_makefile
set ADVANCE_LIST=atc_pipe file_stress simple_tcp tcp_stress fota_stress\fota_ftp fota_stress\fota_http
set BASIC_LIST=api_test secure_data voice_sms watchdog %ADVANCE_LIST% bluetooth audio

IF /i "%1" == "newlib" (
		@copy /y %LIB_CFG% %BUILD_CFG%
		make\make.exe OPTION= -f %USER_MAKEFILE%  2> %BUILD_LOG%
		@cd %SDK_LIB_SRC%
		@if exist app_start.lib del /q app_start.lib
		REM C:\Progra~1\CodeSourcery\Sourcery_CodeBench_Lite_for_ARM_EABI\arm-none-eabi\bin\ar.exe -r app_start.lib *.o
		%USER_PATH%\make\gcc\gcc_test\ar.exe -r app_start.lib *.o
		@cd %USER_PATH%
) ELSE IF /i "%1" == "test_basic" (
	for %%i in (%BASIC_LIST%) do (
		@echo #### %%i ####
		make\make.exe clean -f %USER_MAKEFILE%		
		@copy /y %LIB_CFG% %BUILD_CFG%
		make\make.exe OPTION=%%i -f %USER_MAKEFILE%  2> %BUILD_LOG%
		@md %BASIC_DIR%\%%i
		@copy /y build\gcc\ %BASIC_DIR%\%%i\
		@del /q build\gcc\
	)
) ELSE IF /i "%1" == "test_advance" (
	for %%j in (%ADVANCE_LIST%) do (
		@echo #### %%j ####
		make\make.exe clean -f %USER_MAKEFILE%
		@copy /y %LIB_CFG% %BUILD_CFG%
		make\make.exe OPTION=%%j -f %USER_MAKEFILE%  2> %BUILD_LOG%
		@md %ADVANCE_DIR%\%%j
		@copy /y build\gcc\ %ADVANCE_DIR%\%%j\
		@del /q build\gcc\
	)
) ELSE (
	IF /i "%1" == "clean" (
		make\make.exe %1 -f %USER_MAKEFILE%
		IF EXIST %BUILD_LOG% (
			@del /f %BUILD_LOG%
		)
		IF EXIST %BUILD_CFG% (
			@del /f %BUILD_CFG%
		)
		IF EXIST %TEST_PATH% (
			@rd /q /s %TEST_PATH%
		)
	) ELSE (
		@echo #####"%1"#####
		for %%k in (%BASIC_LIST%) do (
			IF /i "%1" == "%%k" (
				IF NOT EXIST build\gcc\%%k (
					@md build\gcc\%%k
				) ELSE (
					@del /q /s build\gcc\%%k
				)
				make\make.exe clean -f %USER_MAKEFILE%
				@copy /y %LIB_CFG% %BUILD_CFG%
				make\make.exe OPTION=%%k -f %USER_MAKEFILE%  2> %BUILD_LOG%
				@copy /y build\gcc\ build\gcc\%%k\
				@del /q build\gcc\
			)
		)
	)
)