#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <unistd.h>

#define SOCK_PATH "/run/torchd.sock"

int main(int argc, char **argv) {
	if (argc > 1) {
		const char allowed_commands[3][10] = {"off", "torch", "red"};
		int ok = 0;
		for (int i = 0; i < 3; i++) {
			if (strcmp(allowed_commands[i], argv[1]) == 0) {
				ok = 1;
				break;
			}
		}
		if (!ok) {
			printf("Unknown command `%s`; available commands are 'off', 'torch', 'red'.\n", argv[1]);
			return 1;
		}
		int fd = socket(AF_UNIX, SOCK_STREAM, 0);
		if (fd < 0) {
			perror("failed to create socket");
			return 1;
		}
		struct sockaddr_un addr;
		memset(&addr, 0, sizeof(struct sockaddr_un));
		addr.sun_family = AF_UNIX;
		strncpy(addr.sun_path, SOCK_PATH, sizeof(addr.sun_path) - 1);
		
		if (connect(fd, (struct sockaddr *) &addr, sizeof(struct sockaddr_un)) < 0) {
			perror("failed to connect socket");
			close(fd);
			return 1;
		}
		
		int len = strlen(argv[1]);

		if (write(fd, argv[1], len) != len) {
			perror("failed to write");
			close(fd);
			return 1;
		}
		
		close(fd);
		return 0;
	}
	printf("No command supplied.\n");
	return 1;
}
