#include "xmodem_server.h"

static const char *state_name(xmodem_server_state state) {
	#define XDMSTAT(a) case XMODEM_STATE_ ##a: return #a
	switch(state) {
		XDMSTAT(START);
		XDMSTAT(SOH);
		XDMSTAT(BLOCK_NUM);
		XDMSTAT(BLOCK_NEG);
		XDMSTAT(DATA);
		XDMSTAT(CRC0);
		XDMSTAT(CRC1);
		XDMSTAT(PROCESS_PACKET);
		XDMSTAT(SUCCESSFUL);
		XDMSTAT(FAILURE);
		default: return "UNKNOWN";
	}
}

bool xmodem_server_rx_byte(struct xmodem_server *xdm, uint8_t byte) {
	//printf("Got char 0x%x in state %s[%d]\n", byte, state_name(xdm->state), xdm->state);
	switch (xdm->state) {
	case XMODEM_STATE_START:
	case XMODEM_STATE_SOH:
		if (byte == XMODEM_SOH)
			xdm->state = XMODEM_STATE_BLOCK_NUM;
		if (byte == XMODEM_EOT) {
			xdm->state = XMODEM_STATE_SUCCESSFUL;
			xdm->tx_byte(xdm, XMODEM_ACK, xdm->cb_data);
		}
		break;
	case XMODEM_STATE_BLOCK_NUM:
		if (byte == ((xdm->block_num + 1) & 0xff)) {
			xdm->state = XMODEM_STATE_BLOCK_NEG;
			xdm->repeating = false;
		}
		else if (byte == (xdm->block_num & 0xff)) {
			xdm->state = XMODEM_STATE_BLOCK_NEG;
			xdm->repeating = true;
		} else {
			xdm->state = (byte == XMODEM_SOH ? XMODEM_STATE_BLOCK_NUM : XMODEM_STATE_SOH);
		}
		break;

	case XMODEM_STATE_BLOCK_NEG: {
		uint8_t neg_block = ~(xdm->block_num + 1) & 0xff;
		if (xdm->repeating)
			neg_block = (~xdm->block_num) & 0xff;
		if (byte == neg_block) {
			xdm->packet_pos = 0;
			xdm->state = XMODEM_STATE_DATA;
		} else {
			xdm->state = (byte == XMODEM_SOH ? XMODEM_STATE_BLOCK_NUM : XMODEM_STATE_SOH);
		}
		break;
	}
	case XMODEM_STATE_DATA:
		xdm->packet_data[xdm->packet_pos++] = byte;
		if (xdm->packet_pos >= XMODEM_PACKET_SIZE)
			xdm->state = XMODEM_STATE_CRC0;
		break;

	case XMODEM_STATE_CRC0:
		xdm->crc = ((uint16_t)byte) << 8;
		xdm->state = XMODEM_STATE_CRC1;
		break;

	case XMODEM_STATE_CRC1: {
		uint16_t crc = 0;
		xdm->crc |= byte;
		for (int i = 0; i < XMODEM_PACKET_SIZE; i++)
			crc = xmodem_server_crc(crc, xdm->packet_data[i]);
		if (crc != xdm->crc) {
			xdm->state = XMODEM_STATE_SOH;
			xdm->tx_byte(xdm, XMODEM_NACK, xdm->cb_data);
		} else {
			xdm->state = XMODEM_STATE_PROCESS_PACKET;
		}
		break;
	}

	default:
		break;
	}
	//printf("Moved to state %s[%d]\n", state_name(xdm->state), xdm->state);

	return (xdm->state == XMODEM_STATE_PROCESS_PACKET);
}

int xmodem_server_tick(struct xmodem_server *xdm, int64_t ms_time)
{
	// Avoid confusion about legitimate '0' times
	if (ms_time == 0)
		ms_time = 1;
	if (xdm->state == XMODEM_STATE_START &&
		(xdm->last_event_time == 0 || ms_time - xdm->last_event_time > 500)) {
		xdm->tx_byte(xdm, 'C', xdm->cb_data);
		xdm->last_event_time = ms_time;
	}
	return 0;
}

const char *xmodem_server_state_name(struct xmodem_server *xdm)
{
	return state_name(xdm->state);
}
