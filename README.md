# Asynchonous XModem Receiver
This is a minimal C implementation of the XModem transfer protocol.
It is designed to be used in bare-metal embedded systems with no
underlying operating system. It is asynchonous (non-blocking), and
all data is either directly supplied or sent out via callbacks,
with no OS-level dependencies.

## Usage
This is a simple single .c & .h file, designed to be directly imported into
most existing code bases without issue.

For most usage, there are four functions which are of interest
* xmodem_server_init - initialise the state, and provide the callback for
transmitting individual response bytes
* xmodem_server_process - check for timeouts, and extra the next packet
if available
* xmodem_server_is_done - indicates when the transfer is completed
* xmodem_server_get_state - get the specific state of the transfer
(including success/failure)

## Example
```c
struct xmodem_server xdm;

xmodem_server_init(&xdm, uart_tx_char, NULL);
while (!xmodem_server_is_done(&xdm)) {
	uint8_t resp[XMODEM_PACKET_SIZE];
	uint32_t block_nr;
	if (xmodem_server_process(&xdm, resp, &block_nr, ms_time()))
		handle_incoming_packet(resp, block_nr);
}
if (xmodem_server_get_state(&xdm) == XMODEM_STATE_FAILURE)
	handle_transfer_failure();
```

## License
This code is licensed using the [Unlicense](https://unlicense.org/) - do
what you want with it.