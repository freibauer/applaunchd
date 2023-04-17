AGL Application Launcher service reference implementation

`applaunchd` is a simple service for launching applications from other
applications.  It exposes a gRPC RPC interface as described in the file
`protos/applauncher.proto`.  Additionally, there is a now deprecated
`applaunchd-dbus`, which exposes a comparable version of the interface
named 'org.automotivelinux.AppLaunch' on the D-Bus session bus and can
be autostarted by using this interface name.

The interface can be used to:
- retrieve a list of available applications
- request that a specific application be started by using the 'start' method
- subcribe to a status signal (separate 'started' and/or 'terminated' signals
  for the D-Bus implementation) in order to be notified when an application
  has started successfully or terminated.

For more details about the deprecated D-Bus interface, please refer to the
file `data/org.automotivelinux.AppLaunch.xml`.

Applications are enumerated from systemd's list of available units based on
the pattern agl-app*@*.service, and are started and controled using their
systemd unit.  Please note `applaunchd` allows only one instance of a given
application.

Note that while the gRPC and D-Bus implementations are comparable in
functionality, they are not interoperable with respect to status notifications
for applications started by the other interface.  It is advised that their
usage not be mixed in the same image to avoid confusion around application
window activation.

AGL repo for source code:
https://gerrit.automotivelinux.org/gerrit/#/admin/projects/src/applaunchd

You can also clone the source repository by running the following command:
```
$ git clone https://gerrit.automotivelinux.org/gerrit/src/applaunchd
```
