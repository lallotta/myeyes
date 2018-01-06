#include <stdio.h>
#include <stdlib.h>
#include <wiringPi.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <sys/wait.h>
#include <errno.h>

#define PIR 4 
#define SIZE 80

void makeFileName(char *f, time_t *ti);

void waitForMotion();

int main(void)
{
	wiringPiSetupGpio();

	pinMode(PIR, INPUT);

	time_t t;
	char out_file[SIZE];
	int status;
	pid_t p;

	while (1)
	{
		waitForMotion();

		p = fork();
		if (p < 0)
		{
			printf("error creating child process\n");
			perror("fork");
			return 1;
		}
		else if (p == 0)
		{
			t = time(NULL);
			printf("Motion Detected: %s", ctime(&t));
			makeFileName(out_file, &t);
			execlp("/usr/bin/raspivid", "raspivid", "-t", "60000", "-hf", "-vf", "-n", "-o", out_file, NULL);
			exit(1);
		}
		else
		{
			if (wait(&status) < 0)
			{
				perror("wait");
				return 1;
			}

			if (status > 0)
				return 1;

			printf("finished recording\n");
		}
	}
	return 0;
}

void makeFileName(char *f, time_t *ti)
{
	strftime(f, SIZE, "%a_%b_%d_%Y_%T", localtime(ti));
	strcat(f, ".h264");
}

void waitForMotion()
{
	while (1)
	{
		if (digitalRead(PIR) == HIGH)
			return;
	}
}
