# Protocol
## Message :
Message size : 1 byte, unsigned integer.
Message value : Less than 255 bytes, ASCII.
Message value content :
## Commands : from client to server.
+ PLAY
+ PAUSE
+ STOP
+ NEXT
+ PREVIOUS
+ VOLUMEUP
+ VOLUMEDOWN
+ SLEEP
+ LIGHTUP
+ LIGHTDOWN

## Feedback : from server to client.
+ PLAYING title
+ PAUSED
+ VOLUME value
