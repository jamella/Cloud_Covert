#define _POSIX_C_SOURCE 199309
#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <fcntl.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>

#ifdef __i386
__inline__ uint64_t rdtsc(void) {
        uint64_t x;
        __asm__ volatile ("rdtsc" : "=A" (x));
        return x;
}
#elif __amd64
__inline__ uint64_t rdtsc(void) {
        uint64_t a, d;
        __asm__ volatile ("rdtsc" : "=a" (a), "=d" (d));
        return (d<<32) | a;
}
#endif

void cache_flush(uint8_t *address) {
        __asm__ volatile("clflush (%0)": :"r"(address) :"memory");
        return;
}

#define INTERVAL 100000
#define ACCESS_TIME 10000
#define BIT_NR 100

uint8_t *buf;
uint8_t *head;
uint64_t timing[BIT_NR];

void receiver() {
	cpu_set_t set;
	CPU_ZERO(&set);
	CPU_SET(1, &set);
	if (sched_setaffinity(syscall(SYS_gettid), sizeof(cpu_set_t), &set)) {
		fprintf(stderr, "Error set affinity\n")  ;
		return;
	}
	printf("Begin Receiving......\n");

	uint64_t tsc, tsc1;
	volatile uint8_t next;
	uint64_t i, access_nr;
	for (i=0; i<BIT_NR; i++) {
		tsc = rdtsc() + INTERVAL;
		access_nr = 0;
		tsc1 = rdtsc() + ACCESS_TIME;
		while (rdtsc() < tsc1) {
			next += head[0];
			cache_flush(&head[0]);
			access_nr ++;
		}
		timing[i] = ACCESS_TIME/access_nr;
		while(rdtsc() < tsc);
	}
}

int main (int argc, char *argv[]) {
        uint64_t buf_size = 1024*1024*1024;
        int fd = open("/mnt/hugepages/nebula2", O_CREAT|O_RDWR, 0755);
        if (fd<0) {
                printf("file open error!\n");
                return 0;
        }

        buf = mmap(0, buf_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
        if (buf == MAP_FAILED) {
                printf("map error!\n");
                unlink("/mnt/hugepages/nebula2");
                return 0;
        }

	int i;
	for (i=0; i<buf_size/sizeof(uint8_t); i++)
		buf[i] = 1;
	head = &buf[0];

	receiver();

	for (i=0; i<BIT_NR; i++)
		printf("%lu ", timing[i]);

	printf("\n");
	
	munmap(buf, buf_size);
}
