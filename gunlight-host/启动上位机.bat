@echo off
chcp 65001 > nul
title 战术枪灯上位机系统 (Tactical WML HUD)
echo 正在启动 CH32V203 战术枪灯上位机系统...
start "" "%~dp0src-tauri\target\debug\gunlight-host.exe"
exit
