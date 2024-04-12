#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h> // Add this for PATH_MAX


void process_directory(const char *dir_name, int fd)
{
    DIR *dirp;
    struct dirent *d;
    struct stat file_info;

    if((dirp = opendir(dir_name)) == NULL)
    {
        printf("Error: Cannot open directory %s\n", dir_name);
        return;
    }

    while((d = readdir(dirp)) != NULL)
    {
        char file_path[PATH_MAX];
	snprintf(file_path, PATH_MAX, "%s/%s", dir_name, d->d_name);

        if(stat(file_path, &file_info) == -1)
        {
            printf("Error: Cannot get file information for %s\n", file_path);
            continue;
        }

        if(strcmp(d->d_name, ".") == 0 || strcmp(d->d_name, "..") == 0)
            continue;

        char buffer[500];
        sprintf(buffer, "name:%s, type:%d, i_node:%ld, ", d->d_name, d->d_type, d->d_ino);
        write(fd, buffer, strlen(buffer));

        char time_buffer[500];
        strftime(time_buffer, sizeof(time_buffer), "last modified:%Y-%m-%d %H:%M:%S, ", localtime(&file_info.st_mtime));
        write(fd, time_buffer, strlen(time_buffer));
        strftime(time_buffer, sizeof(time_buffer), "last accessed:%Y-%m-%d %H:%M:%S\n", localtime(&file_info.st_atime));
        write(fd, time_buffer, strlen(time_buffer));

        if(S_ISDIR(file_info.st_mode))
        {
	   process_directory(file_path, fd); // Recursive call for subdirectories
        }
    }

    closedir(dirp);
}

int main(int argc, char **argv)
{
  if(argc < 3 || argc>12)
    {
        printf("nr gresit de argumente");
        return 1;
    }

    int fd;
    if((fd = open(argv[argc - 1], O_WRONLY | O_CREAT | O_TRUNC, 0644)) == -1)
    {
        printf("Error: Cannot open output file\n");
        return 1;
    }
    
    for(int i = 1; i < argc - 1; i++){
      DIR* dir;
      if((dir = opendir(argv[i])) == NULL)
        {
	  printf("Error: Cannot open directory %s\n", argv[i]);
	  continue;
        }
      
      
      
      process_directory(argv[1], fd);
      closedir(dir);
    }
    close(fd);
    
    return 0;
}


