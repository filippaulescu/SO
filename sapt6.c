/*utilizatorul poate specifica numele directorului ( in linie de comanda)
primescu argumentul si il verific daca e director cu lstat; daca nu e director returnez un mesaj
daca e director il deschid ( verific)
il parcurg si imi fac un snapshot ( un fisier ce contine nume, inode number, dimensiune, ultima modificare, etc?)

idee snapshot
DIR : informatii
DIR: dir1: inf
DIR: dir1: inf
DIR: file: inf
DIR: dir3: inf
DIR: dir3: file: inf
DIR: dir3: file2: inf

problema 1: program c ce primeste ca argument un director si parcurg directorul cu toate intrarile si directoarele
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//biblioteci incluse pt lstat; furnizeaza definita unor tipuri de date
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>

void listDirectoryRecursively(DIR *dir, const char *currentPath, int snapshot) {
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        // Ignorăm intrările speciale "." și ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        // Construim calea completă către intrarea curentă
        char fullPath[1024];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", currentPath, entry->d_name);

        // Obținem informații despre intrarea curentă
        struct stat fileStat;
        if (lstat(fullPath, &fileStat) < 0) {
            perror("eroare in lstat");
            continue;
        }

        // Construim mesajul pentru snapshot
        char buffer[1024];
        int len = snprintf(buffer, sizeof(buffer), "%s: ", fullPath);
        len += snprintf(buffer + len, sizeof(buffer) - len, "inode number: %ld, size: %ld, last access time: %ld, permissions: %o\n",
                        (long)fileStat.st_ino, (long)fileStat.st_size, (long)fileStat.st_atime, (unsigned int)fileStat.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO));

        if (len < 0 || len >= sizeof(buffer)) {
            perror("eroare in snprintf");
            exit(EXIT_FAILURE);
        }

        // Scriem mesajul în fișierul de snapshot
        if (write(snapshot, buffer, len) < 0) {
            perror("eroare in write");
            exit(EXIT_FAILURE);
        }

        // Verificăm dacă intrarea este un director
        if (S_ISDIR(fileStat.st_mode)) {
            // Deschidem subdirectorul
            DIR *subdir = opendir(fullPath);
            if (subdir == NULL) {
                perror("eroare in opendir");
                continue;
            }

            // Apelăm recursiv listDirectoryRecursively pentru subdirector
            listDirectoryRecursively(subdir, fullPath, snapshot);

            // Închidem subdirectorul
            closedir(subdir);
        }
    }
}



int main(int argc, char* argv[]) {

    if (argc != 2) {
        perror("Numar de argumente incorect");
        exit(EXIT_FAILURE);
    }


    struct stat fileStat;

    if (lstat(argv[1], &fileStat) < 0) {
        perror("eroare in lstat");
        exit(EXIT_FAILURE);
    }

    //verificare director
    if (!S_ISDIR(fileStat.st_mode)) {
        perror("is not a directory.");
        exit(EXIT_FAILURE);
    }

    //deschidere + verificare deschidere
    DIR *dir = opendir(argv[1]);
    if (dir == NULL) {
        perror("eroare in opendir");
        exit(EXIT_FAILURE);
    }

    int snapshot = open("snapshot.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);

    listDirectoryRecursively(dir, argv[1], snapshot);

    close(snapshot);
    closedir(dir);
    return 0;
}
