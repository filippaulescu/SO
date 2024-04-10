#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
        // buffer + len => snprintf suprascrie peste buffer by default. trebuie specificat de unde sa scrie altfel
        //sizeof buffer - len => daca ce vreau sa scriu e mai mic decat asta, scrie si returneaza size scris. daca e mai mare, se trateaza in if-ul de mai jos ca eroare
        len += snprintf(buffer + len, sizeof(buffer) - len, "inode number: %ld, size: %ld, last access time: %ld, permissions: %o\n",
                        (long)fileStat.st_ino, (long)fileStat.st_size, (long)fileStat.st_atime, (unsigned int)fileStat.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO));

        if (len < 0 || len >= sizeof(buffer)) {
            fprintf(stderr, "eroare in sprintf; incercam sa introducem pea multe date\n");
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

void snap_file_make(DIR *dir_entry, const char *dir_name, const char *dir_output) {
    char file_name[1024];

    sprintf(file_name, "%s/%s_snapshot.file.txt", dir_output, dir_name); // Numele fișierului pentru snapshot-ul curent

    int snapshot_file = open(file_name, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH); // Creare fișier, chiar dacă acesta nu există deja

    if (snapshot_file == -1) {
        perror("Eroare deschidere fisier snapshot:");
    } else {
        // Apelăm funcția listDirectoryRecursively pentru a face snapshot-uri
        listDirectoryRecursively(dir_entry, dir_name, snapshot_file);

        if (close(snapshot_file) == -1) {
            perror("Eroare inchidere fisier snapshot:");
        } // Închidem fișierul pentru snapshot
    }
}


int main(int argc, char** argv) {

    //Verificare număr argumente
    if (argc < 4 || argc > 10) {
        fprintf(stderr, "Numar incorect de argumente\n");
        exit(EXIT_FAILURE);
    }

    int poz_dir_output = -1;

    // Cautăm directorul de ieșire
    for (int i = 1; i < argc; i ++) {
        if (strcmp(argv[i], "-o") == 0) {
            if (i+1 < argc) {
                poz_dir_output = ++i;
                break;
            }
            else {
                fprintf(stderr, "Fara director de iesire\n");
                exit(EXIT_FAILURE);
            }
        }
    }

    struct stat buf;

    // Parcurgem directoarele pentru care facem snapshot-uri
    for (int i = 1; i < argc; i ++) {
        // Trecem peste directorul de ieșire
        if (i == poz_dir_output || i == poz_dir_output -1 ) {
            continue;
        }

        // Verificăm dacă argumentul dat este un director
        if (lstat(argv[i], &buf) == 0 && S_ISDIR(buf.st_mode)) {
            DIR *dir_entry;


            // Deschidere directorul curent
            if ((dir_entry = opendir(argv[i])) == NULL) {
                perror("Eroare deschidere director:");
                continue;
            }

            snap_file_make(dir_entry, argv[i], argv[poz_dir_output]);

            if (closedir(dir_entry) == -1) {
                perror("Eroare inchidere director:");
            } // Închidere directorul curent
        }
    }

    return 0;
}
