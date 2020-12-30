#include <dirent.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include "dir.h"
#include <sys/stat.h>
#include <string.h>
#include <stdbool.h>


/* 
   Arguments: 
      fd - a valid open file descriptor. This is not checked for validity
           or for errors with it is used.
      directory - a pointer to a null terminated string that names a 
                  directory

   Returns
      -1 the named directory does not exist or you don't have permission
         to read it.
      -2 insufficient resources to perform request

 
   This function takes the name of a directory and lists all the regular
   files and directories in the directory.
 

 */

int listFiles(int fd, char * directory) {

  // Get resources to see if the directory can be opened for reading
  
  DIR * dir = NULL;
  
  dir = opendir(directory);
  if (!dir) return -1;
  
  // Setup to read the directory. When printing the directory
  // only print regular files and directories. 

  struct dirent *dirEntry;
  int entriesPrinted = 0;
  
  for (dirEntry = readdir(dir);
       dirEntry;
       dirEntry = readdir(dir)) {
    if (dirEntry->d_type == DT_REG) {  // Regular file
      struct stat buf;

      // This call really should check the return value
      stat(dirEntry->d_name, &buf);

	dprintf(fd, "F    %-20s     %d\n", dirEntry->d_name, buf.st_size);
    } else if (dirEntry->d_type == DT_DIR) { // Directory
      dprintf(fd, "D        %s\n", dirEntry->d_name);
    } else {
      dprintf(fd, "U        %s\n", dirEntry->d_name);
    }
    entriesPrinted++;
  }
  
  // Release resources
  closedir(dir);
  return entriesPrinted;
}

/*
 * Returns the parent directory's entire path
 * Argument: char array representing the directory path to copy the current directory path into
 */

void getParentDirectoryPath(char path[]) {
    char current[100];
    char currentcpy[100];
    bzero(currentcpy, 100);

    char parent[100] = "/";
    char* final;

    getcwd(current, sizeof(current));
    strcpy(currentcpy, current);

    char* piece = strtok(current, "/");

    int count = 0;
    while (piece != NULL) {
        count++;
        piece = strtok(NULL, "/");
    }

    piece = strtok(currentcpy, "/");
    strcat(parent, piece);

    for (int i = 0; i < count - 2; i++) {
        piece = strtok(NULL, "/");
        strcat(parent, "/");
        strcat(parent, piece);
    }

    bzero(path, 100);
    strcpy(path, parent);
}




   
