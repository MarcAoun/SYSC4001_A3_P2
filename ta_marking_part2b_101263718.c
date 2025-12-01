#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <time.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>

#define NUM_QUESTIONS 5
#define RUBRIC_FILE "rubric.txt"
#define MAX_EXAMS 200

typedef struct {
    int question;
    char letter;
} RubricEntry;

typedef struct {
    int student_id;
    int marked[NUM_QUESTIONS];
} Exam;

typedef struct {
    RubricEntry rubric[NUM_QUESTIONS];
    Exam exam;
    char exam_files[MAX_EXAMS][256];
    int exam_count;
    int current_exam;
} SharedData;

double rng(double a, double b) {
    return a + ((double) rand() / RAND_MAX) * (b - a);
}

void load_rubric(SharedData *sh) {
    FILE *f = fopen(RUBRIC_FILE, "r");
    if (!f) { perror("rubric fopen"); exit(1); }
    for (int i = 0; i < NUM_QUESTIONS; i++) {
        int q; char comma; char letter;
        fscanf(f, "%d %c %c", &q, &comma, &letter);
        sh->rubric[i].question = q;
        sh->rubric[i].letter = letter;
    }
    fclose(f);
}

void save_rubric(SharedData *sh) {
    FILE *f = fopen(RUBRIC_FILE, "w");
    if (!f) return;
    for (int i = 0; i < NUM_QUESTIONS; i++)
        fprintf(f, "%d, %c\n", sh->rubric[i].question, sh->rubric[i].letter);
    fclose(f);
}

void load_exam(SharedData *sh, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) { perror("exam fopen"); exit(1); }
    fscanf(f, "%d", &sh->exam.student_id);
    fclose(f);
    for (int i = 0; i < NUM_QUESTIONS; i++)
        sh->exam.marked[i] = 0;
}

void ta_review(SharedData *sh, int tid, sem_t *rubric_lock) {
    for (int i = 0; i < NUM_QUESTIONS; i++) {
        printf("[TA %d] Checking rubric Q%d (%c)\n",
               tid, sh->rubric[i].question, sh->rubric[i].letter);

        usleep((useconds_t)(rng(0.5, 1.0) * 1e6));

        if (rand() % 4 == 0) {
            sem_wait(rubric_lock);

            char old = sh->rubric[i].letter;
            sh->rubric[i].letter = old + 1;

            printf("[TA %d] CORRECTED rubric Q%d %c -> %c\n",
                   tid, sh->rubric[i].question, old, sh->rubric[i].letter);

            save_rubric(sh);

            sem_post(rubric_lock);
        }
    }
}

void ta_mark_questions(SharedData *sh, int tid, sem_t *exam_lock) {
    while (1) {
        int q = -1;

        sem_wait(exam_lock);
        for (int i = 0; i < NUM_QUESTIONS; i++) {
            if (sh->exam.marked[i] == 0) {
                sh->exam.marked[i] = 1;
                q = i;
                break;
            }
        }
        sem_post(exam_lock);

        if (q == -1)
            return;

        printf("[TA %d] MARKING Student %04d Q%d\n",
               tid, sh->exam.student_id, q + 1);

        usleep((useconds_t)(rng(1.0, 2.0) * 1e6));

        printf("[TA %d] DONE Student %04d Q%d\n",
               tid, sh->exam.student_id, q + 1);
    }
}

int main(int argc, char *argv[]) {

    if (argc < 2) {
        printf("usage: %s <numTAs>\n", argv[0]);
        return 1;
    }

    int n = atoi(argv[1]);
    if (n < 2) {
        printf("Need >= 2 TAs\n");
        return 1;
    }

    sem_unlink("/rubric_lock");
    sem_unlink("/exam_lock");
    sem_unlink("/load_lock");

    sem_t *rubric_lock = sem_open("/rubric_lock", O_CREAT, 0666, 1);
    sem_t *exam_lock = sem_open("/exam_lock", O_CREAT, 0666, 1);
    sem_t *load_lock = sem_open("/load_lock", O_CREAT, 0666, 1);

    key_t key = ftok(RUBRIC_FILE, 65);
    int shmid = shmget(key, sizeof(SharedData), 0666 | IPC_CREAT);
    SharedData *sh = (SharedData*) shmat(shmid, NULL, 0);

    load_rubric(sh);

    FILE *list = popen("ls exams", "r");
    sh->exam_count = 0;
    while (fscanf(list, "%s", sh->exam_files[sh->exam_count]) == 1)
        sh->exam_count++;
    pclose(list);

    sh->current_exam = 0;

    char start[256];
    sprintf(start, "exams/%s", sh->exam_files[0]);
    load_exam(sh, start);

    for (int i = 0; i < n; i++) {
        if (fork() == 0) {
            int tid = getpid();
            srand(time(NULL) ^ tid);

            while (1) {
                ta_review(sh, tid, rubric_lock);
                ta_mark_questions(sh, tid, exam_lock);

                sem_wait(load_lock);

                sh->current_exam++;
                if (sh->current_exam >= sh->exam_count) {
                    sem_post(load_lock);
                    break;
                }

                char next_path[256];
                sprintf(next_path, "exams/%s", sh->exam_files[sh->current_exam]);

                load_exam(sh, next_path);
                printf("[TA %d] Loaded next exam: %s\n", tid, next_path);

                if (sh->exam.student_id == 9999) {
                    sem_post(load_lock);
                    break;
                }

                sem_post(load_lock);
            }

            shmdt(sh);
            exit(0);
        }
    }

    for (int i = 0; i < n; i++)
        wait(NULL);

    shmdt(sh);
    shmctl(shmid, IPC_RMID, NULL);

    sem_close(rubric_lock);
    sem_close(exam_lock);
    sem_close(load_lock);

    sem_unlink("/rubric_lock");
    sem_unlink("/exam_lock");
    sem_unlink("/load_lock");

    printf("[Parent] All TAs finished.\n");
    return 0;
}
