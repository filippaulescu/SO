#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/wait.h>

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
            perror("Eroare in lstat");
            continue;
        }

         // Verificăm dacă intrarea este un director
        if (S_ISDIR(fileStat.st_mode)) {
            // Deschidem subdirectorul
            DIR *subdir = opendir(fullPath);
            if (subdir == NULL) {
                perror("Eroare in opendir");
                continue;
            }

            // Apelăm recursiv listDirectoryRecursively pentru subdirector
            listDirectoryRecursively(subdir, fullPath, snapshot);

            // Închidem subdirectorul
            closedir(subdir);
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
            fprintf(stderr, "Eroare in sprintf; incercam sa introducem pea multe date\n");
            exit(EXIT_FAILURE);
        }

        // Scriem mesajul în fișierul de snapshot
        if (write(snapshot, buffer, len) < 0) {
            perror("eroare in write");
            exit(EXIT_FAILURE);
        }


    }
}

void createSnapshot(DIR *dir_entry, const char *dir_name, const char *dir_output, int is_aux) {
    struct stat dir_stat;
    if (lstat(dir_name, &dir_stat) != 0) {
        perror("Erare in lstat");
        exit(EXIT_FAILURE);
    }
    char file_name[1024];
    sprintf(file_name, "%s/%ld_snapshot%s.txt", dir_output, (long)dir_stat.st_ino, is_aux ? "_aux" : "");
     int snapshot_file = open(file_name, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH); // Creare fișier, chiar dacă acesta nu există deja

    if (snapshot_file == -1) {
        perror("Eroare deschidere snapshot");
        exit(EXIT_FAILURE);
    }
    // Apelăm funcția listDirectoryRecursively pentru a face snapshot-uri
    listDirectoryRecursively(dir_entry, dir_name, snapshot_file);
    // Închidem fișierul pentru snapshot
    if (close(snapshot_file) == -1) {
        perror("Eroare inchidere snapshot");
    }
}

int compareAndClean(const char *old_file, const char *new_file) {
    char cmd[2048]; // auxiliar pentru crearea de comenzi. acestea se vor rula prim system(cmd) ca si comenzi in schell
    //diff - comanda ce verifica daca doua fisiere sunt diferite. /dev/null este o optiune care permite sa ignori " locul unde s-au gasit modificari" si sa primesti doar rezultatul comenzii diff
    snprintf(cmd, sizeof(cmd), "diff %s %s > /dev/null", old_file, new_file);
    int diff = system(cmd);
    if (diff != 0) { // files are different
        //cp -copiaza continutul din primul fisier in al doilea fisier
        snprintf(cmd, sizeof(cmd), "cp %s %s", new_file, old_file);
        system(cmd);
        printf("Snapshot updated.\n");
    } else {
        printf("No changes detected.\n");
    }
    //rm - sterge fisierul dat ca parametru
    snprintf(cmd, sizeof(cmd), "rm %s", new_file);
    system(cmd);
    return diff;
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
    pid_t pid;

    // Parcurgem directoarele pentru care facem snapshot-uri
    for (int i = 1; i < argc; i ++) {
        // Trecem peste directorul de ieșire
        if (i == poz_dir_output || i == poz_dir_output -1 ) {
            continue;
        }

        // Verificăm dacă argumentul dat este un director
        if (lstat(argv[i], &buf) == 0 && S_ISDIR(buf.st_mode)) {

            pid = fork();

            if (pid < 0) {
                perror("proces nereusit : ");
            }
            else if (pid == 0) {
                //cod fiu

                DIR *dir_entry;
                //printf("%d %s\n", getpid(), argv[i]);

                // Deschidere directorul curent
                if ((dir_entry = opendir(argv[i])) == NULL) {
                    perror("Eroare deschidere director:");
                    continue;
                }




                createSnapshot(dir_entry, argv[i], argv[poz_dir_output], 1);
                char old_snapshot[1024], new_snapshot[1024];
                sprintf(old_snapshot, "%s/%ld_snapshot.txt", argv[poz_dir_output], (long)buf.st_ino);
                sprintf(new_snapshot, "%s/%ld_snapshot_aux.txt", argv[poz_dir_output], (long)buf.st_ino);
                compareAndClean(old_snapshot, new_snapshot);


                if (closedir(dir_entry) == -1) {
                    perror("Eroare inchidere director:");
                } // Închidere directorul curent

                exit(0);
            }
        }
    }

    int status;
    int pid_f;

    for (int  i = 1; i <= argc - 3; i ++) {
        pid_f = wait(&status);
        //printf("Child Process %d termineted whit PID %d and a exit code %d\n", i, pid_f, WEXITSTATUS(status));
    }
    return 0;
}

