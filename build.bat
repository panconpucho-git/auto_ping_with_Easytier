@echo off
gcc ping.c -o ping.exe -fexec-charset=GBK -liphlpapi -lws2_32
pause