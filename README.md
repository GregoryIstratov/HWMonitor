**HWMonitor v0.05**

This little program uses ncurses to gather information from various sources of your Linux system like disk R/W speed, disk transactions, network Tx/Rx Speed, packages,  memory usage, commits in memory, cached/buffered/swapped memory. About your CPU per core load, and so on.

My purpose is to create all in one utility like **htop**, that will refresh data in realtime in one screen.
 
The programm written in pure C99/11 and almost doesn't use external libs except cores one. So it's a reall big probability to compile it on from sources just a few actions, without installing bunch of trashy libs and resolving dependencies.


All you need is Linux, compiler and some core libs like ncurses, which probably is already installed. 