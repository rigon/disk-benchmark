#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BLOCK		4 * 1024 * 1024		// 4M
#define TIME		15 * 100000			// 15s
#define DATA_TYPE 	uint64

typedef unsigned long long int uint64;


unsigned int time_now() {
	struct timespec now;
	clock_gettime(CLOCK_REALTIME, &now);
	return now.tv_sec * 100000 + now.tv_nsec / 10000;
}

int main(int argc, char **argv)
{
	char *file_name;
	FILE *fp;
	int block_size = BLOCK / sizeof(DATA_TYPE);
	
	DATA_TYPE buffer[block_size];
	unsigned int count = 0;
	unsigned long long int bytes = 0;
	unsigned int bytes_last = 0;
	size_t result;
	unsigned int start, end, last_time = 0, read_time = 0;

	if(argc != 2) {
		perror("No filename provided\n");
		exit(EXIT_FAILURE);
	}
	file_name = argv[1];
	
	fp = fopen(file_name, "r"); // read mode
	if (fp == NULL) {
		perror("Error while opening the file.\n");
		exit(EXIT_FAILURE);
	}

	printf("Calibrating...\n");
	for(int i=0; i<5; i++)
		result = fread(buffer, sizeof(DATA_TYPE), block_size, fp);

	printf("Reading contents of %s file...\n", file_name);
	
	do {		
		start = time_now();
		result = fread(buffer, sizeof(DATA_TYPE), block_size, fp);
		end = time_now();
		
		//printf("Clocks: %d - Time: %d\n", end - start, CLOCKS_PER_SEC);
		
		unsigned int bytes_read = result > 0 ? block_size * sizeof(DATA_TYPE) : 0;
		
		read_time += end - start;
		last_time += end - start;
		bytes += bytes_read;
		bytes_last += bytes_read;
		count++;
		
		
/* 		printf("%.2fMB/s (%dMB in %.2f s)\n",
			bytes_read / ((float)(end - start) / (float)CLOCKS_PER_SEC) / 1024 / 1024,
			// bytes_read / 1024 / 1024,
			bytes_read / 1024 / 1024,
			(end - start) / (float)CLOCKS_PER_SEC); */
		
		if(last_time / 100000 > 0) {
			float time = last_time / (float)100000;
			
			printf("%.2fMB/s\n",
				bytes_last / time / 1024 / 1024);
				
			bytes_last = 0;
			last_time = 0;
		}

		if(result != block_size)
			fprintf(stderr, "No enough data!\n");
		
	} while(read_time < TIME && !feof(fp));

	fclose(fp);
	
	printf("Bytes read: %lluMB (%llu) in %.2fs, %d blocks\n",
		bytes / 1024 / 1024, bytes,
		read_time / (float)100000,
		count);
	
	printf("Read speed: %.2fMB/s\n",
		bytes / (read_time / (float)100000) / 1024 / 1024);
	
	return 0;
}

