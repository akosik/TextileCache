# SocketToMe
##### Alec Kosik and Erik Lopez
<br />
```
Disclaimer:
Compiled and tested on Mac. Those not using Mac will need to add -std=c99 flag in makefile.

```


<br />
###STEP 1 
####TCP server:
For the TCP implementation we used Port 2001 (description of port:CAPTAN Test Stand System).
We passed our simple tests on the TCP server using curl. We used raw sockets in our implementtation of TCP.


###STEP 2
####TCP client:



###STEP 3
####UDP:
The UDP implementation we used was on Port 3001 (description of port: Miralix Phone Monitor).
When timing 1000 GET requests on TCP we recorded 18ms per request, and roughly 12ms per request when timing on UDP. The variability on UDP was noticeably higher than TCP when sending multiple 1000 GET requests one right after the other. The 1000 GET requests tests resulted in no misses, but when sending packets with a size of 10,000 bytes we it loops and sends huge keys
