# COP4610 Project 2: Kernel Module Programming
System Calls, Kernel Module, and Elevator Scheduling

## Team Members
- Tomas Munoz-Moxey
- Andrew Perez-Napan
- John Washer

## Division of Labor
### Core Features
- Part 1: System Call Tracing - **Andrew**
- Part 2: Kernel Module - **Andrew**
- Part 3: Elevator Schedule
    - **Tomas**
        - Foundational architecture
        - Procfile setup
        - Unit testing
    - **Andrew**
        - Elevator loading/unloading
        - Unit testing
    - **John**
        - Elevator movement
        - Makefile creation
        - Unit testing

### Git Log
See screenshot [here](https://www.dropbox.com/s/5hezuq08oh0e0cz/git_log_proj2.png?dl=0).  
Please note that due to differing experiences with source control, the git log may not accurately reflect the division of labor outlined above. Additionally one final commit by John will not be present, but will contain only the final updates to this file.

## Archive Contents
`README.md` contains a brief breakdown of the project and relevant information for grading.

##### Part 1
`empty.c` is an empty C program.  
`empty.trace` is the system call tracer that monitors interaction between `empty` and kernel.  
`part1.c` is a C program with 4 more system calls than `empty`.  
`part1.trace` is the system call tracer that monitors interaction between `part1` and kernel.

##### Part 2
`Makefile` creates `my_timer` kernel object and its dependencies.  
`my_timer.c` is a C program used to create a kernel module called `my_timer` that stores both the current kernel time and elapsed kernel time since the last call using the proc interface.

##### Part 3
`elevator.c` is a C program used to create a kernel module called `elevator` that contains a scheduling algorithm for an elevator.  
`Makefile` creates `elevator` kernel object and its dependencies as well as a `watch_proc` command to monitor the elevator's status in the terminal.  
`syscalls.c` contains the three unique system call STUBs used in `elevator`.

## Compilation and Execution
Part 1 was completed on `cs.linprog.fsu.edu`. All other testing was done on `Ubuntu 16.04.6` using kernel versions `4.15.0` (Part 2) and `4.19.98` (Part 3) in the `/usr/src` directory.

##### Part 1
To create trace files for comparison:
```
gcc -o empty.x empty.c
gcc -o part1.x part1.c
strace -o empty.trace ./empty.x
strace -o part1.trace ./part1.x
```

##### Part 2
To install `my_timer`:
```
sudo make
sudo insmod my_timer.ko
```  
To test, execute `cat /proc/timer` at desired intervals.
To remove, execute `sudo rmmod my_timer.ko`.
To clean, execute `sudo make clean`.

##### Part 3
To install `elevator`:
```
sudo make
sudo insmod elevator.ko
```  
To test, execute `make watch_proc` and interact with given `consumer.c` and `producer.c`.
To remove, execute `sudo rmmod elevator.ko`.
To clean, execute `sudo make clean`.

## Known Bugs and Unfinished Portions
No known bugs exist in this project.