# Disk Benchmark

Tool to benchmark disks, devices, filesystems or even files, like CrystalDiskMark but for the command line.

## Disclaimer

Use this tool with caution. This tool is NOT yet extensively tested and data loss can occur. Use it at your own risk.

## Features

Multiple test types can be performed:
- **Read** test
- **Write** test. The data written is randomly generated, this is a DESTRUCTIVE test
- **Read and write** test. The data written is the data read 
- **Write and read** test. It verifies if the write operation completed correcly. The data written is randomly generated, this is a DESTRUCTIVE test
- **Read, write and read** test. It verifies if the data was written correcly. The data written is the data read, this should not be destructive

Tests can be done using different stratagies:
- Sequentially
- Randomly

## Build

This tool uses as few dependencies as possible in order to make the compilation process as simple as possible. You just need `gcc` installed:

    gcc -o disk-benchmark disk-benchmark.c

or simply:

    make
    
### ArchLinux package

ArchLinux users can install directly from AUR:
[https://aur.archlinux.org/packages/disk-benchmark](https://aur.archlinux.org/packages/disk-benchmark)

## Command line options

    Usage of disk-benchmark:
      -r                 Read test
      -w                 Write test. The data written is randomly generated, this is a DESTRUCTIVE test
      -rw                Read and write test. The data written is the data read
      -wr                Write and read test. It verifies if the write operation completed correcly.
                         The data written is randomly generated, this is a DESTRUCTIVE test
      -rwr               Read, write and read test. It verifies if the data was written correcly.
                         The data written is the data read, this should not be destructive
      -h, --help         Shows this help
      -s, --sequential   Performs a sequential test
      -u, --random       Performs a random test, a 4K block size will be used
      -b, --block-size   Sets the block size for sequential test (K and M multiplier allowed)
      -t, --time-limit   Duration of each test


## Contribute

Contributions to this project are very welcome, by reporting issues or just by sharing your support. That means the world to me!

Please help me maintaining this tool, only with your support I can take the time to make it even better. Look here for more info [https://www.rigon.tk/#contribute](https://www.rigon.tk/#contribute)