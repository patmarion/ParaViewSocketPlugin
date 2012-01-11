ParaView Remote Control Plugin

This plugin adds a dock panel named 'Remote Control' to the ParaView
GUI.  The 'Remote Control' can be used to create one or more socket
connections that can be used to remotely control the ParaView GUI.
All data received on the socket connection will be treated as python
code and will be executed in the ParaView python shell.  Output will
be displayed in the ParaView python shell but will not be transmitted
back over the socket.

Example usage:

Click the 'New' button in the 'Remote Control' dock panel.  Set the
socket type to 'server' and set the port number to 9000.  Click
'Listen'.  Open a new command line terminal and use the netcat command
(available in most unix environments) to connect to the waiting
socket:

    netcat localhost 9000

Now enter python commands into the netcat terminal:

  Sphere()
  Show()
  Render()
