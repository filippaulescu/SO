#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/wait.h>
#include <errno.h>



int checkPermissions(const char *path) {
    struct stat fileStat;
    if (lstat(path, &fileStat) == 0) {
        // Verifică dacă permisiunile de scriere lipsesc
        if (!(fileStat.st_mode & (S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH))) {
            return 1; // Fișierul nu are nicio permisiune setată
        }
    } else {
        perror("Eroare la obținerea informațiilor fișierului");
        return -1; // Eroare la obținerea informațiilor, nu cred ca o sa o folosesc
    }
    return 0; // Fișierul are permisiuni setate
}

// Funcția pentru mutarea fișierului într-un nou director
void moveFileToDirectory(const char *sourcePath, const char *destinationDir) {

    char destinationPath[1024];
    snprintf(destinationPath, sizeof(destinationPath), "%s/%s", destinationDir, strrchr(sourcePath, '/') + 1);
    if (rename(sourcePath, destinationPath) == -1) {
        perror("Eroare la mutarea fișierului");
    } else {
        printf("Fișierul a fost mutat cu succes la: %s\n", destinationPath);
    }
}

int processFile(const char *fullPath,const char *izolarePath) {


    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("Pipe failed");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();

    if (pid == -1) {
        perror("Eroare la fork");
    } else if (pid == 0) {
        // Procesul copil
        close(pipefd[0]);
        dup2(pipefd[1], 1);// 1 e stdout, am facut redirectare aici, stdout scrie  direct in pipe;
        close(pipefd[1]);

        execl("./verificare_fisier.sh", "verificare_fisier.sh", fullPath, (char *)NULL);
        perror("Eroare la exec");
        exit(EXIT_FAILURE);
    } else {
        // Procesul părinte așteaptă copilul să termine
        close(pipefd[1]);
        int status;
        wait(&status);

        if (WIFEXITED(status)) {
            char buffer[512] = {0};
            read(pipefd[0], buffer, sizeof(buffer));
            close(pipefd[1]);
            if( strcmp(buffer, "SAFE") != 0){
                moveFileToDirectory(fullPath, izolarePath);
                return 1;
            }
            //printf(" A AJUNS in process!!!\n");
        }
    }
    return 0;
}


void listDirectoryRecursively(DIR *dir, const char *currentPath, int snapshot,const char *izolarePath, int* nrFisiereMalitioase) {

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
            listDirectoryRecursively(subdir, fullPath, snapshot, izolarePath, nrFisiereMalitioase);

            // Închidem subdirectorul
            closedir(subdir);
            continue;
        }

        // Dacă fișierul nu are permisiuni, executam scriptul de verificare
        if (checkPermissions(fullPath) == 1) {
            *nrFisiereMalitioase +=processFile(fullPath, izolarePath);
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

int createSnapshot(DIR *dir_entry, const char *dir_name, const char *dir_output,const char *dir_izolare, int is_aux) {
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
    int nrFisiereMalitioase =  0;
    listDirectoryRecursively(dir_entry, dir_name, snapshot_file, dir_izolare, &nrFisiereMalitioase);
    // Închidem fișierul pentru snapshot
    if (close(snapshot_file) == -1) {
        perror("Eroare inchidere snapshot");
    }
    //printf("Fisiere malitioase Create: %d\n", nrFisiereMalitioase);
    return nrFisiereMalitioase;
}




int compareAndClean(const char *old_file, const char *new_file) {
    char cmd[2048];  // auxiliar pentru crearea de comenzi. acestea se vor rula prim system(cmd) ca si comenzi in schell
    //diff - comanda ce verifica daca doua fisiere sunt diferite. /dev/null este o optiune care permite sa ignori " locul unde s-au gasit modificari" si sa primesti doar rezultatul comenzii diff

    // Verifică dacă fișierul vechi există
    if (access(old_file, F_OK) != 0) {
        // Fișierul vechi nu există, deci trebuie să copiem noul fișier
        snprintf(cmd, sizeof(cmd), "cp %s %s", new_file, old_file);
        system(cmd);
        printf("Snapshot created.\n");
    } else {
        // Ambele fișiere există, compară-le
        snprintf(cmd, sizeof(cmd), "diff %s %s > /dev/null", old_file, new_file);
        int diff = system(cmd);
        if (diff != 0) {
            // Fișierele sunt diferite, actualizează fișierul vechi
            snprintf(cmd, sizeof(cmd), "cp %s %s", new_file, old_file);
            system(cmd);
            printf("Snapshot updated.\n");
        } else {
            printf("No changes detected.\n");
        }
    }

    //cp -copiaza continutul din primul fisier in al doilea fisier
    //rm - sterge fisierul dat ca parametru
    // Șterge fișierul nou, deoarece nu mai este necesar
    snprintf(cmd, sizeof(cmd), "rm %s", new_file);
    system(cmd);
    return 1;  // Returnează un cod non-zero pentru a indica o actualizare
}



int main(int argc, char** argv) {

    //Verificare număr argumente (10 directoare, 2 pt output si 2 pt izolare
    if (argc < 4 || argc > 14) {
        fprintf(stderr, "Numar incorect de argumente\n");
        exit(EXIT_FAILURE);
    }

    int poz_dir_output = -1;
    int poz_dir_izolare = -1;


    // Cautăm directorul de ieșire si pe cel de izolare
    for (int i = 1; i < argc; i ++) {
        if (strcmp(argv[i], "-o") == 0) {
            if (i+1 < argc) {
                poz_dir_output = ++i;

            }
            else {
                fprintf(stderr, "Fara director de iesire\n");
                exit(EXIT_FAILURE);
            }
        }
        if (strcmp(argv[i], "-s") == 0) {
            if (i+1 < argc) {
                poz_dir_izolare = ++i;

            }
            else {
                fprintf(stderr, "Fara director de izolare\n");
                exit(EXIT_FAILURE);
            }
    }
    }
     if( poz_dir_izolare == -1 || poz_dir_output == -1){
                printf("Argumentele nu sunt conforme. Lipsesc directoarele\n");
                exit(EXIT_FAILURE);
        }




    struct stat buf;
    pid_t pid;
    int nrFisiereMalitioase =0;

    // Parcurgem directoarele pentru care facem snapshot-uri
    for (int i = 1; i < argc; i ++) {
        // Trecem peste directorul de ieșire
        if (i == poz_dir_output || i == poz_dir_output -1 || i == poz_dir_izolare || i == poz_dir_izolare -1  ) {
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



                nrFisiereMalitioase = createSnapshot(dir_entry, argv[i], argv[poz_dir_output], argv[poz_dir_izolare], 1);
                char old_snapshot[1024], new_snapshot[1024];
                sprintf(old_snapshot, "%s/%ld_snapshot.txt", argv[poz_dir_output], (long)buf.st_ino);
                sprintf(new_snapshot, "%s/%ld_snapshot_aux.txt", argv[poz_dir_output], (long)buf.st_ino);
                compareAndClean(old_snapshot, new_snapshot);


                if (closedir(dir_entry) == -1) {
                    perror("Eroare inchidere director:");
                } // Închidere directorul curent

                exit(nrFisiereMalitioase);
            }
        }
    }

    int status;
    int pid_f;

    while(1){
        pid_f = wait(&status);
        if(pid_f == -1){
                if( errno != ECHILD){
                        perror("Eroare la wait");
                        exit(EXIT_FAILURE);
                }
        break;
        }
        printf("Child Process termineted whit PID %d and had %d malicious files\n", pid_f, WEXITSTATUS(status));
    }
    return 0;
}
