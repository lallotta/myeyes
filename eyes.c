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
#define TYPE ".h264"

void printTime(time_t *ti);

void makeFileName(char *f, time_t *ti, struct tm *i);

int main(void)
{
	wiringPiSetupGpio();

	pinMode(PIR, INPUT);

	time_t t;
	struct tm *info;
	char file[SIZE];
	int status;
	pid_t p;

	while (1)
	{
		if (digitalRead(PIR) == HIGH)
		{
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
				printTime(&t);
				makeFileName(file, &t, info);
				execlp("raspivid", "raspivid", "-hf", "-vf", "-n", "-o", file, NULL);
				exit(1);
			}
			else
			{
				if (wait(&status) < 0)
				{
					perror("wait");
					return 1;
				}
				if (status > 0) { return 1; }
				printf("finished recording\n");
			}
		}
	}
	return 0;
}

void printTime(time_t *ti)
{
	char *n = ctime(ti);
	printf("Motion Detected %s", n);
}

void makeFileName(char *f, time_t *ti, struct tm *i)
{
	i = localtime(ti);
	strftime(f, SIZE, "%a_%b_%d_%Y_%T", i);
	strcat(f, TYPE);
}

