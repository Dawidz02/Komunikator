#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <math.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>

#define BABBLE_NAME "/1234"
#define BABBLE_MODE 0777
#define BABBLE_LIMIT 10
#define BABBLE_LENGTH 80

struct babblespace
{
    pthread_mutex_t babble_mutex;
    pthread_cond_t babble_cond;
    int babble_first, babble_total;
    char babbles[BABBLE_LIMIT][BABBLE_LENGTH];
};

void wyswietl_komunikat(struct babblespace *ptr)
{
    /* Zabezpieczenie przed bledami przy zmianie wartosci BABBLE_LIMIT */
    if (ptr->babble_total > BABBLE_LIMIT)
    {
        ptr->babble_total = BABBLE_LIMIT;
    }

    printf("Istniejace komunikaty: %d   %d\n", ptr->babble_total, ptr->babble_first);

    for (int i = 0; i < ptr->babble_total; i++)
    {
        printf("%d.) %s\n", i, ptr->babbles[(ptr->babble_first + i) % BABBLE_LIMIT]);
    }
}

void napisz_komunikat(struct babblespace *ptr)
{
    char nowy_kom[BABBLE_LENGTH];
    printf("Wprowadz swoj komunikat: ");

    /* Pobranie nowego komunikatu ze standardowego wejscia. */
    fgets(nowy_kom, BABBLE_LENGTH, stdin);

    /* Oproznienie bufora wejsciowego z nadmiarowych znakow. */
    if (strlen(nowy_kom) > 0 && nowy_kom[strlen(nowy_kom) - 1] != '\n')
    {
        int c;
        while ((c = getchar()) != '\n' && c != EOF);
    }

    int dlugosc = strlen(nowy_kom);
    if (nowy_kom[dlugosc - 1] == '\n')
    {
        nowy_kom[dlugosc - 1] = '\0';
    }

    /* Zablokowanie muteksu. */
    pthread_mutex_lock(&(ptr->babble_mutex));

    /* Skopiowanie nowego komunikatu do tablicy. */
    if (ptr->babble_total < BABBLE_LIMIT)
    {
        strncpy(ptr->babbles[(ptr->babble_total) % BABBLE_LIMIT], nowy_kom, BABBLE_LENGTH);
    }
    if (ptr->babble_total == BABBLE_LIMIT)
    {
        strncpy(ptr->babbles[(ptr->babble_first) % BABBLE_LIMIT], nowy_kom, BABBLE_LENGTH);
    }
    /* Zaktualizowanie danych babble_first i  babble_total. */
    if (ptr->babble_total < BABBLE_LIMIT)
    {
        ptr->babble_total++;
        ptr->babble_first = 0;
    }
    else
    {
        ptr->babble_first = (ptr->babble_first + 1) % BABBLE_LIMIT;
    }
    /* Zwolnienie muteksu. */
    pthread_mutex_unlock(&(ptr->babble_mutex));
    pthread_cond_broadcast(&(ptr->babble_cond));
}

void potomek(struct babblespace *ptr)
{
    while (1)
    {
        pthread_mutex_lock(&(ptr->babble_mutex));
        pthread_cond_wait(&(ptr->babble_cond), &(ptr->babble_mutex));
        wyswietl_komunikat(ptr);
        pthread_mutex_unlock(&(ptr->babble_mutex));      
    }
}

int main()
{
    int shm = shm_open(BABBLE_NAME, O_RDWR, S_IRUSR | S_IWUSR);
    if (shm == -1)
    {
        printf("Nie uzyskano dostepu do wspolnej pamieci. \n");
        return -1;
    }

    struct babblespace *pamiec = mmap(NULL, sizeof(struct babblespace), PROT_READ | PROT_WRITE, MAP_SHARED, shm, 0);
    if (pamiec == MAP_FAILED)
    {
        printf("Blad przy mapowaniu pamieci. \n");
        return -2;
    }

    /* Tworzenie procesu */
    int id = fork();
    if (id == -1)
    {
        printf("Blad przy wywolaniu funkcji fork().\n");
        return -4;
    }

    if (id == 0)
    {
        potomek(pamiec);
        exit(EXIT_SUCCESS); // Wyjście z pętli dla procesu potomnego
    }
    else
    {
        char wybor;
        do
        {
            napisz_komunikat(pamiec);
            printf("Czy chcesz wprowadzic kolejny komunikat? Jesli tak wpisz 't'\n");
            wybor = getchar();
            getchar();
        } while (wybor == 'T' || wybor == 't');
        printf("Ostatnie\n");
    }
    // Czekanie na zakończenie potomka
    munmap(pamiec, sizeof(struct babblespace));
    kill(id, SIGTERM); 
    waitpid(id, NULL, 0);
    close(shm);
    return 0;
}
