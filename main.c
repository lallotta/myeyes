#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <linux/gpio.h>

int libcamera_vid(const char *outfile)
{
	char *cmd;
	int ret;

	ret = asprintf(&cmd, "libcamera-vid -t 60000 -n -o %s", outfile);
	if (ret == -1) {
		fprintf(stderr, "failed to allocate libcamera-vid command string\n");
		return ret;
	}
	
	ret = system(cmd);
	if (ret != 0) {
		if (ret == -1)
			perror("system");
		else if (WIFEXITED(ret))
			fprintf(stderr, "libcamera-vid encountered an error: exit status: %d\n", WEXITSTATUS(ret));
		else if (WIFSIGNALED(ret)) {
			if (WTERMSIG(ret) == SIGINT)
				fprintf(stderr, "Keyboard interrupt received while recording %s\n", outfile);
			else
				fprintf(stderr, "libcamera-vid terminated by signal: %d\n", WTERMSIG(ret));
		}

		ret = -1;
	}

	free(cmd);
	return ret;	
}

struct motion_sensor {
	int chip_fd;
	unsigned int pin;
	time_t timestamp;
};

struct motion_sensor *pir_init(unsigned int pin)
{
	struct motion_sensor *pir;
	int fd;

	pir = malloc(sizeof(struct motion_sensor));
	if (!pir)
		return NULL;

	fd = open("/dev/gpiochip0", O_RDONLY);
	if (fd == -1) {
		free(pir);
		return NULL;
	}

	pir->chip_fd = fd;
	pir->pin = pin;
	return pir;
}

int pir_wait_for_motion(struct motion_sensor *pir)
{
	struct gpio_v2_line_request req;
	struct gpio_v2_line_event event;
	ssize_t rd;
	int ret;

	memset(&req, 0, sizeof(req));
	req.offsets[0] = pir->pin;
	req.num_lines = 1;
	req.config.flags = GPIO_V2_LINE_FLAG_INPUT | GPIO_V2_LINE_FLAG_EDGE_RISING | GPIO_V2_LINE_FLAG_EVENT_CLOCK_REALTIME;
	strcpy(req.consumer, "myeyes:motion_sensor");

	ret = ioctl(pir->chip_fd, GPIO_V2_GET_LINE_IOCTL, &req);
	if (ret == -1)
		return ret;

	rd = read(req.fd, &event, sizeof(event));
	if (rd == -1)
		return -1;

	if (rd != sizeof(event)) {
		errno = EIO;
		return -1;
	}

	pir->timestamp = event.timestamp_ns / 1000000000UL;
	
	close(req.fd);

	return 0;
}

void pir_release(struct motion_sensor *pir)
{
	close(pir->chip_fd);
	free(pir);
}

void print_event_timestamp(const struct tm *tm)
{
	char now[64];
	strftime(now, sizeof(now), "%a, %b %d %Y %T %Z", tm);
	fprintf(stderr, "Motion Detected: %s\n", now);
}

int main()
{
	struct motion_sensor *pir;
	struct tm *tm;
	char outfile[64];

	pir = pir_init(17);
	if (!pir) {
		perror("pir_init");
		return 1;
	}

	while (1) {
		if (pir_wait_for_motion(pir) == -1) {
			perror("pir_wait_for_motion");
			break;
		}

		tm = localtime(&pir->timestamp);
		if (!tm) {
			perror("localtime");
			break;
		}

		print_event_timestamp(tm);

		strftime(outfile, sizeof(outfile), "%F-%T-%Z.h264", tm);

		if (libcamera_vid(outfile) == -1)
			break;

		fprintf(stderr, "Finished recording %s\n", outfile);
	}

	pir_release(pir);
	
	return 1;
}

