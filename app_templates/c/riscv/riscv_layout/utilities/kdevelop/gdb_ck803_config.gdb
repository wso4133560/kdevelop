set pagination off
set confirm off
set breakpoint pending on
set remotetimeout 10
directory .
directory src/TC_adda_%{APPNAMELC}/c
file Obj/%{APPNAMELC}.elf
target remote localhost:39000
monitor reset halt
