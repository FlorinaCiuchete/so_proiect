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
                int nr_bytes = read(pipe_fd[0], buffer, sizeof(buffer));
                if (nr_bytes > 0)
                {
                    buffer[nr_bytes] = '\0';
                    if (strcmp(buffer, "SAFE\n") != 0)
                    {
                        printf("Procesul cu PID ul %d a găsit un fișier periculos: %s\n", pid, file_path);
                        *nr_malitioase = *nr_malitioase + 1;
                        pid_t pid = fork();
                        if (pid < 0)
                        {
                            return;
                        }
                        else if (pid == 0)
                        {
                            execl("/bin/mv", "mv", file_path, isolated_space_dir, (char *)NULL);
                            return;
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
        char time_buffer[MAX_BUFFER_SIZE];
        strftime(time_buffer, sizeof(time_buffer), "last modified:%Y-%m-%d %H:%M:%S", localtime(&file_info.st_mtime));

        char buffer[2*MAX_BUFFER_SIZE];
        sprintf(buffer, "name:%s, %s , type:%d, i_node:%ld\n", d->d_name,time_buffer, d->d_type, d->d_ino);
        write(snapshot_fd, buffer, strlen(buffer));

        

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
        int snapshot_fd = open(snapshot_file, O_RDONLY);
        if (snapshot_fd == -1)
        {
            perror("Error opening snapshot file for reading");
            return;
        }

        int temp_fd = open(temp_file, O_RDONLY);
        if (temp_fd == -1)
        {
            perror("Error opening temp file for reading");
            return;
        }

        FILE *snapshot_fp = fdopen(snapshot_fd, "r");
        if (snapshot_fp == NULL)
        {
            perror("Failed to convert file descriptor to FILE * for snapshot");
            close(snapshot_fd); 
            return;
        }

        FILE *temp_fp = fdopen(temp_fd, "r");
        if (temp_fp == NULL)
        {
            perror("Failed to convert file descriptor to FILE * for temp");
            close(temp_fd); 
            return;
        }


        if (snapshot_fp && temp_fp)
        {
            printf("Modified files:\n");
            char snapshot_mat[100][2*MAX_BUFFER_SIZE],temp_mat[100][2*MAX_BUFFER_SIZE];
            int size_snap=0;
            long inode_snap[100],inode_temp[100];
            char *p,*aux;
            while (fgets(snapshot_mat[size_snap], sizeof(snapshot_mat[size_snap]), snapshot_fp)){
                p=strstr(snapshot_mat[size_snap],"i_node:");
                p=p+7;
                aux=strtok(p,",");
                inode_snap[size_snap]=strtol(aux,NULL,10);
                size_snap=size_snap+1;
            }
            int size_temp=0;
            while (fgets(temp_mat[size_temp], sizeof(temp_mat[size_temp]), temp_fp)){
                p=strstr(temp_mat[size_temp],"i_node:");
                p=p+7;
                aux=strtok(p,",");
                inode_temp[size_temp]=strtol(aux,NULL,10);
                size_temp=size_temp+1;
            }
            for(int i=0;i<size_snap;i++)
            {
                for(int j=0;j<size_temp;j++)
                    if(inode_snap[i]==inode_temp[j])
                    {
                        if(strcmp(snapshot_mat[i],temp_mat[j]))
                        {
                            printf("Modified from %s  -to- %s\n",snapshot_mat[i],temp_mat[j]);
                        }
                    }
            }
            for(int i=0;i<size_snap;i++)//vechi
            {
                int gasit=0;
                for(int j=0;j<size_temp;j++)
                    if(inode_snap[i]==inode_temp[j])
                        gasit=1;
                if(gasit==0)
                {
                    printf("The file %s   --was deleted--\n",snapshot_mat[i]);
                }
            }
            for(int i=0;i<size_temp;i++)//nou
            {
                int gasit=0;
                for(int j=0;j<size_snap;j++)
                    if(inode_temp[i]==inode_snap[j])
                        gasit=1;
                if(gasit==0)
                {
                    printf("The file %s  --is NEW--\n",temp_mat[i]);
                }
            }
            fclose(snapshot_fp);
            close(snapshot_fd);
            fclose(temp_fp);
            close(temp_fd);
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

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-o") == 0 && (i + 1) < argc)
        {
            i++;
            output_dir = argv[i];
            if ((o_d = open(output_dir, O_RDONLY)) == -1)
            {
                printf("Error: Cannot open output file\n");
                return 0;
            }
            continue;
        }
        if (strcmp(argv[i], "-s") == 0 && (i + 1) < argc)
        {
            i++;
            isolated_space_dir = argv[i];

            continue;
        }
        dir_name = argv[i];
        if ((dir = opendir(argv[i])) == NULL)
        {
            continue;
        }
        nr_malitioase = 0;
        compare_and_update_snapshot(dir_name, output_dir, isolated_space_dir, &nr_malitioase);
        printf("Pentru directorul %s s au gasit %d fisiere malitioase\n", dir_name, nr_malitioase);
    }

    close(o_d);

    return 0;
}
