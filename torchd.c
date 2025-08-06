#include <gpiod.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <linux/i2c-dev.h>
#include <i2c/smbus.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sys/un.h>

#define CONSUMER "torchd"
#define SOCK_PATH "/run/torchd.sock"

// On N900, ADP1653 flash controller is connected to GPIO pin 88 of 
// the CPU, which corresponds to gpiochip2, line 24 of libgpiod interface
#define GPIO_CHIP "/dev/gpiochip2"
#define LED_CONTROLLER_EN_LINE 24

// ADP1653 is connected to the second I2C adapter
#define I2C_ADAPTER "/dev/i2c-2"
// ADP1653 has 0x30 address
#define I2C_ADDRESS 0x30
#define I2C_REGISTER 0x0

enum LED_STATE {
	LED_STATE_OFF, LED_STATE_RED, LED_STATE_TORCH
};

struct gpiod_chip *chip = NULL;
struct gpiod_line_request *line = NULL;

int sockfd = -1;
int clientfd = -1;

int get_line() {
	int ret = 1;
	chip = gpiod_chip_open(GPIO_CHIP);
	if (!chip) {
		perror("failed to open GPIO chip");
		return 0;
	}
	struct gpiod_line_settings *settings = gpiod_line_settings_new();
	if (!settings) {
		perror("failed to create line settings");
		ret = 0;
		goto settings_failed;
	}
	gpiod_line_settings_set_direction(settings,
					  GPIOD_LINE_DIRECTION_OUTPUT);

	struct gpiod_request_config *req_conf = gpiod_request_config_new();
	if (!req_conf) {
		perror("failed to create request config");
		ret = 0;
		goto request_config_failed;
	}
	gpiod_request_config_set_consumer(req_conf, CONSUMER);

	struct gpiod_line_config *line_conf = gpiod_line_config_new();
	if (!line_conf) {
		perror("failed to create line config");
		ret = 0;
		goto line_config_failed;
	}
	const unsigned int offset = LED_CONTROLLER_EN_LINE; 
	int res = gpiod_line_config_add_line_settings(line_conf,
						      &offset,
						      1,
						      settings);
	if (res < 0) {
		perror("failed to add line settings");
		ret = 0;
		goto add_settings_failed;
	}

	line = gpiod_chip_request_lines(chip,
					req_conf,
					line_conf);
	if (!line) {
		perror("failed to get GPIO line");	
		ret = 0;
	}

add_settings_failed:
	gpiod_line_config_free(line_conf);
line_config_failed:
	gpiod_request_config_free(req_conf);
request_config_failed:
	gpiod_line_settings_free(settings);
settings_failed:
	if (!ret) {
		gpiod_chip_close(chip);
		chip = NULL;
	}

	return ret;
}

void close_chip() {
	if (line) {
		gpiod_line_request_release(line);
		line = NULL;
	}
	if (chip) {
		gpiod_chip_close(chip);
		chip = NULL;
	}
}

int set_en(int state) {
	if (!line) return 0;
	enum gpiod_line_value val = GPIOD_LINE_VALUE_INACTIVE;
	if (state) val = GPIOD_LINE_VALUE_ACTIVE;
	int ret = gpiod_line_request_set_value(line,
					   LED_CONTROLLER_EN_LINE,
					   val);
	return !ret;
}

int write_i2c(unsigned char val) {
	int file;
	file = open(I2C_ADAPTER, O_RDWR);
	if (file < 0) {
		perror("failed to open I2C adapter");
		return 0;	
	}
	if (ioctl(file, I2C_SLAVE, I2C_ADDRESS) < 0) {
		perror("failed to set slave I2C address");
		close(file);
		return 0;
	}

	int res = i2c_smbus_write_byte_data(file, I2C_REGISTER, val);
	if (res < 0) perror("failed to write value via I2C");
	close(file);
	return !res;
}

void toggle_led(enum LED_STATE state) {
	if (state == LED_STATE_OFF) {
		close_chip();	
	} else {
		int val;
		if (state == LED_STATE_RED) val = 0x7;
		else val = 0x8;
		if (chip == NULL && line == NULL) {
			if (!get_line()) return;
		}
		if (!set_en(1)) {
			perror("failed to set EN pin state");
			close_chip();
			return;
		}
		if (!write_i2c(val)) {
			close_chip();
			return;
		}
	}	
}

void quit(int sig) {
	close_chip();
	if (sockfd != -1) close(sockfd);
	if (clientfd != -1) close(clientfd);
	exit(0);
}

int main(int argc, char **argv) {
	if (!get_line()) return 1;
	close_chip();
	
	struct sigaction act = {
		.sa_handler = quit
	};

	sigaction(SIGINT, &act, NULL);
	sigaction(SIGTERM, &act, NULL);

	sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sockfd < 0) {
		perror("failed to create a socket");
		return 1;
	}
	
	if (remove(SOCK_PATH) < 0 && errno != ENOENT) {
		perror("failed to delete a file in the socket path");
		goto failure;
	}

	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(struct sockaddr_un));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, SOCK_PATH, sizeof(addr.sun_path) - 1);
	
	if (bind(sockfd, (struct sockaddr *) &addr, sizeof(struct sockaddr_un)) < 0) {
		perror("failed to bind socket");
		goto failure;
	}

	if (listen(sockfd, 5) < 0) {
		perror("listen failed");
		goto failure;
	}
	
	// allow writing to socket as non-root
	mode_t sock_mode = S_IRUSR |
			   S_IWUSR |
			   S_IRGRP |
			   S_IWGRP |
			   S_IROTH |
			   S_IWOTH;
	if (chmod(SOCK_PATH, sock_mode) < 0) {
		perror("failed setting socket permissions");
		goto failure;
	}

	while (3) {
		clientfd = accept(sockfd, NULL, NULL);

		char buf[11];
		ssize_t num_read;

		while ((num_read = read(clientfd, buf, 10)) > 0) {
			buf[num_read] = '\0';
			if (strcmp(buf, "off") == 0) toggle_led(LED_STATE_OFF);
			else if (strcmp(buf, "torch") == 0) toggle_led(LED_STATE_TORCH);
			else if (strcmp(buf, "red") == 0) toggle_led(LED_STATE_RED);
		}
		close(clientfd);
		clientfd = -1;
	}

failure:
	if (sockfd != -1) close(sockfd);
	close_chip();
	return 1;
}
