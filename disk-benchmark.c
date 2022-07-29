#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <limits.h>

#define DEFAULT_BLOCK_SIZE 4 * 1024 * 1024 // 4M
#define DEFAULT_TIME_LIMIT 15              // 15s
#define S_TO_NS            1000000000      // Seconds to nanoseconds ratio

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

struct timespec time_now() {
	struct timespec now;
	clock_gettime(CLOCK_REALTIME, &now);
	return now;
}

int parse_block_size(char *a) {
	int i;
	int unit = 1;
	
	for(i=0; a[i] != '\0'; i++) {
		if(!isdigit(a[i])) {
			switch(a[i]) {
				case 'K':
					unit = 1024;
					break;
				case 'M':
					unit = 1024 * 1024;
					break;
				default:
					fprintf(stderr, "Invalid format %s, only K and M allowed\n", a);
					exit(EXIT_FAILURE);
			}
			a[i] = '\0';
			break;
		}
	}
	
	if(i < 1) {
		fprintf(stderr, "Invalid format %s, only K and M allowed\n", a);
		exit(EXIT_FAILURE);
	}
	
	return atoi(a) * unit;
}

void fill_buffer(char *buffer, size_t size) {
	for(int i=0; i<size; i+=2)
		buffer[i]=(uint16) rand() % USHRT_MAX;
}

char *format_unit(char *str, double val) {
	char *unit = "B";
	if(val > 1024) {
		val /= 1024;
		unit = "KB";
	}
	if(val > 1024) {
		val /= 1024;
		unit = "MB";
	}
	if(val > 1024) {
		val /= 1024;
		unit = "GB";
	}
	if(val > 1024) {
		val /= 1024;
		unit = "TB";
	}
	sprintf(str, "%4.2f%s", val, unit);
	return str;
}

char *format_speed(char *str, uint64 bytes, time_t time) {
	double speed = (double)bytes / ((double)time / S_TO_NS);
	format_unit(str, speed);
	strcat(str, "/s");
	return str;
}

void print_partial(TEST *test, bool is_write) {
	char s[20];
	if(test->time_partial > S_TO_NS) {		// 1 second
		char *msg = (!is_write ?  "Read" : "Write");
		printf("%s %s\n", msg, format_speed(s, test->bytes_partial, test->time_partial));
		
		test->bytes_partial = 0;
		test->time_partial = 0;
	}
}

void print_total(TEST *test, size_t block_size, bool is_write) {
	char s1[20];
	char s2[20];
	char *type = (!is_write ?  "Read" : "Write");
	printf("Bytes %s: %s (%lluB) in %.2fs, %d blocks of %s\n",
		type,
		format_unit(s1, test->bytes_total),
		test->bytes_total,
		test->time_total / (double) S_TO_NS,
		test->count_op,
		format_unit(s2, block_size));
	printf("%s speed: %s\n", type,
		format_speed(s1, test->bytes_total, test->time_total));
}

ssize_t benchmark_operation(int fd, TEST *test, char *buffer, size_t block_size, bool is_write) {
	struct timespec start, end;
	ssize_t result;

	start = time_now();
	result = is_write ?	// Is read or write operations
		write(fd, buffer, block_size) :
		read(fd, buffer, block_size);
	end = time_now();
	
	if(result < 0) {
		perror("I/O operation error");
		return result;
	}

	time_t time_elapsed =		// Calculate time difference
		(end.tv_sec - start.tv_sec) * S_TO_NS +		// Convert to ns
		(end.tv_nsec - start.tv_nsec);
	
	// Update statistics
	test->time_partial += time_elapsed;
	test->time_total += time_elapsed;
	test->bytes_partial += result;
	test->bytes_total += result;
	test->count_op++;

	print_partial(test, is_write);

	return result;
}

