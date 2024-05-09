#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <libgen.h>

#define MAX_BUFFER_SIZE 500

void process_directory(const char *dir_name, int snapshot_fd)
{
  DIR *dirp;
  struct dirent *d;
  struct stat file_info;
  
  if ((dirp = opendir(dir_name)) == NULL){
    printf("Error: Cannot open directory %s\n", dir_name);
    return;
  }
  
  while ((d = readdir(dirp)) != NULL){
    char file_path[PATH_MAX];
    snprintf(file_path, PATH_MAX, "%s/%s", dir_name, d->d_name);
    
    if (stat(file_path, &file_info) == -1){
      printf("Error: Cannot get file information for %s\n", file_path);
      continue;
    }
    
    if (strcmp(d->d_name, ".") == 0 || strcmp(d->d_name, "..") == 0)
      continue;
    
    char buffer[MAX_BUFFER_SIZE];
    sprintf(buffer, "name:%s, type:%d, i_node:%ld, ", d->d_name, d->d_type, d->d_ino);
    write(snapshot_fd, buffer, strlen(buffer));
    
    char time_buffer[MAX_BUFFER_SIZE];
    strftime(time_buffer, sizeof(time_buffer), "last modified:%Y-%m-%d %H:%M:%S, ", localtime(&file_info.st_mtime));
    write(snapshot_fd, time_buffer, strlen(time_buffer));
    strftime(time_buffer, sizeof(time_buffer), "last accessed:%Y-%m-%d %H:%M:%S\n", localtime(&file_info.st_atime));
    write(snapshot_fd, time_buffer, strlen(time_buffer));
    
    if (S_ISDIR(file_info.st_mode)){
      process_directory(file_path, snapshot_fd);
    }
  }
  
  closedir(dirp);
}


void compare_and_update_snapshot(const char *dir_name)
{
  char snapshot_file[PATH_MAX];
  char temp_file[PATH_MAX];
  char diff_output_file[PATH_MAX];
  
  int snapshot_fd;
  snprintf(snapshot_file, PATH_MAX, "%s/%s_snapshot.txt", dir_name, basename((char *)dir_name));
  if ((snapshot_fd = open(snapshot_file, O_RDONLY)) == -1)
    {
      if (errno == ENOENT)
        {
	  //create_snapshot(dir_name);
	  if ((snapshot_fd = open(snapshot_file, O_WRONLY | O_CREAT | O_TRUNC, 0644)) == -1)
	    {
	      printf("Error: Cannot open output file\n");
	      return;
	    }
	  
	  process_directory(dir_name, snapshot_fd);
	  
	  close(snapshot_fd);
	  printf("Snapshot for directory %s created.\n", dir_name);
	  return;
        }
      else
        {
	  printf("Error: Cannot open snapshot file %s\n", snapshot_file);
	  return;
        }
    }
  //daca nu a fost primul apel de creeare de snapshot, facem un fisier temporar cu care sa comparam apoi diferentele
  int temp_fd;
  snprintf(temp_file, PATH_MAX, "%s/%s_temp.txt", dir_name, basename((char *)dir_name));
  if ((temp_fd = open(temp_file, O_RDONLY)) == -1)
    {
      if (errno == ENOENT)
        {
	  if ((temp_fd = open(temp_file, O_WRONLY | O_CREAT | O_TRUNC, 0644)) == -1)
	    {
	      printf("Error: Cannot open temp file\n");
	      return;
	    }
	  
	  process_directory(dir_name, temp_fd);
	  
	  close(temp_fd);
	  printf("Temp file for directory %s created.\n", dir_name);
        }
      else
        {
	  printf("Error: Cannot open temp file %s\n", snapshot_file);
	  return;
        }
    }

  
  snprintf(diff_output_file, PATH_MAX, "%s/%s_diff.txt", dir_name, basename((char *)dir_name));
  
  char command[2 * PATH_MAX + 50]; 
  snprintf(command, sizeof(command), "diff -uN --strip-trailing-cr %s %s > /dev/null", snapshot_file, temp_file);
  
  if (system(command) == 0) {
    printf("No changes found in %s\n", dir_name);
    remove(temp_file);
    remove(diff_output_file);
  }
  else{
    printf("Changes found in %s. Updating snapshot.\n", dir_name);
  }
  
  FILE *diff_fp = fopen(diff_output_file, "r");
  if (diff_fp){
    printf("Differences:\n");
    char diff_line[MAX_BUFFER_SIZE];
    while (fgets(diff_line, sizeof(diff_line), diff_fp)){
      printf("%s", diff_line);
    }
    fclose(diff_fp);
  }
  
  FILE *snapshot_fp = fopen(snapshot_file, "r");
  FILE *temp_fp = fopen(temp_file, "r");
  
  if (snapshot_fp && temp_fp)
    {
      printf("Renamed or deleted files:\n");
      char snapshot_line[MAX_BUFFER_SIZE];
      char temp_line[MAX_BUFFER_SIZE];
      
      while (fgets(snapshot_line, sizeof(snapshot_line), snapshot_fp) && fgets(temp_line, sizeof(temp_line), temp_fp))
	{
	  if (strcmp(snapshot_line, temp_line) != 0)
	    {
		if (strstr(temp_line, snapshot_line) == NULL)
		  {
		    printf("Deleted: %s", snapshot_line);
		  }
		else
		  {
                    printf("Renamed: %s -> %s", snapshot_line, temp_line);
		  }
	    }
	}
      while (fgets(snapshot_line, sizeof(snapshot_line), snapshot_fp))
	{
	  printf("Deleted: %s", snapshot_line);
	}
      while (fgets(temp_line, sizeof(temp_line), temp_fp))
	{
	  printf("Renamed: %s", temp_line);
	  }
      
      fclose(snapshot_fp);
      fclose(temp_fp);
      }
  else
      {
	printf("Error: Cannot open snapshot files for comparison\n");
      }
  
  rename(temp_file, snapshot_file);
}


int main(int argc, char **argv){
  if (argc != 2){
    printf("Usage: %s <directory_path>\n", argv[0]);
    return 1;
  }
  
  const char *dir_name = argv[1];
  
  compare_and_update_snapshot(dir_name);

  return 0;
} 
