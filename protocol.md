# Protocol
## Message structure :
Message size : *unsigned 32 bit integer, big endian* + Message content
## Message content structure :
Message head : *text, utf8 encoded* + (" " + Message body)*
## Message body structure :
Parameter : *text, utf8 encoded* | Value : *signed 64 bit integer, big endian* | Data
## Data structure :
Data size : *unsigned 32 bit integer, big endian* + Data content : bytes

# Valid message :
## From server to client :
- "PLAYBACK" + parameter
- "POSITION" + value
- "PROFILES" + (parameter)*
- "SHUFFLE" + ("ON" | "OFF")
- "TRACK" + "COVERART" + data
- "TRACK" + "LENGTH" + value
- "TRACK" + ("TITLE" | "ARTIST" | "ALBUM") + parameter

## From client to server :
- "PLAY"
- "PAUSE"
- "STOP"
- "NEXT"
- "PREV"
- "POSITION"
- "SLEEP"
- "VOLUMEUP"
- "VOLUMEDOWN"
- "PROFILE" + parameter
