The protocal for this assignment is AF_INET connect by serveice ip and post number.
To compile the program need to add -lpthread at the end, because otherwise the program cannot identify what is pthread.

Up to now, there are few error still remain
-The observer need to open before user
-The unix time don't have anything after the decimal
-Sometime the print on user client will show a repeat two or three times, which I cannot find the reason why.
-when server end the user client will keep repeat printing the last information received
-The daemon does not work in the assignment, evertime when the monitoring action happens it will domp and cause user client keep repeating blank.

The socket had been set up to contain 1024 client at once(never tested that many),I used int array to do contain different socket number, if a thread is exit the socket number in the tarray will be   initialize back to 0 and wait for other socket connection to use.

The code are modified from lab6,7,8 and deamon code were from https://stackoverflow.com/questions/17954432/creating-a-daemon-in-linux

The refinement is server timestamps, the time were successfully set by the server but still have no decimal number after the whole number.



