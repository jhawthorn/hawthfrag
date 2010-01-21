/*
 * WARNING WARNING WARNING WARNING WARNING WARNING 
 * This program is only for crazy people.
 * It will rip your data into tiny little pieces.
 */
#include <stdlib.h>
#include <string.h>

#include <ftw.h>
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/fs.h>

#define MIN(a,b) ((a)<(b)?(a):(b))

int total_files = 0;
int examined_files = 0;

struct filecost{
	int cost;
	char *filename;
};

struct filecost *costs;

int fccmp(const void *b, const void *a){
	return (((struct filecost *)a)->cost - ((struct filecost *)b)->cost);
}

int filefrag(const char *filename){
	int fd;
	struct stat statinfo;
	int block;
	int num_blocks;
	int block_size;
	int i;

	if((fd = open(filename, O_RDONLY)) < 0) {
		fprintf(stderr, "Cannot open %s\n", filename);
		return 0;
	}

	if(ioctl(fd, FIGETBSZ, &block_size) < 0) {
		fprintf(stderr, "Cannot get block size\n");
		close(fd);
		return 0;
	}

	if(fstat(fd, &statinfo) < 0) {
		fprintf(stderr, "Cannot stat %s\n", filename);
		close(fd);
		return 0;
	}

	num_blocks = (statinfo.st_size + block_size - 1) / block_size;

	int cost = 0;
	int lastblock = 0;
	for(i = 0; i < num_blocks; i++) {
		block = i;
		if(ioctl(fd, FIBMAP, &block)) {
			printf("ioctl failed: %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
		if(block != lastblock + 1){
			cost++;
		}
		lastblock = block;
	}
	close(fd);
	return cost;
}

int count(const char *name, const struct stat *status, int type){
	if(type == FTW_F){
		total_files++;
	}
	return 0;
}

void printstatus(){
	printf("\e[s%5i/%i\e[u", examined_files, total_files);
	fflush(stdout);
}

int walk(const char *name, const struct stat *status, int type){
	if(type == FTW_F){
		costs[examined_files].cost = filefrag(name);
		costs[examined_files].filename = strdup(name);
		examined_files++;
		printstatus();
	}
	return 0;
}

char buffer[4096 * 1024];

void defrag(const char *filename){
	int rfd, wfd;
	struct stat stats;
	{
		rfd = open(filename, O_RDONLY);
		fstat(rfd, &stats);
		off_t length = stats.st_size;
		wfd = open("tmpfile", O_CREAT | O_WRONLY);
		ftruncate(wfd, length);
	}

	for(;;){
		int length;
		if((length = read(rfd, buffer, sizeof buffer)) <= 0){
			break;
		}
		if(write(wfd, buffer, length) <= 0){
			perror("write");
			exit(EXIT_FAILURE);
		}
	}

	fsync(wfd);

	fchown(wfd, stats.st_uid, stats.st_gid);
	fchmod(wfd, stats.st_mode);

	close(wfd);
	close(rfd);

	rename("tmpfile", filename);
}

int main(){
	int i;
	printf("counting files\n");
	ftw(".", count, 5);
	costs = malloc(total_files * sizeof(struct filecost));
	printf("Finding fragmented files (of %i files)\n", total_files);
	ftw(".", walk, 5);
	qsort(costs, total_files, sizeof(struct filecost), fccmp);
	printf("Defragmenting...\n");
	for(i = 0; i < MIN(100, total_files); i++){
		if(costs[i].cost < 10){
			break;
		}
		printf("Defragmenting '%s', which has %i fragments\n", costs[i].filename, costs[i].cost);
		defrag(costs[i].filename);
		printf("\tnow %i fragments\n", filefrag(costs[i].filename));
	}
	return 0;
}


