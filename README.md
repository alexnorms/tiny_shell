# tiny_shell
Tiny Linux-like shell written in C

To run, clone on Linux machine, compile and run ./tiny_shell

Takes commands *history*, *chdir* and *limit* as process internal commands
- history shows last 100 commands run
- chdir changer directory: must be run with valid dir path (i.e chdir ./home)
- limit sets avail memory size

External commands such as *pwd* and *ls* also run
