#include <fcntl.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dma_xfer_utils.c"

#define MAX_TRANSFER_SIZE (1024 * 1024 * 64)
#define DEVICE_NAME_DEFAULT "/dev/qdma01000-MM-0"

static struct option const long_opts[] = {
    {"device", required_argument, NULL, 'd'},
    {"address", required_argument, NULL, 'a'},
    {"data infile", required_argument, NULL, 'f'},
    {"help", no_argument, NULL, 'h'},
    {0, 0, 0, 0}};

static int read_file_to_buffer(int fd, char *buffer, uint64_t size) {
  uint64_t total_read = 0;
  while (total_read < size) {
    ssize_t rc = read(fd, buffer + total_read, size - total_read);
    if (rc < 0) {
      perror("read");
      return -1;
    }
    total_read += rc;
  }
  return total_read;
}

static int do_transfer(char *dev, uint64_t addr, const char *filename) {
  int rc = 0;
  int i, left;
  char *buffer = NULL;
  uint64_t input_size = 0;
  uint64_t page_size = sysconf(_SC_PAGESIZE);
  int fd = open(dev, O_RDWR);
  if (fd < 0) {
    perror("open device");
    return -1;
  }
  int ifile = open(filename, O_RDONLY);
  if (ifile < 0) {
    perror("open input file");
    return -1;
  }
  input_size = lseek(ifile, 0, SEEK_END);
  lseek(ifile, 0, SEEK_SET);
  posix_memalign((void **)&buffer, page_size, MAX_TRANSFER_SIZE + page_size);
  if (!buffer) {
    rc = -ENOMEM;
    goto err;
  }
  int tranfers = (input_size + page_size - 1) / page_size; 
  for (i = 0, left = input_size; i < tranfers; ++i) {
    uint64_t rd = read_file_to_buffer(ifile, buffer, left > MAX_TRANSFER_SIZE ? MAX_TRANSFER_SIZE : left);
    if (rd < 0)
      goto err;
    left -= rd;
    rc = write_from_buffer(dev, fd, buffer, rd, addr + i * MAX_TRANSFER_SIZE);
    if (rc < 0)
      goto err;
  }
  rc = 0;
err:
  close(ifile);
  close(fd);
  if (buffer)
    free(buffer);
  return rc;
}

static int test_dma(char *devname, uint64_t addr, uint64_t size,
                    uint64_t offset, uint64_t count, char *infname, char *);

static void usage(const char *name) {
  int i = 0;

  fprintf(stdout, "%s\n\n", name);
  fprintf(stdout, "usage: %s [OPTIONS]\n\n", name);
  fprintf(stdout, "Write a file to the fpga.\n\n");

  fprintf(stdout, "  -%c (--%s) device (defaults to %s)\n", long_opts[i].val,
          long_opts[i].name, DEVICE_NAME_DEFAULT);
  i++;
  fprintf(stdout, "  -%c (--%s) the start address on the AXI bus\n",
          long_opts[i].val, long_opts[i].name);
  i++;
  fprintf(stdout, "  -%c (--%s) filename to read the data from.\n",
          long_opts[i].val, long_opts[i].name);
  i++;
  fprintf(stdout, "  -%c (--%s) print usage help and exit\n", long_opts[i].val,
          long_opts[i].name);
}

int main(int argc, char *argv[]) {
  int cmd_opt;
  char *device = DEVICE_NAME_DEFAULT;
  uint64_t address = 0;
  char *infname = NULL;

  while ((cmd_opt = getopt_long(argc, argv, "vhc:f:d:a:s:o:w:", long_opts,
                                NULL)) != -1) {
    switch (cmd_opt) {
    case 0:
      /* long option */
      break;
    case 'd':
      /* device node name */
      // fprintf(stdout, "'%s'\n", optarg);
      device = strdup(optarg);
      break;
    case 'a':
      /* RAM address on the AXI bus in bytes */
      address = getopt_integer(optarg);
      break;
    case 'f':
      infname = strdup(optarg);
      break;
    case 'v':
      verbose = 1;
      break;
    case 'h':
    default:
      usage(argv[0]);
      exit(0);
      break;
    }
  }
  return do_transfer(device, address, infname);
}
