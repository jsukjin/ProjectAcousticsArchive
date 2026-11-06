@echo off
rem ============================================================================
rem  run_tests.bat  --  AcousticsToolkit 테스트 실행 도우미
rem ----------------------------------------------------------------------------
rem  하는 일 (순서대로)
rem    1) cmake 를 찾는다 (PATH 에 없으면 Visual Studio 에 포함된 cmake 를 탐색)
rem    2) build 폴더가 없으면 자동으로 구성(configure)한다
rem    3) Release 로 증분 빌드한다   (이미 최신이면 몇 초 안에 끝남)
rem    4) 메뉴 프로그램(pa_menu.exe)을 실행한다 (테스트 1개 = 실행 파일 1개, 메뉴는 그것들을 골라 실행)
rem
rem  사용법
rem    run_tests.bat              대화형 메뉴(인자 없음): 모든 테스트를 설명과 함께 보여 주고 번호를 골라 실행
rem    run_tests.bat all          e2e 를 제외한 전부 실행
rem    run_tests.bat unit         suite 하나 실행   (unit / hrtf / preprocessor / runtime / e2e)
rem    run_tests.bat list         테스트 목록(설명 포함)만 출력
rem    run_tests.bat rebuild      build 폴더를 지우고 처음부터 다시 구성 + 빌드 (실행은 안 함)
rem    run_tests.bat build        빌드만 하고 종료
rem    run_tests.bat --suite hrtf --filter right_ear     그 밖의 인자는 pa_menu.exe 에 그대로 전달
rem
rem  환경변수
rem    PA_NO_BUILD=1              빌드 단계를 건너뛰고 기존 pa_menu.exe 를 바로 실행
rem    PA_NO_PAUSE=1              종료 시 "아무 키나 누르세요" 대기를 끔 (스크립트/CI 용)
rem
rem  종료 코드: 테스트 FAIL 이 있으면 1, 빌드/환경 문제는 3 이상, 정상은 0
rem ============================================================================
setlocal EnableExtensions

rem 한글이 깨지지 않도록 콘솔 코드페이지를 UTF-8 로 (pa_menu.exe 도 UTF-8 로 출력함)
chcp 65001 >nul

set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"
set "BUILD=%ROOT%\build"
set "EXE=%BUILD%\bin\Release\pa_menu.exe"

rem --- 첫 번째 인자에 따라 동작 결정 ---------------------------------------------
set "ARG1=%~1"
set "RUNARGS=%*"
if /I "%ARG1%"=="help"  goto :usage
if /I "%ARG1%"=="-h"    goto :usage
if /I "%ARG1%"=="--help" goto :usage
if /I "%ARG1%"=="all"          set "RUNARGS=--suite unit --suite hrtf --suite preprocessor --suite runtime"
if /I "%ARG1%"=="unit"         set "RUNARGS=--suite unit"
if /I "%ARG1%"=="hrtf"         set "RUNARGS=--suite hrtf"
if /I "%ARG1%"=="preprocessor" set "RUNARGS=--suite preprocessor"
if /I "%ARG1%"=="runtime"      set "RUNARGS=--suite runtime"
if /I "%ARG1%"=="e2e"          set "RUNARGS=--suite e2e"
if /I "%ARG1%"=="list"         set "RUNARGS=--list"
if /I "%ARG1%"=="build"        set "RUNARGS="
if /I "%ARG1%"=="rebuild"      set "RUNARGS="
rem 인자가 없으면 항상 대화형 메뉴 (콘솔이 아닌 입력 파이프에서도 메뉴가 뜨도록 --menu 를 명시)
if "%ARG1%"=="" set "RUNARGS=--menu"

rem --- 1) cmake 찾기 ---------------------------------------------------------------
set "CMAKE=cmake"
where cmake >nul 2>&1
if errorlevel 1 (
  set "CMAKE="
  for %%E in (Enterprise Professional Community BuildTools) do (
    if not defined CMAKE if exist "%ProgramFiles%\Microsoft Visual Studio\2022\%%E\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" (
      set "CMAKE=%ProgramFiles%\Microsoft Visual Studio\2022\%%E\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    )
  )
)
if not defined CMAKE (
  echo [오류] cmake 를 찾을 수 없습니다. CMake 를 설치하거나 PATH 에 추가해 주세요.
  set "RC=3"
  goto :finish
)

rem --- rebuild: build 폴더 삭제 ------------------------------------------------------
if /I "%ARG1%"=="rebuild" (
  echo [정리] build 폴더를 삭제합니다...
  if exist "%BUILD%" rmdir /s /q "%BUILD%"
)

rem --- 2) 구성(configure): build 폴더가 없을 때만 ---------------------------------------
if not exist "%BUILD%\CMakeCache.txt" (
  echo [구성] cmake 를 구성합니다 ^(Visual Studio 17 2022, x64^)...
  "%CMAKE%" -S "%ROOT%" -B "%BUILD%" -G "Visual Studio 17 2022" -A x64
  if errorlevel 1 (
    echo [오류] cmake 구성에 실패했습니다.
    set "RC=4"
    goto :finish
  )
)

rem --- 3) 빌드(Release, 증분) -------------------------------------------------------
if defined PA_NO_BUILD goto :run
echo [빌드] Release 빌드 중... ^(이미 최신이면 금방 끝납니다^)
if not exist "%BUILD%" mkdir "%BUILD%"
"%CMAKE%" --build "%BUILD%" --config Release > "%BUILD%\build.log" 2>&1
if errorlevel 1 (
  echo [오류] 빌드에 실패했습니다. 오류 줄:
  findstr /I /C:"error" "%BUILD%\build.log"
  echo.
  echo 전체 로그: %BUILD%\build.log
  set "RC=5"
  goto :finish
)
echo [빌드] 완료.

if /I "%ARG1%"=="build"   ( set "RC=0" & goto :finish )
if /I "%ARG1%"=="rebuild" ( set "RC=0" & goto :finish )

rem --- 4) 실행 ----------------------------------------------------------------------
:run
if not exist "%EXE%" (
  echo [오류] %EXE% 가 없습니다. PA_NO_BUILD 를 해제하고 다시 실행해 주세요.
  set "RC=5"
  goto :finish
)
echo.
"%EXE%" %RUNARGS%
set "RC=%ERRORLEVEL%"
goto :finish

:usage
echo 사용법:
echo   run_tests.bat              대화형 메뉴 (모든 테스트 설명 표시 + 번호로 선택 실행)
echo   run_tests.bat all          e2e 제외 전부 실행
echo   run_tests.bat unit         suite 실행: unit / hrtf / preprocessor / runtime / e2e
echo   run_tests.bat list         테스트 목록^(설명 포함^)만 출력
echo   run_tests.bat build        빌드만
echo   run_tests.bat rebuild      build 폴더 삭제 후 처음부터 구성 + 빌드
echo   run_tests.bat ^<기타 인자^>  pa_menu.exe 에 그대로 전달 ^(예: --suite hrtf --filter right_ear^)
echo.
echo   환경변수 PA_NO_BUILD=1 : 빌드 건너뛰기
set "RC=0"
goto :finish

:finish
rem 탐색기에서 더블클릭으로 실행했다면(창이 바로 닫히므로) 키 입력을 기다린다.
rem 스크립트/CI 에서 호출할 때는 PA_NO_PAUSE=1 로 이 대기를 끌 수 있다.
if defined PA_NO_PAUSE goto :done
echo %CMDCMDLINE% | findstr /I /C:"/c" >nul
if not errorlevel 1 (
  echo.
  pause
)
:done
endlocal & exit /b %RC%
