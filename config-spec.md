## Config file
The configuration file uses the json format.
The top level item must be either an object or an array of object. Each
one of this object represents a dbus interface, it must then hold the
following mandatory members :
+ key : "app", value : `string`. Mandatory. Holds the name of the application owning this
interface. Not used by the dbus protocol, but used internaly by Remote.
+ key : "dbus-name", value : `string`. Mandatory. Holds the well-known bus name of the
interface, as defined by the dbus protocol.
+key : "path", value : `string`. Mandatory. Holds the object path of the interface,
as defined by the dbus protocol.
+key : "interface", value : `string`. Mandatory. Holds the interface name.
+key : "cmds", value : `object`. Holds an object associating dbus methods
to Remote commands (as defined in protocol.md).
+key : "feedback", value : `object`. Holds an object associating dbus signals
or properties, to Remote feedback (as defined in protocol.md).

## Commands object
A command object must hold one or more key : `string` members.
The key must be a valid Remote command, and string must be a valid
method name (without the parenthesis) in the defined interface.
The method must not ask for any parameters.

## Feedback object
A feedback object must hold one or more key : `string` members.
The key must be a valid Remote feedback, and string must be a valid
property name (signals are not yet supported).  
Currently, the only way to get feedback from a media player supported
by this application, is to associate the `PLAYING` feedback, to
a `Metadata` property which corresponds to the `Metadata` property as
defined by the [mpris specification](http://specifications.freedesktop.org/mpris-spec/2.2/),
ie : `"PLAYING" : "Metadata"`
