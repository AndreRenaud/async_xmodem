#ifndef XMODEM_SERVER_H
#define XMODEM_SERVER_H

#include <stdint.h>
#include <stdbool.h>

#define XMODEM_PACKET_SIZE 128

typedef enum {
	XMODEM_STATE_START,
	XMODEM_STATE_SOH,
	XMODEM_STATE_BLOCK_NUM,
	XMODEM_STATE_BLOCK_NEG,
	XMODEM_STATE_DATA,
	XMODEM_STATE_CRC0,
	XMODEM_STATE_CRC1,
	XMODEM_STATE_PROCESS_PACKET,
	XMODEM_STATE_SUCCESSFUL,
	XMODEM_STATE_FAILURE,

	XMODEM_STATE_COUNT,
} xmodem_server_state;

struct xmodem_server;

typedef void (*xmodem_tx_byte)(struct xmodem_server *xdm, uint8_t byte, void *cb_data);

/**
 * This contains the state for the xmodem server.
 * None of its contents should be accessed directly, this structure
 * should be considered opaque
 */
struct xmodem_server {
	xmodem_server_state state; // What state are we in?
	uint8_t packet_data[XMODEM_PACKET_SIZE]; // Incoming packet data
	int packet_pos; // Where are we up to in this packet
	uint16_t crc; // Whatis the expected CRC of the incoming packet
	int64_t last_event_time; // When did we last do something interesting?
	uint32_t block_num; // What block are we up to?
	bool repeating; // Are we receiving a packet that we've already processed?
	uint32_t error_count; // How many errors have we seen?
	xmodem_tx_byte tx_byte;
	void *cb_data;
};

int xmodem_server_init(struct xmodem_server *xdm, xmodem_tx_byte tx_byte, void *cb_data);

/**
 * Send a single byte to the xmodem state machine
 * @returns true if a packet is now available for processing, false if more data is needed
 */
bool xmodem_server_rx_byte(struct xmodem_server *xdm, uint8_t byte);
xmodem_server_state xmodem_server_get_state(const struct xmodem_server *xdm);
const char *xmodem_server_state_name(const struct xmodem_server *xdm);
uint16_t xmodem_server_crc(uint16_t crc, uint8_t byte);

bool xmodem_server_process(struct xmodem_server *xdm, uint8_t *packet, uint32_t *block_num, int64_t ms_time);
bool xmodem_server_is_done(const struct xmodem_server *xdm);

#endif