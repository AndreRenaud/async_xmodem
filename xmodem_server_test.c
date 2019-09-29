#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>

#include "xmodem_server.h"
#include "acutest.h"

static void tx_byte(struct xmodem_server *xdm, uint8_t byte, void *cb_data)
{
	uint8_t *buf = cb_data;
	(void)xdm;
	*buf = byte;
}

static bool rx_packet(struct xmodem_server *xdm, const uint8_t *data, int data_len, int block_nr, int error_rate) {
	uint16_t crc = 0;
	if (data_len > XMODEM_PACKET_SIZE)
		data_len = XMODEM_PACKET_SIZE;
	xmodem_server_rx_byte(xdm, 0x01);
	xmodem_server_rx_byte(xdm, block_nr + 1);
	xmodem_server_rx_byte(xdm, (block_nr + 1) ^ 0xff);
	for (int i = 0; i < data_len; i++) {
		uint8_t b = data[i];
		if (error_rate && (rand() % error_rate) == 0)
			b ^= rand() & 0xff;
		xmodem_server_rx_byte(xdm, b);
		crc = xmodem_server_crc(crc, data[i]);
	}
	for (int i = data_len; i < XMODEM_PACKET_SIZE; i++) {
		xmodem_server_rx_byte(xdm, 0xff);
		crc = xmodem_server_crc(crc, 0xff);
	}
	xmodem_server_rx_byte(xdm, crc >> 8);
	return xmodem_server_rx_byte(xdm, crc & 0xff);
}

