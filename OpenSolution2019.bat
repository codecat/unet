@echo off

cd genie
genie vs2017

if not ["%errorlevel%"]==["0"] (
	pause
	exit /b %errorlevel%
)

"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\IDE\devenv.exe" projects/vs2017/UniversalNetworking.sln
