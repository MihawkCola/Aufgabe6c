#include <stdio.h>
#include "web_request.c"

//Hier sind Quellen die uns geholfen haben
//Quelle: https://stackoverflow.com/questions/1352749/multiple-arguments-to-function-called-by-pthread-create
//Quelle: http://www.c-howto.de/tutorial/arrays-felder/speicherverwaltung/
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>

#define QUEUESIZE 10


void *producer (void *q);
void *consumer (void *q);
char *datei;
char *parameter[1];
int j= 0;

typedef struct {
    char *buf[QUEUESIZE];
    long head, tail;
    int full, empty;
    pthread_mutex_t *mut;
    pthread_cond_t *notFull, *notEmpty,*start;
    bool eof;// haben wir erweitert um zu checken ob das lesen vorbei ist
} queue;
pthread_cond_t *conditions;

typedef struct {
    queue *q;
    int id;
} consum;

queue *queueInit (void);
void queueDelete (queue *q);
void queueAdd (queue *q, char *in);
char* queueDel (queue *q);

int main (int argc, char *argv[])
{
    datei= argv[1];
    struct timeval  go, end;
    parameter[0] = "--webreq-delay 0";

    queue *fifo;
    pthread_t pro;
    pthread_attr_t attr;

    fifo = queueInit ();
    if (fifo ==  NULL) {
        fprintf (stderr, "main: Queue Init failed.\n");
        exit (1);
    }
    pthread_attr_init(&attr);
    int threads=0;
    printf("Wie viel Client-Threads mächten Sie?\n");
    scanf("%d",&threads);
 //   threads = 5;

    pthread_t con[threads];
    consum *c ;

    for (int i = 0; i < threads; ++i) {
        c = (consum *)malloc(sizeof(consum));
        c->id= i;
        c->q = fifo;
        pthread_create (&con[i], NULL, consumer, c);
    }
    gettimeofday(&go, NULL);
    pthread_create (&pro, &attr, producer, fifo);
    pthread_join (pro, NULL);
    for (int i = 0; i < threads; ++i) {
        pthread_join (con[i], NULL);
    }
    gettimeofday(&end, NULL);
    printf("%lu ms",(end.tv_sec-go.tv_sec) * 1000 + (end.tv_usec-go.tv_usec) / 1000);
    queueDelete (fifo);

    return 0;
}


void *producer (void *q)
{
    queue *fifo;
    char *d;
    fifo = (queue *)q;

    FILE *file = fopen(datei,"r");
    if(file == NULL){
        perror("fail");
        exit(EXIT_FAILURE);
    }
    char line[256];
    while(fgets(line, sizeof(line),file)){
        line[strcspn(line,"\n")]= '\0';


        pthread_mutex_lock (fifo->mut);
        // while -> spinlock
        if (fifo->full) {
       //    printf ("producer: queue FULL.\n");
           pthread_cond_wait (fifo->notFull, fifo->mut);

        }
        queueAdd (fifo, line);

   //   printf("producer: produced %s.\n",line);

        pthread_mutex_unlock (fifo->mut);
        pthread_cond_signal (fifo->notEmpty);
    }
    fifo->eof=true;
  // printf ("END OF PRODUCER.\n");
    pthread_cond_broadcast(fifo->notEmpty);
    fclose(file);
    return (NULL);
}

void *consumer (void *q)
{
    queue *fifo;
    char *url;
    char filename[64];
    consum *con = (consum*)q;
    int id =con->id;
    webreq_init(1,parameter);
    fifo = con->q;
    //pthread_cond_signal(fifo->start);
    while (!fifo->empty || !fifo->eof){
        pthread_mutex_lock (fifo->mut);
        //pthread_cond_wait (fifo->notEmpty, fifo->mut);
        // while -> spinlock
        if(fifo->empty && !fifo->eof) {
            printf ("consumer: queue EMPTY.\n");
            pthread_cond_wait (fifo->notEmpty, fifo->mut);
            //time_t t;
             //struct timeval start;
            //time(t);
            //gettimeofday(&sta,NULL);
         //   clock_gettime(CLOCK_REALTIME,ts);

        //    if(fifo->empty){

        //    }

        }
        if(!fifo->empty) {
            url = strdup(queueDel(fifo));
            j++;
            pthread_mutex_unlock(fifo->mut);
            pthread_cond_signal (fifo->notFull);
            char *downloadUrl = strdup(url);
            strtok(url,"/");
            char *domain =strtok(NULL,"/");

            snprintf(filename, sizeof (filename), "%d_%d_%s.html", j,id, domain);

            printf("[START] Downloading URL: %s ->> File: %s\n", url, filename);
            int res = webreq_download(url, filename);
            if (res < 0)
                fprintf(stderr, "[ERROR] URL: %s, Message: %s\n", url, webreq_error(res));
            else if (res != WEBREQ_HTTP_OK)
                fprintf(stderr, "[ERROR] HTTP Status %d returned for URL: %s\n", res, url);
            else
                printf("[DONE ] URL: %s ->> File: %s\n", url, filename);
         /*    if (webreq_download(url, filename) < 0) {
                  fprintf(stderr, "Bei der URL %s gabes probleme mit dem Download versuche über proxy\n", url);
              } else if(webreq_download_via_proxy(url, filename)<0){
                  fprintf(stderr, "Die url %s konnte nicht gedownload werden\n", url);
              }*/
            if (fifo->eof) {
                pthread_mutex_unlock(fifo->mut);
                pthread_cond_broadcast(fifo->notEmpty);
            }
        }else if(fifo->eof){
            pthread_mutex_unlock(fifo->mut);
            pthread_cond_broadcast(fifo->notEmpty);
        }
    }
 //   printf ("END OF CONSUMER %d.\n", id);
    return (NULL);
}

#if 0
typedef struct {
	int buf[QUEUESIZE];
	long head, tail;
	int full, empty;
	pthread_mutex_t *mut;
	pthread_cond_t *notFull, *notEmpty;
} queue;
#endif

queue *queueInit (void)
{
    queue *q;

    q = (queue *)malloc (sizeof (queue));
    if (q == NULL) return (NULL);
    q->empty = 1;
    q->full = 0;
    q->head = 0;
    q->tail = 0;
    q->eof =false;
    q->mut = (pthread_mutex_t *) malloc (sizeof (pthread_mutex_t));
    pthread_mutex_init (q->mut, NULL);
    q->notFull = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
    pthread_cond_init (q->notFull, NULL);
    q->notEmpty = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
    pthread_cond_init (q->notEmpty, NULL);
    q->start = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
    pthread_cond_init (q->notEmpty, NULL);

    return (q);
}

void queueDelete (queue *q)
{
    pthread_mutex_destroy (q->mut);
    free (q->mut);
    pthread_cond_destroy (q->notFull);
    free (q->notFull);
    pthread_cond_destroy (q->notEmpty);
    free (q->notEmpty);
    free (q);
}

void queueAdd (queue *q, char *in)
{
    q->buf[q->tail] = strdup(in);
    q->tail++;
    if (q->tail == QUEUESIZE)
        q->tail = 0;
    if (q->tail == q->head)
        q->full = 1;
    q->empty = 0;

    return;
}

char* queueDel (queue *q)
{
     char *out = q->buf[q->head];
    q->head++;
    if (q->head == QUEUESIZE)
        q->head = 0;
    if (q->head == q->tail)
        q->empty = 1;
    q->full = 0;
    return out;

}