static int64_t ms_time(void)
{
	struct timeval  tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static void test_simple(void) {
	struct xmodem_server xdm;
	uint8_t tx_char = 0;

	TEST_CHECK(xmodem_server_init(&xdm, tx_byte, &tx_char) >= 0);
	for (uint32_t i = 0; i < 5; i++) {
		uint8_t data[XMODEM_PACKET_SIZE];
		uint8_t resp[XMODEM_PACKET_SIZE];
		uint32_t block_nr;
		memset(data, i, sizeof(data));
		TEST_CHECK(rx_packet(&xdm, data, sizeof(data), i, 0));
		// Should be fine to receive the same packet twice
		TEST_CHECK(rx_packet(&xdm, data, sizeof(data), i, 0));
		TEST_CHECK(xmodem_server_process(&xdm, resp, &block_nr, ms_time()));
		TEST_CHECK(memcmp(data, resp, sizeof(data)) == 0);
		TEST_CHECK(block_nr == i);
		TEST_CHECK(tx_char == 0x06); // We received an ack
		tx_char = 0;
	}
	xmodem_server_rx_byte(&xdm, 0x04);
	TEST_CHECK(xmodem_server_is_done(&xdm));
}

static void test_errors(void) {
	struct xmodem_server xdm;
	uint8_t tx_char = 0;
	int attempts = 0;

	TEST_CHECK(xmodem_server_init(&xdm, tx_byte, &tx_char) >= 0);
	for (uint32_t i = 0; i < 5; ) {
		uint8_t data[XMODEM_PACKET_SIZE];
		uint8_t resp[XMODEM_PACKET_SIZE];
		uint32_t block_nr;
		memset(data, i, sizeof(data));
		tx_char = 0;
		// Inject 1:1000 rate of bad data bytes
		rx_packet(&xdm, data, sizeof(data), i, 100);
		xmodem_server_process(&xdm, resp, &block_nr, ms_time());
		attempts++;
		//printf("tx char: 0x%x\n", tx_char);
		if (tx_char == 0x06) { // ACK
			TEST_CHECK(memcmp(data, resp, sizeof(data)) == 0);
			TEST_CHECK(block_nr == i);
			i++;
		}
	}
	xmodem_server_rx_byte(&xdm, 0x04);
	TEST_CHECK(xmodem_server_is_done(&xdm));
}

/**
 * Spawn a process using fork/exec and get back the file descriptos to
 * read/write from it
 */
static pid_t spawn_process(char * const args[], int *rd_fd, int *wr_fd)
{
	pid_t pid;
	int pipeto[2];
	int pipefrom[2];

	if (pipe(pipeto) < 0)
		return -1;

	if (pipe(pipefrom) < 0) {
		close(pipeto[0]);
		close(pipeto[1]);
		return -1;
	}

	pid = fork();
	if  (pid < 0) {
		close(pipeto[0]);
		close(pipeto[1]);
		close(pipefrom[0]);
		close(pipefrom[1]);
		return -1;
	}

	if (pid == 0) {
		/* dup pipe read/write to stdin/stdout */
		dup2(pipeto[0], STDIN_FILENO);
		dup2(pipefrom[1], STDOUT_FILENO);
		/* close unnecessary pipe descriptors for a clean environment */
		close(pipeto[0]);
		close(pipeto[1]);
		close(pipefrom[0]);
		close(pipefrom[1]);
		execvp(args[0], args);
		perror( "execlp()" );
		return -1;
	}

	usleep(rand() % 1000000);

	close(pipeto[0]);
	close(pipefrom[1]);
	*wr_fd = pipeto[1];
	*rd_fd = pipefrom[0];
	return pid;
}

static void tx_byte_fd(struct xmodem_server *xdm, uint8_t byte, void *cb_data)
{
	int *fd = cb_data;
	(void)xdm;
	write(*fd, &byte, 1);
}

static void test_lsz(void) {
	uint8_t output_data[4096] = {0};
	uint8_t input_data[4096];
	for (int i = 0; i < 4096; i++) {
		input_data[i] = rand();
	}
	char raw_data_name[20];
	sprintf(raw_data_name, "/tmp/xmodem.XXXXXXX");
	TEST_CHECK(mkstemp(raw_data_name) >= 0);
	FILE *fp = fopen(raw_data_name, "wb");
	TEST_CHECK(fwrite(input_data, sizeof(input_data), 1, fp) == 1);
	fclose(fp);
	char * const args[] = {"lsz", "--xmodem", "--quiet", raw_data_name, NULL};
	int wr_fd = -1, rd_fd = -1;
	struct xmodem_server xdm;
	pid_t pid = spawn_process(args, &rd_fd, &wr_fd);
	uint32_t block_nr;
	TEST_CHECK(pid >= 0);
	TEST_CHECK(xmodem_server_init(&xdm, tx_byte_fd, &wr_fd) >= 0);

	while (!xmodem_server_is_done(&xdm)) {
		fd_set rd_fds, wr_fds;
		FD_ZERO(&rd_fds);
		FD_ZERO(&wr_fds);
		FD_SET(rd_fd, &rd_fds);
		FD_SET(wr_fd, &wr_fds);
		struct timeval tv;
		int max_fd = rd_fd;
		if (wr_fd > max_fd)
			max_fd = wr_fd;
		tv.tv_sec = 0;
		tv.tv_usec = 1000;

		if (select(max_fd + 1, &rd_fds, &wr_fds, NULL, &tv) >= 0) {
			uint8_t resp[XMODEM_PACKET_SIZE];
			if (FD_ISSET(rd_fd, &rd_fds)) {
				uint8_t buffer[32];
				size_t count = read(rd_fd, buffer, sizeof(buffer));
				for (size_t i = 0; i < count; i++) {
					xmodem_server_rx_byte(&xdm, buffer[i]);
				}
			}
			if (xmodem_server_process(&xdm, resp, &block_nr, ms_time())) {
				memcpy(&output_data[block_nr * XMODEM_PACKET_SIZE], resp, XMODEM_PACKET_SIZE);
			}
		}
	}
	TEST_CHECK(xmodem_server_get_state(&xdm) == XMODEM_STATE_SUCCESSFUL);
	TEST_CHECK(block_nr + 1 == sizeof(input_data) / XMODEM_PACKET_SIZE);
	waitpid(pid, NULL, 0);
	unlink(raw_data_name);
	for (uint32_t i = 0; i < sizeof(input_data); i++) {
		if (output_data[i] != input_data[i]) {
			fprintf(stderr, "Diff at %d: 0x%x != 0x%x\n", i, output_data[i], input_data[i]);
		}
	}
	TEST_CHECK(memcmp(output_data, input_data, sizeof(input_data)) == 0);
	close(rd_fd);
	close(wr_fd);
}

TEST_LIST = {
	{"simple", test_simple},
	{"errors", test_errors},
	{"lsz", test_lsz},
	{NULL, NULL},
};
