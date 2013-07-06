# Remote
Remote is a client/server application designed to control some desktop computer
features using an android phone as a remote control.

Remote should mainly allow to control media player.

## Build
### Server
To build the server, you will need libjansson-dev, glib2.0 and optionally gstreamer-1.0 (this last one
is if you want to use the streaming to phone option, which seems pretty useless anyway). Once the
dependencies satisfied, go into remote-server and `make setup` (to pull lib smdp) then `make`.
You can enable/disable build options in the makefile.

### Client
To build the client, you will need an up-to-date [android-sdk](http://developer.android.com/sdk/index.html),
and [android-ndk](http://developer.android.com/tools/sdk/ndk/index.html).
You should then copy the local.properties.default file to local.properties,
and edit it to point to your android sdk.
You can then use the Makefile to setup the gstreamer sdk for android with `make setup`.
Once the sdk is installed, use `make` to build the client, and `make install` to upload
it on your phone.

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

The default config file allows to control clementine via the 'music' profile, vlc via
the 'video' profile, and in a very basic way, xbmc via the 'xbmc' profile.
