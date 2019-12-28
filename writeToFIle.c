#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>

char* itoa(int val, int base){
	
	static char buf[32] = {0};
	
	int i = 30;
	
	for(; val && i ; --i, val /= base)
	
		buf[i] = "0123456789abcdef"[val % base];
	
	return &buf[i+1];
}

int writeToFile(char *filepath, int in){
    char *text=malloc(17*sizeof(char));
    text=itoa(in,10);
    int fd = open(filepath, O_RDWR | O_CREAT | O_TRUNC, (mode_t)0600);
    //printf("write to %s value %d\n",filepath, in);
    if (fd == -1){
        perror("Error opening file for writing");
        exit(EXIT_FAILURE);
    }
    size_t textsize = strlen(text) + 1; 
    
    if (lseek(fd, textsize, SEEK_SET) == -1){
        close(fd);
        perror("Error calling lseek() to 'stretch' the file");
        exit(EXIT_FAILURE);
    }
    
    if (write(fd, "", 1) == -1){
        close(fd);
        perror("Error writing last byte of the file");
        exit(EXIT_FAILURE);
    }
    
    char *map = mmap(0, textsize, PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED){
        close(fd);
        perror("Error mmapping the file");
        exit(EXIT_FAILURE);
    }
    
    for (size_t i = 0; i < textsize-1; i++){
        map[i] = text[i];
    }

    if (msync(map, textsize, MS_SYNC) == -1){
        perror("Could not sync the file to disk");
    }
    
    if (munmap(map, textsize) == -1){
        close(fd);
        perror("Error un-mmapping the file");
        exit(EXIT_FAILURE);
    }

    close(fd);
    
    return 0;
}