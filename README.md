# Asynchonous XModem Receiver
This is a minimal C implementation of the XModem transfer protocol.
It is designed to be used in bare-metal embedded systems with no
underlying operating system. It is asynchonous (non-blocking), and
all data is either directly supplied or sent out via callbacks,
with no OS-level dependencies.

