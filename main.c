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

int raspivid(const char *outfile)
{
	char *cmd;
	int ret;

	ret = asprintf(&cmd, "raspivid -t 60000 -hf -vf -n -o %s", outfile);
	if (ret == -1) {
		fprintf(stderr, "failed to allocate raspivid command string\n");
		return ret;
	}
	
	ret = system(cmd);
	if (ret != 0) {
		if (ret == -1)
			perror("system");
		else if (WIFEXITED(ret))
			fprintf(stderr, "raspivid encountered an error: exit status: %d\n", WEXITSTATUS(ret));
		else if (WIFSIGNALED(ret)) {
			if (WTERMSIG(ret) == SIGINT)
				fprintf(stderr, "Keyboard interrupt recevied while recording %s\n", outfile);
			else
				fprintf(stderr, "raspivid terminated by signal: %d\n", WTERMSIG(ret));
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
	struct gpioevent_request req;
	struct gpioevent_data event;
	ssize_t rd;
	int ret;

	req.lineoffset = pir->pin;
	req.handleflags = GPIOHANDLE_REQUEST_INPUT;
	req.eventflags = GPIOEVENT_REQUEST_RISING_EDGE;
	strcpy(req.consumer_label, "motion_sensor");

	ret = ioctl(pir->chip_fd, GPIO_GET_LINEEVENT_IOCTL, &req);
	if (ret == -1)
		return ret;
	
	rd = read(req.fd, &event, sizeof(event));
	if (rd == -1)
		return -1;

	if (rd != sizeof(event)) {
		errno = EIO;
		return -1;
	}

	pir->timestamp = event.timestamp / 1000000000ULL;
	
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
	char now[32];
	strftime(now, sizeof(now), "%a, %b %d %Y %r %Z", tm);
	printf("Motion Detected: %s\n", now);
}

int main()
{
	struct motion_sensor *pir;
	struct tm *localtm;
	char outfile[64];
	int ret;

	pir = pir_init(17);
	if (!pir) {
		perror("pir_init");
		return -1;
	}
	
	while (1) {
		ret = pir_wait_for_motion(pir);
		if (ret == -1) {
			perror("wait_for_motion");
			ret = 1;
			break;
		}

		localtm = localtime(&pir->timestamp);
		if (!localtm) {
			perror("localtime");
			ret = 1;
			break;
		}

		print_event_timestamp(localtm);

		strftime(outfile, sizeof(outfile), "%F-%T.h264", localtm);

		if (raspivid(outfile) == -1) {
			ret = 1;
			break;
		}

		printf("Finished recording %s\n", outfile);
	}

	pir_release(pir);
	
	return ret;
}

