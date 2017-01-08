**HWMonitor v0.05**

This little program uses ncurses to show gathered information from various sources of your Linux system like disks R/W speed, disks transactions, networks Tx/Rx Speed, packages,  memory usage, commits in memory, cached/buffered/swapped memory. About your CPU per core load, and so on.

My purpose is to create all in one utility like **htop**, that will refresh data in realtime in one screen.
 
The programm written in pure C11 and almost doesn't use external libs except cores one. So it's a big probability to compile it on from sources just a few actions, without installing bunch of trashy libs and resolving dependencies.


All you need is Linux, compiler and some core libs like ncurses, which probably is already installed. 


**Tested on Arch Linux x86_64 (kernel 4.8.13) with clang 3.9.0, glibc - 2.24**
