#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>


int main(int argc, char **argv)
{
    DIR* dirp;
    struct stat st;
    struct dirent *d;

    if(argc!=2)
        printf ("nr gresit de argumente\n");

    
    if((dirp=opendir(argv[1]))==NULL)
        printf("nu s-a dat un director valid\n");

    
    if(stat(argv[1], &st)<0)
        printf("eroare\n");

    while((d=readdir(dirp))!=NULL)
    {
        printf("%s\n", d->d_name);
    }
    //struct dirent * dir_ent;
    //dir_ent=readdir(dirp);
return 0;
}