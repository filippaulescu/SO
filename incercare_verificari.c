#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>  // Include pentru `basename`
#include <time.h>
#include <sys/wait.h>

#define MAX_PATH 260
#define MAX_DATA 1024

// Funcție pentru determinarea permisiunilor unui fișier
char* drepturi(mode_t mode) {
    char *dr = malloc(10);
    if (dr == NULL) {
        fprintf(stderr, "Eroare la alocarea memoriei pentru drepturi\n");
        return NULL;
    }
    strcpy(dr, "rwxrwxrwx");
    for (int i = 0; i < 9; i++) {
        if (!(mode & (1 << (8 - i)))) {
            dr[i] = '-';
        }
    }
    return dr;
}

// Funcție pentru verificarea absenței permisiunilor
int fara_drepturi(char *drepturi) {
    return strcmp(drepturi, "---------") == 0;
}

// Funcție pentru izolarea fișierelor suspecte
void izoleaza_fisier(const char* fisier_verificat, const char* dir_izolare) {
    char path_izolare[MAX_PATH];
    // Folosesc basaname. nu imi place si nu cred ca am voie. am sa incerc sa modific :))
    char* base = basename(strdup(fisier_verificat));  // Facem o copie pentru `basename`
    sprintf(path_izolare, "%s/%s", dir_izolare, base);

    if (rename(fisier_verificat, path_izolare) != 0) {
        perror("Eroare la mutarea fișierului suspect");
    }

    free(base);  // Eliberăm copia după utilizare
}

// Funcție pentru a executa scriptul de verificare și izolare
int analizeaza_fisier(char* fisier_verificat, const char* dir_izolare) {
    int status = system("./verify_for_malicious.sh");

    if (WIFEXITED(status) && WEXITSTATUS(status) == 1) {
        izoleaza_fisier(fisier_verificat, dir_izolare);
        return 1;  // Fișierul este suspect
    }
    
    return 0;  // Fișierul este curat
}




