# SocketToMe
##### Alec Kosik and Erik Lopez
<br />
#####Disclaimer:
```

Compiled and tested on Mac/Linux (done by adding -gnu=c99 flag to compile on Linux)
Make file will work on Mac/Linux, no need for any changes if testing on either operating system

./server //will run server side
./client //will run client side
```

To clone this repository:
```git clone --recursive https://github.com/akosik/SocketToMe.git```
Leaving out the recursive flag will make git ignore the submodule jsmn and stunt client compilation.


<br />
###STEP 1 
####TCP server/client:
For the TCP implementation we used Port 2001 (description of port: CAPTAN Test Stand System).
We passed our simple tests on the TCP server using curl. We did not use any libraries when implementing sockets for TCP(no protocol specific transport layer format).

All our tests pass from the last homework: "Testy Cache".

###STEP 2
####UDP:
The UDP implementation we used was on Port 3001 (description of port: Miralix Phone Monitor).
For our tests on timing GET requests, we used Mac.

When timing 1000 GET requests on TCP we recorded 18ms per request, and roughly 10ms per request when timing on UDP. The variability on UDP was noticeably higher with increased response times when sending multiple 1000 GET requests back to back. This change could be due to increased network traffic caused by the number of GET requests. The 1000 GET request tests resulted in no misses.

