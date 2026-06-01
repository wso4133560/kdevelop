set pagination off
set confirm off
set remotetimeout 10
file Obj/%{APPNAMELC}.elf
target remote localhost:39000
monitor reset halt
