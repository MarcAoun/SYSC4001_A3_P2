#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <string.h>

#define NUM_QUESTIONS 5
#define RUBRIC_FILE   "rubric.txt"
#define FIRST_EXAM    "exams/exam_0001.txt"

typedef struct {
    int  question;
    char letter;
} RubricEntry;

typedef struct {
    int student_id;
    int question_marked[NUM_QUESTIONS];
} SharedExam;

typedef struct {
    RubricEntry rubric[NUM_QUESTIONS];
    SharedExam  exam;
} SharedData;

void load_rubric(SharedData *shared) {
    FILE *f = fopen(RUBRIC_FILE, "r");
    if (!f) { perror("rubric fopen"); exit(1); }

    for (int i = 0; i < NUM_QUESTIONS; i++) {
        int q; char comma; char letter;
        fscanf(f, "%d %c %c", &q, &comma, &letter);
        shared->rubric[i].question = q;
        shared->rubric[i].letter = letter;
    }
    fclose(f);
}

void save_rubric(SharedData *shared) {
    FILE *f = fopen(RUBRIC_FILE, "w");
    if (!f) return;

    for (int i = 0; i < NUM_QUESTIONS; i++)
        fprintf(f, "%d, %c\n", shared->rubric[i].question, shared->rubric[i].letter);

    fclose(f);
}

void load_exam(SharedData *shared, const char *file) {
    FILE *f = fopen(file, "r");
    if (!f) { perror("exam fopen"); exit(1); }

    fscanf(f, "%d", &shared->exam.student_id);
    fclose(f);

    for (int i = 0; i < NUM_QUESTIONS; i++)
        shared->exam.question_marked[i] = 0;
}

double rng(double a, double b) {
    double r = (double)rand() / RAND_MAX;
    return a + r * (b - a);
}

void ta_review(SharedData *shared, int ta_id) {
    for (int i = 0; i < NUM_QUESTIONS; i++) {
        printf("[TA %d] Reading Q%d (%c)\n",
               ta_id, shared->rubric[i].question, shared->rubric[i].letter);

        usleep((useconds_t)(rng(0.5, 1.0) * 1e6));

        if (rand() % 4 == 0) {
            char old = shared->rubric[i].letter;
            shared->rubric[i].letter = old + 1;
            printf("[TA %d] Corrected Q%d %c -> %c\n",
                   ta_id, shared->rubric[i].question, old, shared->rubric[i].letter);
            save_rubric(shared);
        }
    }
}

void ta_mark(SharedData *shared, int ta_id) {
    int qi = -1;

    for (int i = 0; i < NUM_QUESTIONS; i++)
        if (!shared->exam.question_marked[i]) { qi = i; shared->exam.question_marked[i] = 1; break; }

    if (qi == -1) {
        printf("[TA %d] Nothing left to mark\n", ta_id);
        return;
    }

    printf("[TA %d] Marking student %04d Q%d\n",
           ta_id, shared->exam.student_id, qi + 1);

    usleep((useconds_t)(rng(1.0, 2.0) * 1e6));

    printf("[TA %d] Finished student %04d Q%d\n",
           ta_id, shared->exam.student_id, qi + 1);
}

int main(int argc, char *argv[]) {
    if (argc < 2) { printf("usage: %s <numTAs>\n", argv[0]); return 1; }

    int n = atoi(argv[1]);
    if (n < 2) { printf("need >= 2\n"); return 1; }

    key_t key = ftok(RUBRIC_FILE, 65);
    int shmid = shmget(key, sizeof(SharedData), 0666 | IPC_CREAT);
    SharedData *shared = shmat(shmid, NULL, 0);

    load_rubric(shared);
    load_exam(shared, FIRST_EXAM);

    printf("[Parent] Loaded exam for %04d\n", shared->exam.student_id);

    for (int i = 0; i < n; i++) {
        if (fork() == 0) {
            int tid = getpid();
            srand(time(NULL) ^ tid);
            printf("[TA %d] Started\n", tid);
            ta_review(shared, tid);
            ta_mark(shared, tid);
            printf("[TA %d] Done\n", tid);
            shmdt(shared);
            exit(0);
        }
    }

    for (int i = 0; i < n; i++) wait(NULL);

    shmdt(shared);
    shmctl(shmid, IPC_RMID, NULL);

    printf("[Parent] All done\n");
    return 0;
}
