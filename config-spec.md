## Config file
The configuration file uses the json format.
The top level item must be either an object or an array of object. Each
one of these objects represents a dbus interface, it must then hold the
following mandatory members :
+ key : "app", value : `string`. Mandatory. Holds the name of the application owning this
interface. Not used by the dbus protocol, but used internally by Remote.
+ key : "dbus-name", value : `string`. Mandatory. Holds the well-known bus name of the
interface, as defined by the dbus protocol.
+key : "path", value : `string`. Mandatory. Holds the object path of the interface,
as defined by the dbus protocol.
+key : "interface", value : `string`. Mandatory. Holds the interface name.
+key : "cmds", value : `object`. Holds an object associating dbus methods
to Remote commands (as defined in protocol.md).
+key : "feedback", value : `string`. Holds a dbus protocol used to get
feedback on the state of the player. Currently the only supported value
is 'mpris'. The dbus object specified by the previous properties must
implement the mpris protocol.

## Commands object
A command object must hold one or more key : `string` members.
The key must be a valid Remote command, and string must be a valid
method name (without the parenthesis) in the defined interface.
The method must not ask for any parameters.

## Profiles
It is also possible to define a profile by enclosing the previous dbus object/arrays
into an object containing :
+ key : "profile-name", value `string`. Mandatory.
+ key : "profile-data", value : `object | array`. Mandatory, holds the dbus configuration
In this case the same command can be assigned different calls in different profiles, and
depending on the active profile, the action associated to a command will change.
This is useful to control different type of media application and allow the remote to switch
between those applications without any action on the server. (eg music/video/slides)
