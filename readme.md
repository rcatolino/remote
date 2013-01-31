# Remote
Remote is a client/server application designed to control some desktop computer
features using an android phone as a remote control.

Remote should mainly allow to control media player.

## Build
To build the server, you will need libjansson-dev and glib2.0. Once the
dependencies satisfied, go into remote-server and `make`.

To build the client, you will need a working [android-sdk](http://developer.android.com/sdk/index.html).
You should then copy the local.properties.default file to local.properties,
and edit it to point to your android sdk.
Then `ant debug` and install it on your phone using [adt](http://developer.android.com/tools/building/building-cmdline.html#RunningOnDevice).

## Design
The client should remain as much as possible unaware of the desktop app being
controlled. This is so that the server can easily be modified, through a config
file, to control any media playing application. The client then only sends
generic commands that can apply to multiple type of media playing, such as PLAY,
PAUSE, NEXT, PREVIOUS, etc. (see [protocol](https://github.com/rcatolino/remote/blob/master/protocol.md))

Then the server reads the config file to know how to translate these commands
into appropriate dbus calls (see [config-spec](https://github.com/rcatolino/remote/blob/master/config-spec.md)).
You can associate any command to any dbus call that doesn't ask for a input
parameters. You can use a tool such as d-feet to find the calls you want to use.

