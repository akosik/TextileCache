# SocketToMe
##### Alec Kosik and Erik Lopez
<br />
#####Disclaimer:
```
Compiled and tested on Mac.
Those running on an operating system other than Mac will need to add -std=c99 flag in makefile.

```


<br />
###STEP 1 
####TCP server:
For the TCP implementation we used Port 2001 (description of port: CAPTAN Test Stand System).
We passed our simple tests on the TCP server using curl. We used raw sockets in our implementtation of TCP.


###STEP 2
####TCP client:



###STEP 3
####UDP:
The UDP implementation we used was on Port 3001 (description of port: Miralix Phone Monitor).
When timing 1000 GET requests on TCP we recorded 18ms per request, and roughly 10ms per request when timing on UDP. The variability on UDP was noticeably higher with increased response times when sending multiple 1000 GET requests back to back. This change could be due to increased network traffic caused by the number of GET requests. The 1000 GET requests tests resulted in no misses, but it should be noted that when sending a buffer with 10000 bytes causes some problems.
