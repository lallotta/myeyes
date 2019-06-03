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

void print_event_timestamp(const struct tm *tm)
{
	char now[32];
	strftime(now, sizeof(now), "%a %F %T", tm);
	printf("Motion Detected: %s\n", now);
}

int main()
{
	struct gpioevent_request req;
	time_t t;
	struct tm *localtm;
	char outfile[64];
	ssize_t rd;
	int ret, fd;

	fd = open("/dev/gpiochip0", 0);
	if (fd == -1) {
		perror("error opening GPIO character device file");
		return 1;
	}

	req.lineoffset = 17;
	req.handleflags = GPIOHANDLE_REQUEST_INPUT;
	req.eventflags = GPIOEVENT_REQUEST_RISING_EDGE;
	strcpy(req.consumer_label, "motion_sensor");

	while (1) {
		struct gpioevent_data event;

		ret = ioctl(fd, GPIO_GET_LINEEVENT_IOCTL, &req);
		if (ret == -1) {
			perror("ioctl: error getting line event");
			ret = 1;
			break;
		}
		
		rd = read(req.fd, &event, sizeof(event));
		if (rd == -1) {
			perror("error reading event");
			ret = 1;
			break;
		}

		if (rd != sizeof(event)) {
			fprintf(stderr, "failed to read event\n");
			ret = 1;
			break;
		}

		t = event.timestamp / 1000000000ULL;

		localtm = localtime(&t);
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

		if (close(req.fd) == -1) {
			perror("error closing line event fd");
			ret = 1;
			break;
		}
	}

	if (close(fd) == -1)
		perror("error closing GPIO character device file");
	
	return ret;
}

