#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <limits.h>

#define BLOCK_SIZE	4 * 1024 * 1024		// 4M
#define N_BLOCKS	64					// 256MB (64 * 4)
#define TIME		15 * 100000			// 15s
#define DATA_TYPE	uint64

typedef unsigned int uint;
typedef unsigned long long int uint64;
typedef unsigned short int uint16;

typedef enum {
	READ,
	READ_WRITE,
	WRITE_READ,
	READ_WRITE_READ
} TEST_TYPE;

typedef struct {
	uint count_op;
	time_t time_partial;
	time_t time_total;
	uint64 bytes_partial;
	uint64 bytes_total;
} TEST;

time_t time_now() {
	struct timespec now;
	clock_gettime(CLOCK_REALTIME, &now);
	// convert to milliseconds
	return now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

void fill_buffer(char *buffer, size_t size) {
	for(int i=0; i<size; i+=2)
		buffer[i]=(uint16) rand() % USHRT_MAX;
}

char *format_speed(char *s, uint64 bytes, time_t time) {
	float speed = ((float)bytes / 1024) / ((float)time / 1000);	// KB/s
	char *unit = "KB";

	if(speed > 1024) {
		speed /= 1024;
		unit = "MB";
	}
	if(speed > 1024) {
		speed /= 1024;
		unit = "GB";
	}
	if(speed > 1024) {
		speed /= 1024;
		unit = "TB";
	}

	sprintf(s, "%4.2f%s/s", speed, unit);
	return s;
}

void print_partial(TEST *test, bool is_write) {
	char str[20];
	if(test->time_partial > 1000) {		// 1 second
		char *msg = (!is_write ?  "Read" : "Write");
		printf("%s %s\n", msg, format_speed(str, test->bytes_partial, test->time_partial));
		
		test->bytes_partial = 0;
		test->time_partial = 0;
	}
}

void print_total(TEST *test, bool is_write) {
	char str[20];
	char *msg = (!is_write ?  "Read" : "Write");
	printf("Bytes %s: %lluMB (%llu) in %.2fs, %d blocks\n", msg,
		test->bytes_total / 1024 / 1024,
		test->bytes_total,
		test->time_total / (float) 1000,
		test->count_op);
	printf("%s speed: %s\n", msg,
		format_speed(str, test->bytes_total, test->time_total));
}

int benchmark_operation(int fd, TEST *test, char *buffer, bool write_enabled) {
	time_t start, end;
	ssize_t result;

	start = time_now();
	result = write_enabled ?	// Is read or write operations
		write(fd, buffer, BLOCK_SIZE) :
		read(fd, buffer, BLOCK_SIZE);
	end = time_now();
	
	if(result < 1) {
		print_partial(test, write_enabled);
		fprintf(stderr, "I/O operation error with errno %d\n", errno);
		return 1;
	}

	time_t time_elapsed = end - start;
	test->time_partial += time_elapsed;
	test->time_total += time_elapsed;
	test->bytes_partial += result;
	test->bytes_total += result;
	test->count_op++;

	// if (ferror(f)) {
	// 	fprintf(stderr, "Error accessing the file!\n");
	// 	return 1;
	// }
	// if (feof(f)) {
	// 	fprintf(stderr, "EOF reached!\n");
	// 	return 1;
	// }
	// if(result != block_size) {
	// 	fprintf(stderr, "No enough data!\n");
	// 	return 1;
	// }

	print_partial(test, write_enabled);

	return 0;
}

void benchmark_file(char *filename, TEST_TYPE test_type) {
	char buffer[BLOCK_SIZE];
	TEST t1, t2, t3;
	time_t time_total;

	// Select flag for READ-ONLY or READ-WRITE
	int open_mode = (
		test_type == READ_WRITE ||
		test_type == WRITE_READ ||
		test_type == READ_WRITE_READ) ?
		O_RDWR : O_RDONLY;

	// Open file
	int fd = open(filename, open_mode | /* O_DIRECT | */ O_SYNC | O_CREAT, S_IRUSR | S_IWUSR);
	if(fd < 0) {
		fprintf(stderr, "Cannot open %s, errno %d\n", filename, errno);
		exit(EXIT_FAILURE);
	}

	int err = 0;
	printf("Reading contents of %s file...\n", filename);
	do {
		switch (test_type) {
			case READ_WRITE_READ:
			case READ_WRITE:
				// Read
				benchmark_operation(fd, &t1, buffer, false);
			case WRITE_READ:
				// Generate data for WRITE_READ mode
				if(test_type == WRITE_READ)
					fill_buffer(buffer, BLOCK_SIZE);
				// Write
				benchmark_operation(fd, &t2, buffer, true);
				// Do not read back with READ_WRITE
				if(test_type == READ_WRITE)
					break;
			case READ:
				err = benchmark_operation(fd, &t3, buffer, false);
				break;
			default:
				perror("Invalid operation!\n");
				exit(EXIT_FAILURE);
		}
		time_total = (t1.time_total + t2.time_total + t3.time_total) / 1000;
	} while(time_total < 15 && err == 0);	// 15s

	close(fd);

	print_total(&t1, false);
	print_total(&t2, true);
	print_total(&t3, false);
}

	// do {
	// } while(read_time < TIME && !feof(fp));


int main(int argc, char **argv) {
	
	int i;

	for(i=0; i<argc; i++) {


	}

	
	if(argc != 2) {
		perror("No filename provided\n");
		exit(EXIT_FAILURE);
	}
	char *file_name = argv[1];

	srand(time(NULL));

	benchmark_file(file_name, READ);
}
