@echo off

cd genie
genie vs2019

if not ["%errorlevel%"]==["0"] (
	pause
	exit /b %errorlevel%
)
