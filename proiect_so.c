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
#include <sys/types.h>
#include <sys/wait.h>
#include <stdbool.h>

#define MAX_BUFFER_SIZE 500
#define MAX_SCRIPT_PATH_SIZE 1024

bool has_no_permissions(char *file_path)
{
    struct stat file_stat;
    if (stat(file_path, &file_stat) == -1)
    {
        perror("Error: Cannot get file information");
        return true;
    }
    if ((file_stat.st_mode & S_IRWXU) == 0 &&
        (file_stat.st_mode & S_IRWXG) == 0 &&
        (file_stat.st_mode & S_IRWXO) == 0)
    {
        return true;
    }
    return false;
}

void process_directory(char *dir_name, int snapshot_fd, char *isolated_space_dir, int *nr_malitioase)
{
    DIR *dirp;
    struct dirent *d;
    struct stat file_info;

    if ((dirp = opendir(dir_name)) == NULL)
    {
        printf("Error: Cannot open directory %s\n", dir_name);
        return;
    }

    while ((d = readdir(dirp)) != NULL)
    {
        char file_path[MAX_SCRIPT_PATH_SIZE];
        snprintf(file_path, PATH_MAX, "%s/%s", dir_name, d->d_name);

        if (stat(file_path, &file_info) == -1)
        {
            printf("Error: Cannot get file information for %s\n", file_path);
            continue;
        }

        if (strcmp(d->d_name, ".") == 0 || strcmp(d->d_name, "..") == 0)
            continue;

        if (has_no_permissions(file_path))
        {
	 int pipe_fd[2];
                if (pipe(pipe_fd) == -1)
                {
                    perror("Pipe creation failed");
                    exit(EXIT_FAILURE);
                }

                pid_t pid = fork();
                if (pid == 0)
                {
                    close(pipe_fd[0]); 

                    dup2(pipe_fd[1], STDOUT_FILENO);
                    execl("/bin/bash", "sh", "verify_for_malicious.sh", file_path, (char *)NULL);

                    perror("Error executing sh");
                    exit(EXIT_FAILURE);
                }
                else if (pid > 0)
                {
                    close(pipe_fd[1]);
                    char buffer[256];
                    ssize_t nbytes = read(pipe_fd[0], buffer, sizeof(buffer));
                    if (nbytes > 0)
                    {
                        buffer[nbytes] = '\0';
                        if (strcmp(buffer, "SAFE\n") != 0)
                        {
                            printf("Procesul cu PID ul %d a găsit un fișier periculos: %s\n", pid, file_path);
                            *nr_malitioase = *nr_malitioase + 1;
                            pid_t pid = fork();
                            if (pid < 0)
                            {
                                perror("Error forking process");
                                exit(EXIT_FAILURE);
                            }
                            else if (pid == 0)
                            {
                                execl("/bin/mv", "mv", file_path, isolated_space_dir, (char *)NULL);
                                perror("Error executing mv");
                                exit(EXIT_FAILURE);
                            }
                            else
                            {
                                int status;
                                waitpid(pid, &status, 0);
                                if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
                                {
                                    printf("Failed to move the file\n");
                                }
                            }
                        }
                        else
                        {
                            printf("Procesulul cu PID ul %d a verificat și nu este periculos: %s\n", pid, file_path);
                        }
                    }
                    int status;
                    waitpid(pid, &status, 0);
                    printf("Procesul s-a încheiat cu PID ul %d și cu statusul %d\n", pid, WEXITSTATUS(status));
                }
                else
                {
                    perror("Error forking process");
                    exit(EXIT_FAILURE);
                }
	}
	  /* //cerinta 4
            printf("%s has no permission\n", file_path);
            pid_t pid = fork();
            if (pid == -1)
            {
                perror("Error: Failed to fork process");
                exit(EXIT_FAILURE);
            }
            else if (pid == 0)
            {


                execl("/bin/bash", "sh", "verify_for_malicious.sh", file_path, isolated_space_dir, (char *)NULL); // mută fișierul
                perror("Error executing sh");                                     
                exit(EXIT_FAILURE);
                
            }
            else
            {
                int status;
                pid_t result = waitpid(pid, &status, 0);
                if (result == -1)
                {
                    perror("Error: Failed to wait for child process");
                    exit(EXIT_FAILURE);
                }
                else if (WIFEXITED(status))
                {
                    int exit_status = WEXITSTATUS(status);
                    if (exit_status == 0)
                    {
                        printf("Process for %s ended successfully.\n", file_path);
                        printf("Skipping snapshot creation for %s due to being a dangerous file.\n", file_path);
                        return;
                    }
                    else
                    {
                        printf("Process for %s failed with exit status: %d\n", file_path, exit_status);
                    }
                }
                else
                {
                    printf("Process for %s terminated abnormally\n", dir_name);
                }
            }
        }
	  */
        char buffer[MAX_BUFFER_SIZE];
        sprintf(buffer, "name:%s, type:%d, i_node:%ld, ", d->d_name, d->d_type, d->d_ino);
        write(snapshot_fd, buffer, strlen(buffer));

        char time_buffer[MAX_BUFFER_SIZE];
        strftime(time_buffer, sizeof(time_buffer), "last modified:%Y-%m-%d %H:%M:%S, ", localtime(&file_info.st_mtime));
        write(snapshot_fd, time_buffer, strlen(time_buffer));
        strftime(time_buffer, sizeof(time_buffer), "last accessed:%Y-%m-%d %H:%M:%S\n", localtime(&file_info.st_atime));
        write(snapshot_fd, time_buffer, strlen(time_buffer));

        if (S_ISDIR(file_info.st_mode))
        {
	  process_directory(file_path, snapshot_fd, isolated_space_dir, nr_malitioase);
        }
    }

    closedir(dirp);
}

    void compare_and_update_snapshot(char *dir_name, char *output_dir, char *isolated_space_dir, int *nr_malitioase)
{
    char snapshot_file[PATH_MAX];
    char temp_file[PATH_MAX];

    int snapshot_fd;
    snprintf(snapshot_file, PATH_MAX, "%s/%s_snapshot.txt", output_dir, basename((char *)dir_name));
    if ((snapshot_fd = open(snapshot_file, O_RDONLY)) == -1)
    {
        if (errno == ENOENT)
        {
            if ((snapshot_fd = open(snapshot_file, O_WRONLY | O_CREAT | O_TRUNC, 0644)) == -1)
            {
                printf("Error: Cannot open output file\n");
                return;
            }

            process_directory(dir_name, snapshot_fd, isolated_space_dir, nr_malitioase);

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
    // daca nu a fost primul apel de creeare de snapshot, facem un fisier temporar cu care sa comparam apoi diferentele
    int temp_fd;
    snprintf(temp_file, PATH_MAX, "%s_temp.txt", dir_name);
    if ((temp_fd = open(temp_file, O_RDONLY)) == -1)
    {
        if (errno == ENOENT)
        {
            if ((temp_fd = open(temp_file, O_WRONLY | O_CREAT | O_TRUNC, 0644)) == -1)
            {
                printf("Error: Cannot open temp file\n");
                return;
            }

            process_directory(dir_name, temp_fd, isolated_space_dir, nr_malitioase);

            close(temp_fd);
            printf("A snapshot already exists, a snapshot comparison temporary file for directory %s was created.\n", dir_name);
        }
        else
        {
            printf("Error: Cannot open temp file %s\n", snapshot_file);
            return;
        }
    }

    char command[2 * PATH_MAX + 50];
    snprintf(command, sizeof(command), "diff -uN --strip-trailing-cr %s %s > /dev/null", snapshot_file, temp_file);

    if (system(command) == 0)
    {
        printf("No changes found in %s\n", dir_name);
        remove(temp_file);
        return;
    }
    else
    {
        printf("Changes found in %s. Updating snapshot.\n", dir_name);

        FILE *snapshot_fp = fopen(snapshot_file, "r");
        FILE *temp_fp = fopen(temp_file, "r");

        if (snapshot_fp && temp_fp)
        {
            printf("Modified files:\n");
            char snapshot_line[MAX_BUFFER_SIZE];
            char temp_line[MAX_BUFFER_SIZE];

            while (fgets(snapshot_line, sizeof(snapshot_line), snapshot_fp) && fgets(temp_line, sizeof(temp_line), temp_fp))
            {
                if (strcmp(snapshot_line, temp_line) != 0)
                {
                    if (strstr(temp_line, snapshot_line) == NULL)
                    {
                        printf("Modified: %s", snapshot_line);
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
    }
    rename(temp_file, snapshot_file);
}

int main(int argc, char **argv)
{
    if (argc < 6 || argc > 13)
    {
        printf("Usage: %s -o <output_path> -s <izolated_space_path> <dir_path>\n", argv[0]);
        return 0;
    }

    DIR *dir;
    int o_d;
    char *dir_name = NULL;
    char *output_dir = NULL;
    char *isolated_space_dir = NULL;
    int nr_malitioase = 0;

    for (int i = 1; i < argc; i++){
      if (strcmp(argv[i], "-o") == 0 && (i + 1) < argc){
            i++;
            output_dir = argv[i];
            if ((o_d = open(output_dir, O_RDONLY)) == -1)
	      {
                printf("Error: Cannot open output file\n");
                return 0;
	      }
            continue;
      }
      if (strcmp(argv[i], "-s") == 0 && (i + 1) < argc){
	i++;
	isolated_space_dir = argv[i];
        
	continue;
      }
      dir_name = argv[i];
      if ((dir = opendir(argv[i])) == NULL){
	continue;
      }
      nr_malitioase=0;
      compare_and_update_snapshot(dir_name, output_dir, isolated_space_dir, &nr_malitioase);
      printf("Pentru directorul %s s au gasit %d fisiere malitioase\n",dir_name, nr_malitioase);
    }

    close(o_d);

    return 0;
}
