#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <limits.h>

#define DEFAULT_BLOCK_SIZE	4 * 1024 * 1024		// 4M
#define DEFAULT_TIME_LIMIT	15					// 15s

typedef unsigned int uint;
typedef unsigned long long int uint64;
typedef unsigned short int uint16;

typedef enum {
	READ,
	WRITE,
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

int parse_block_size(const char *a) {
	return DEFAULT_BLOCK_SIZE;
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
	char *type = (!is_write ?  "Read" : "Write");
	printf("Bytes %s: %lluMB (%llu) in %.2fs, %d blocks\n", type,
		test->bytes_total / 1024 / 1024,
		test->bytes_total,
		test->time_total / (float) 1000,
		test->count_op);
	printf("%s speed: %s\n", type,
		format_speed(str, test->bytes_total, test->time_total));
}

ssize_t benchmark_operation(int fd, TEST *test, char *buffer, size_t block_size, bool is_write) {
	time_t start, end;
	ssize_t result;

	start = time_now();
	result = is_write ?	// Is read or write operations
		write(fd, buffer, block_size) :
		read(fd, buffer, block_size);
	end = time_now();
	
	if(result < 1) {
		print_partial(test, is_write);
		fprintf(stderr, "I/O operation error with errno %d\n", errno);
		return -1;
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

	print_partial(test, is_write);

	return result;
}

void benchmark_file(const char *file_name, TEST_TYPE test_type, size_t block_size, time_t time_limit, bool is_random) {
	char buffer[block_size];
	TEST t1, t2, t3;
	time_t time_total;
	off_t result;

	// Select flag for READ-ONLY or READ-WRITE
	int open_mode = (test_type != READ ? O_RDONLY : O_RDWR);

	// Open file
	int fd = open(file_name, open_mode | /* O_DIRECT | */ O_SYNC | O_CREAT, S_IRUSR | S_IWUSR);
	if(fd < 0) {
		fprintf(stderr, "Cannot open %s, errno %d\n", file_name, errno);
		exit(EXIT_FAILURE);
	}

	int err = 0;
	printf("Running benchmark on file %s...\n", file_name);
	do {
		printf("%d\n", test_type);
		switch (test_type) {
			case READ_WRITE_READ:
			case READ_WRITE:
				printf("READ_WRITE_READ\n");
				// Read
				result = benchmark_operation(fd, &t1, buffer, block_size, false);
				lseek(fd, result, SEEK_CUR);
			case WRITE_READ:
			case WRITE:
				printf("WRITE_READ\n");
				// Generate data for WRITE_READ and WRITE modes
				if(test_type == WRITE_READ || test_type == WRITE)
					fill_buffer(buffer, block_size);
				// Write
				result = benchmark_operation(fd, &t2, buffer, block_size, true);
				printf("%ld\n", result);
				// Do not read back with READ_WRITE and WRITE modes
				if(test_type == READ_WRITE || test_type == WRITE)
					break;
					
				lseek(fd, result, SEEK_CUR);
			case READ:
				printf("READ\n");
				err = benchmark_operation(fd, &t3, buffer, block_size, false);
				break;
			default:
				perror("Invalid operation!\n");
				exit(EXIT_FAILURE);
		}
		time_total = (t1.time_total + t2.time_total + t3.time_total) / 1000;
	} while(time_total < time_limit && err == 0);	// Terminate test

	close(fd);

	// Print results
	switch (test_type) {
		case READ_WRITE_READ:
		case READ_WRITE:
			print_total(&t1, false);
		case WRITE_READ:
		case WRITE:
			print_total(&t2, true);
			if(test_type == READ_WRITE || test_type == WRITE)
				break;
		case READ:
			print_total(&t3, false);
	}
}

void help() {
// Usage of server/photo-gallery:
//   -b, --cache-thumbnails         Generate thumbnails in background when the application starts
//   -c, --collection stringArray   Specify a new collection. Example name=Default,path=/photos,thumbs=/thumbs
//       --disable-webdav           Disable WebDAV
	puts("Usage of disk-benchmark:");
	puts("  -r                 Read test");
	puts("  -w                 Write test. The data written is randomly generated, this is a DESTRUCTIVE test");
	puts("  -rw                Read and write test. The data written is the data read");
	puts("  -wr                Write and read test. It verifies if the write operation completed correcly. The data written is randomly generated, this is a DESTRUCTIVE test");
	puts("  -rwr               Read, write and read test. It verifies if the write operation completed correcly. The data written is the data read");
	puts("  -h, --help         Shows this help");
	puts("  -s, --sequential   Performs a sequential test");
	puts("  -u, --random       Performs a random test, a 4K block size will be used");
	puts("  -b, --block-size   Sets the block size for sequential test");
	puts("  -t, --time-limit   Duration of each test");
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
	TEST_TYPE test_type = READ;
	char *file_name = NULL;
	bool sequential_test = false;
	bool random_test = false;
	size_t block_size = DEFAULT_BLOCK_SIZE;
	time_t time_limit = DEFAULT_TIME_LIMIT;
	int i;
	// initialize random seed
	srand(time(NULL));

	for(i=1; i<argc; i++) {
		char *a = argv[i];
		// Mode selection
		if(strcmp("-r", a) == 0)
			test_type = READ;
		else if(strcmp("-w", a) == 0)
			test_type = WRITE;
		else if(strcmp("-rw", a) == 0)
			test_type = READ_WRITE;
		else if(strcmp("-wr", a) == 0)
			test_type = WRITE_READ;
		else if(strcmp("-rwr", a) == 0)
			test_type = READ_WRITE_READ;
		// Sequencial or random test
		else if(strcmp("-s", a) == 0 || strcmp("--sequential", a) == 0)
			sequential_test = true;
		else if(strcmp("-u", a) == 0 || strcmp("--random", a) == 0)
			random_test = true;
		// Block size and time limit
		else if(strcmp("-b", a) == 0 || strcmp("--block-size", a) == 0) {
			i++;
			block_size = parse_block_size(argv[i]);
		}
		else if(strcmp("-t", a) == 0 || strcmp("--time-limit", a) == 0) {
			i++;
			time_limit = atoi(argv[i]);
		}
		else if(strcmp("-h", a) == 0 || strcmp("--help", a) == 0)
			help();
		else {
			if(strncmp("-", a, 1)==0) {	// Option not recognized
				fprintf(stderr, "Option %s not recognized\n", a);
				help();
			}
			file_name = a;
		}
	}

	if(file_name == NULL) {
		perror("No filename provided\n");
		help();
	}
	
	// Data loss checks
	if(test_type != READ) {
		char ans[20];
		puts("The selected test will perform writing operation on the file.");
		puts("Data LOSS can occur. Are you sure you want to continue? (yes/no)");
		fgets(ans, 20, stdin);
		if(strcmp("yes\n", ans) != 0)
			exit(EXIT_FAILURE);
	}
	if(test_type == WRITE || test_type == WRITE_READ) {
		char ans[20];
		puts("Random data will be written over your existing data.");
		puts("This action is NOT RECOVERABLE. Are you really sure you want to continue? (yes/no)");
		fgets(ans, 20, stdin);
		if(strcmp("yes\n", ans) != 0)
			exit(EXIT_FAILURE);
	}
	
	if(sequential_test || sequential_test == false && random_test == false)		// Default option
		benchmark_file(file_name, test_type, block_size, time_limit, false);
	if(random_test)
		benchmark_file(file_name, test_type, 4 * 1024, time_limit, true);		// 4K (i.e. 4k random)
	
	exit(EXIT_SUCCESS);
}
