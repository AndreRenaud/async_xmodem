#ifndef XMODEM_SERVER_H
#define XMODEM_SERVER_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define XMODEM_SOH 0x01
#define XMODEM_EOT 0x04
#define XMODEM_ACK 0x06
#define XMODEM_NACK 0x17

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

#define XMODEM_PACKET_SIZE 128

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
	xmodem_tx_byte tx_byte;
	void *cb_data;
};

static inline int xmodem_server_init(struct xmodem_server *xdm, xmodem_tx_byte tx_byte, void *cb_data) {
	if (tx_byte == NULL)
		return -1;
	memset(xdm, 0, sizeof(*xdm));
	xdm->tx_byte = tx_byte;
	xdm->cb_data = cb_data;

	xdm->tx_byte(xdm, 'C', xdm->cb_data);

	return 0;
}

static inline uint16_t xmodem_server_crc(uint16_t crc, uint8_t byte)
{
	crc = crc ^ ((uint16_t)byte) << 8;
	for (int i = 8; i > 0; i--) {
		if (crc & 0x8000)
			crc = crc << 1 ^ 0x1021;
		else
			crc = crc << 1;
	}
	return crc;
}

/**
 * Send a single byte to the xmodem state machine
 * @returns true if a packet is now available for processing, false if more data is needed
 */
bool xmodem_server_rx_byte(struct xmodem_server *xdm, uint8_t byte);
static inline xmodem_server_state xmodem_server_get_state(struct xmodem_server *xdm) {
	return xdm->state;
}
const char *xmodem_server_state_name(struct xmodem_server *xdm);
int xmodem_server_tick(struct xmodem_server *xdm, int64_t ms_time);

static inline bool xmodem_server_get_packet(struct xmodem_server *xdm, uint8_t *packet, uint32_t *block_num) {
	if (xdm->state != XMODEM_STATE_PROCESS_PACKET)
		return false;
	memcpy(packet, xdm->packet_data, XMODEM_PACKET_SIZE);
	*block_num = xdm->block_num;
	xdm->block_num++;
	xdm->state = XMODEM_STATE_SOH;
	xdm->tx_byte(xdm, XMODEM_ACK, xdm->cb_data);
	return true;
}

static inline bool xmodem_server_is_done(struct xmodem_server *xdm) {
	return xdm->state == XMODEM_STATE_SUCCESSFUL || xdm->state == XMODEM_STATE_FAILURE;
}

#endif