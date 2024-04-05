#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <time.h>


int main(int argc, char **argv)
{
    DIR* dirp;
    struct stat file_info;
    struct dirent *d;

    if(argc!=2)
        printf ("nr gresit de argumente\n");

    
    if((dirp=opendir(argv[1]))==NULL)
        printf("nu s-a dat un director valid\n");
    if(stat(argv[1], &file_info)==-1) //functia da info despre argv[1]
      printf("eroare\n");

    while((d=readdir(dirp))!=NULL)
    {
      char file_path[PATH_MAX];
        snprintf(file_path, PATH_MAX, "%s/%s", argv[1], d->d_name);

        if(stat(file_path, &file_info) == -1)
        {
            printf("eroare");
        }
      if(strcmp(d->d_name,".")==0 || strcmp(d->d_name,"..")==0)
            continue;
      
      printf("name:%s, type:%d, i_node:%ld, ", d->d_name, d->d_type,d-> d_ino);
      char time_buffer[50];
      strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", localtime(&file_info.st_mtime));
      printf("%s\n", time_buffer);
    }
return 0;
}
