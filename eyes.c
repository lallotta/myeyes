#include <stdio.h>
#include <wiringPi.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <sys/wait.h>

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
	int p, w;

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
				execlp("raspivid", "-t", "60", "-hf", "-vf", "-n", "-o", file, (char *) NULL); 
			}
			else
			{
				w = wait(NULL);
				printf("finished recording\n");
			}
		}
	}
	return 0;
}
