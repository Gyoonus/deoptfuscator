# dt_fd_forward

dt_fd_forward is a jdwpTransport library. It implements the [Java Debug Wire
Protocol Transport Interface
(jdwpTransport)](https://docs.oracle.com/javase/7/docs/technotes/guides/jpda/jdwpTransport.html).
It allows one to handle and proxy JDWP traffic by supplying the implementation
for Attach. This transport requires an address. The address is a single integer
value that is the file-descriptor of an open AF\_UNIX socket.

When this transport begins listening or attaching it will send the
null-terminated string "dt_fd_forward:START-LISTEN\0" over the given socket.

When this transport stops listening for connections it will send the
null-terminated string "dt_fd_forward:END-LISTEN\0" over the socket.

When this transport has successfully received fds from the proxy it sends the
message "dt_fd_forward:ATTACHED\0" over the socket.

When this transport has closed its copies of the fds it will send the proxy the
message "dt_fd_forward:CLOSING\0" over the socket.

When this transport accepts or attaches to a connection it will read from the
socket a 1 byte message and 3 file-descriptors. The file descriptors are, in
order, an fd that will be read from to get incoming JDWP packets (read\_fd\_),
an fd that outgoing JDWP packets will be written to (write\_fd\_), and an
_eventfd_ (write\_lock\_fd\_). The eventfd should not have any flags set. Prior
to writing any data to write\_fd\_ the transport will _read_ from the
write\_lock\_fd\_ and after finishing the write it will _write_ to it. This
allows one to safely multiplex data on the write\_fd\_.

This transport implements no optional capabilities, though this may change in
the future.
