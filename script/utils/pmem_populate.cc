// pmem_populate.cpp
#include <fcntl.h>
#include <immintrin.h>
#include <linux/fs.h>
#include <linux/magic.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include <libpmem.h>

#ifndef ST_DAX
#define ST_DAX (1ULL << 4)  // Value from linux/magic.h
#endif

void populate_single_thread(char* pmem, size_t size) {
  printf("Populating PMem space using single thread (read-only)...\n");
  const size_t page_size = sysconf(_SC_PAGESIZE);
  volatile char temp;  // volatile to prevent optimization

  // Read one byte per page to force page population
  for (size_t offset = 0; offset < size; offset += page_size) {
    temp = pmem[offset];  // Just read, don't write
  }

  printf("Population complete\n");
}

void populate_multi_thread(char* pmem, size_t size, int num_threads) {
  printf("Populating PMem space using %d threads (read-only)...\n", num_threads);
  const size_t page_size = sysconf(_SC_PAGESIZE);
  std::vector<std::thread> threads;
  size_t chunk_size = size / num_threads;

  for (int i = 0; i < num_threads; i++) {
    threads.emplace_back([=]() {
      char* start = pmem + (i * chunk_size);
      size_t len = (i == num_threads - 1) ? size - (i * chunk_size) : chunk_size;
      volatile char temp;  // volatile to prevent optimization

      // Read one byte per page to force page population
      for (size_t offset = 0; offset < len; offset += page_size) {
        temp = start[offset];  // Just read, don't write
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  printf("Population complete\n");
}

bool isPagePopulated(void* addr) {
  const size_t page_size = sysconf(_SC_PAGESIZE);
  uint64_t page_frame_number = 0;
  int fd = open("/proc/self/pagemap", O_RDONLY);

  if (fd < 0) return false;

  // Calculate the index in pagemap
  uint64_t offset =
      (reinterpret_cast<uint64_t>(addr) / page_size) * sizeof(uint64_t);
  if (pread(fd, &page_frame_number, sizeof(uint64_t), offset) !=
      sizeof(uint64_t)) {
    close(fd);
    return false;
  }
  close(fd);

  // Bit 63 indicates if page is present
  return (page_frame_number & (1ULL << 63)) != 0;
}

bool verify_population(char* pmem, size_t size) {
  printf("Population Check ...\n");
  const size_t page_size = sysconf(_SC_PAGESIZE);
  size_t count = 0;
  for (size_t i = 0; i < size / page_size; i++) {
    if (!isPagePopulated(pmem + i * page_size)) {
      printf("Population Check interrupted! %lu pages are not populated\n",
             count);
      return false;
    }
    count++;
  }
  printf("Population Check done, %lu pages are populated\n", count);
  return true;
}

bool verify_population_fast(char* pmem, size_t size) {
  printf("Population Check with mincore ...\n");
  const size_t page_size = sysconf(_SC_PAGESIZE);
  size_t num_pages = (size + page_size - 1) / page_size;
  size_t resident_pages = 0;
  std::vector<unsigned char> vec(num_pages);

  if (mincore(pmem, size, vec.data()) != 0) {
    return false;
  }

  for (size_t i = 0; i < num_pages; i++) {
    if (vec[i] & 1) resident_pages++;
  }
  printf("Fast Population Check done. Pages in memory: %zu out of %zu\n",
         resident_pages, num_pages);
  return resident_pages == num_pages;
}

void print_usage(const char* program) {
  printf("Usage: %s <filepath> [-t threads]\n", program);
  printf("  filepath: Path to the existing file to populate\n");
  printf("  -t threads: Optional - number of threads to use (default: 1)\n");
}

// Get file size in bytes, returns 0 if file doesn't exist
size_t get_file_size(const char* filepath) {
  struct stat st;
  if (stat(filepath, &st) == 0) {
    return st.st_size;
  }
  return 0;
}

// Add this function to check if the filesystem supports DAX
bool is_dax_filesystem(const char* path) {
  struct statfs fs_stats;
  if (statfs(path, &fs_stats) != 0) {
    printf("Warning: Could not check filesystem type: %s\n", strerror(errno));
    return false;
  }

  printf("Debug: filesystem flags: 0x%lx, ST_DAX: 0x%llx\n", 
         fs_stats.f_flags, ST_DAX);
  
  return (fs_stats.f_flags & ST_DAX) != 0;
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    print_usage(argv[0]);
    return 1;
  }

  const char* filepath = argv[1];
  size_t size = get_file_size(filepath);
  int num_threads = 1;

  // Parse optional thread count
  for (int i = 2; i < argc; i++) {
    if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
      num_threads = std::stoi(argv[i + 1]);
      if (num_threads < 1) num_threads = 1;
      i++;
    }
  }

  if (size == 0) {
    printf("Error: File does not exist or is empty: %s\n", filepath);
    return 1;
  }

  printf("File size: %zu bytes (%.2f GB)\n", size, size / (1024.0 * 1024 * 1024));

  // Check if filesystem supports DAX
  if (!is_dax_filesystem(filepath)) {
    printf("Warning: Filesystem does not support DAX\n");
  } else {
    printf("Detected DAX-enabled filesystem\n");
  }

  // Open existing file - removed O_CREAT
  int fd = open(filepath, O_RDWR);
  if (fd < 0) {
    printf("Error opening file: %s\n", strerror(errno));
    return 1;
  }

  size_t mapped_len;
  int is_pmem;
  char* pmem = (char*)pmem_map_file(filepath, size, 
                                   PMEM_FILE_CREATE, 0666,
                                   &mapped_len, &is_pmem);
  if (pmem == NULL) {
    printf("Error mapping file with pmem_map_file: %s\n", strerror(errno));
    return 1;
  }

  if (!is_pmem) {
    printf("Warning: File is not on a PMem device\n");
  }

  if (mapped_len != size) {
    printf("Warning: Mapped size (%zu) differs from file size (%zu)\n", 
           mapped_len, size);
  }

  // Populate the space
  if (num_threads == 1) {
    populate_single_thread(pmem, size);
  } else {
    populate_multi_thread(pmem, size, num_threads);
  }

  // Verify population
  if (!verify_population_fast(pmem, size)) {
    printf("Warning: File may not be fully populated!\n");
  }

  // Cleanup
  pmem_unmap(pmem, mapped_len);
  return 0;
}