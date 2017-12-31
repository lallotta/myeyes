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

int main(void)
{
	wiringPiSetupGpio();

	pinMode(PIR, INPUT);

	time_t t;
	char *now;
	struct tm *info;
	char file[SIZE];
	int p, status;

	while (1)
	{
		if (digitalRead(PIR) == HIGH)
		{
			t = time(NULL);
			now = ctime(&t);
			
			printf("Motion Detected %s", now);

			p = fork();
			if (p == -1)
			{
				printf("error creating child process\n");
				return 1;
			}
			else if (p == 0)
			{
				info = localtime(&t);
				strftime(file, SIZE, "%a_%b_%d_%Y_%T", info);
				strcat(file, TYPE);
				execlp("raspivid", "raspivid", "-t", "60000", "-hf", "-vf", "-n", "-o", file, NULL);
				perror("camera error");
				exit(1);
			}
			else
			{
				if (wait(&status) == -1)
				{
					perror("wait error");
					return 1;
				}
				if (status > 0)
				{
					perror("child error");
					return 1;
				}
				printf("finished recording\n");
			}
		}
	}
	return 0;
}