void benchmark_file(const char *file_name, TEST_TYPE test_type, size_t block_size, time_t time_limit, bool is_random) {
	char *buffer = (char *)malloc(block_size * sizeof(char));
	char *buffer_cmp = (char *)malloc(block_size * sizeof(char));
	TEST t1 = {}, t2 = {}, t3 = {};
	time_t time_total;
	off_t file_size, result;
	char str[20];

	// Select flags for READ-ONLY or READ-WRITE
	int open_mode =
		test_type == READ_WRITE || test_type == READ_WRITE_READ ? O_RDWR :
		test_type == WRITE_READ || test_type == WRITE ? O_RDWR | O_CREAT :
		O_RDONLY;	// default
	
	// Open file
	int fd = open(file_name, open_mode | /* O_DIRECT | */ O_SYNC, S_IRUSR | S_IWUSR);
	if(fd < 0) {
		fprintf(stderr, "Cannot open %s: %s (errno %d)\n", file_name, strerror(errno), errno);
		exit(EXIT_FAILURE);
	}
	
	// Find file size
	file_size = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_SET);

	printf("## %s test with block size %s, file %s...\n",
		is_random ? "Random" : "Sequential",
		format_unit(str, block_size), file_name);
	
	do {
		size_t buffer_size = block_size;
		
		if(is_random)	// Seek into a random location of the file
			lseek(fd, ((double)rand()/RAND_MAX) * file_size - block_size, SEEK_SET);
		
		switch (test_type) {
			case READ_WRITE_READ:
			case READ_WRITE:
				// Read
				result = benchmark_operation(fd, &t1, buffer, block_size, false);
				if(result < 1)	// check for errors
					break;
				lseek(fd, -result, SEEK_CUR);	// Seek back to the initial position
				buffer_size = result;			// Write the same amount of data
			case WRITE_READ:
			case WRITE:
				// Generate data for WRITE_READ and WRITE modes
				if(test_type == WRITE_READ || test_type == WRITE)
					fill_buffer(buffer, buffer_size);
				// Write
				result = benchmark_operation(fd, &t2, buffer, buffer_size, true);
				if(result < 1)	// check for errors
					break;
				// Do not read back with READ_WRITE and WRITE modes
				if(test_type == READ_WRITE || test_type == WRITE)
					break;
					
				lseek(fd, -result, SEEK_CUR);	// Seek back to the initial position
				buffer_size = result;			// Write the same amount of data
			case READ:
				result = benchmark_operation(fd, &t3, buffer_cmp, buffer_size, false);
				// Compare between data written and data read back
				if(test_type != READ && strncmp(buffer, buffer_cmp, block_size) != 0) {
					fprintf(stderr, "Error: the data written differs from the data read back\n");
					result = -1;
				}
				break;
		}
		time_total = (t1.time_total + t2.time_total + t3.time_total) / S_TO_NS;
	} while(time_total < time_limit && result > 0);	// Terminate test

	// Close file
	close(fd);
	free(buffer);
	free(buffer_cmp);
	
	if(result == 0)
		fprintf(stderr, "End-of-file reached, time limit not hit!\n");
	else if(result < 0)
		fprintf(stderr, "Test ended due to errors (errno %d)!\n", errno);

	// Print results
	switch (test_type) {
		case READ_WRITE_READ:
		case READ_WRITE:
			puts("-- Results test Read before Write:");
			print_total(&t1, block_size, false);
		case WRITE_READ:
		case WRITE:
			puts("-- Results test Write:");
			print_total(&t2, block_size, true);
			if(test_type == READ_WRITE || test_type == WRITE)
				break;
		case READ:
			puts("-- Results Read:");
			print_total(&t3, block_size, false);
	}
}

void help() {
	puts("Usage of disk-benchmark:");
	puts("  -r                 Read test");
	puts("  -w                 Write test. The data written is randomly generated, this is a DESTRUCTIVE test");
	puts("  -rw                Read and write test. The data written is the data read");
	puts("  -wr                Write and read test. It verifies if the write operation completed correcly. The data written is randomly generated, this is a DESTRUCTIVE test");
	puts("  -rwr               Read, write and read test. It verifies if the write operation completed correcly. The data written is the data read");
	puts("  -h, --help         Shows this help");
	puts("  -s, --sequential   Performs a sequential test");
	puts("  -u, --random       Performs a random test, a 4K block size will be used");
	puts("  -b, --block-size   Sets the block size for sequential test (K and M multiplier allowed)");
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
			if(i >= argc)
				fprintf(stderr, "Block size not provided\n"), exit(EXIT_FAILURE);
			block_size = parse_block_size(argv[i]);
		}
		else if(strcmp("-t", a) == 0 || strcmp("--time-limit", a) == 0) {
			i++;
			if(i >= argc)
				fprintf(stderr, "Time limit not provided\n"), exit(EXIT_FAILURE);
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
		fputs("No filename provided\n", stderr);
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
