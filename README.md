# udpdisplay

## Description

This android app acts as a primitive networked display server. Pixels can be sent
via UDP to the app to be drawn on the screen. For larger images, jpeg files can be
sent via TCP to be drawn more efficiently.

Example code is included to send the image data to the server.

I use this to send images from an android camera to my Fire TV, using my 
[Timelapse Camera](https://github.com/sanjayrao77/timelapsecamera)
app to take the pictures and upload them to a server. The server can then
forward images to my TV.

## Building and usage

```bash
$ cd android
$ make debug
ls -l /tmp/androidbuild/apk
# install udpdisplay.debug.apk to your android device
```

To send images to the android app,
you'll want to edit the IP in *draw\_udp.sh* and *draw\tcp.sh* first.

Then:
```bash
guilty@ftl:~/src/android/udpdisplay/c$ make
cc -g -Wall -O2   -c -o udpdisplay.o udpdisplay.c
gcc -o udpdisplay udpdisplay.o
guilty@ftl:~/src/android/udpdisplay/c$ ./draw_udp.sh test.jpg 
Listening on port 8081
Connecting to 192.168.1.100:8081
Sent query request
Received reply with 5 bytes
Query dimensions w:1200,h:1737
Listening on port 8081
Connecting to 192.168.1.100:8081
jpegtopnm: WRITING PPM FILE
Drawing x:0, y:0, w:12, h:13 to port 8081
Drawing x:0, y:13, w:12, h:13 to port 8081
Drawing x:0, y:26, w:12, h:13 to port 8081
Drawing x:0, y:39, w:12, h:13 to port 8081
Drawing x:0, y:52, w:12, h:13 to port 8081
Drawing x:0, y:65, w:12, h:13 to port 8081
Drawing x:0, y:78, w:12, h:13 to port 8081
Drawing x:0, y:91, w:12, h:13 to port 8081
Drawing x:0, y:104, w:12, h:13 to port 8081
Drawing x:0, y:117, w:12, h:13 to port 8081
^C
guilty@ftl:~/src/android/udpdisplay/c$ ./draw_tcp.sh test.jpg 
Query dimensions w:1200,h:1737
jpegtopnm: WRITING PPM FILE
Sent header
Sent data, waiting for reply
Got reply
Received OK
guilty@ftl:~/src/android/udpdisplay/c$ exit
```
