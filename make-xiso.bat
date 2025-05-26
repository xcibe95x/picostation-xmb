@echo off
tools\mkpsxiso -y "%~dp0\assets\xiso\isoconfig.xml" -o "%~dp0\build\picostation-menu.bin" -c "%~dp0\build\picostation-menu.cue"