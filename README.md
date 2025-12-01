This project contains the implementation for Part 2 of the assignment.
There are two programs:

Part 2A
Uses shared memory and multiple TA processes. No synchronization. Race conditions are allowed.

Part 2B
Uses shared memory and semaphores. Synchronization is used to control access to the rubric, question selection, and loading the next exam.

The project also includes a rubric file and a folder of exam files.

Project files:
ta_marking_part2a_101263718.c
ta_marking_part2b_101263718.c
rubric.txt
exams folder containing exam_0001.txt to exam_0020.txt and exam_9999.txt
reportPartC.pdf
README

How to compile:

Compile Part 2A:
gcc -o part2a ta_marking_part2a_101263718.c -lrt

Compile Part 2B:
gcc -o part2b ta_marking_part2b_101263718.c -lrt -pthread

Some systems may not require -lrt. If needed, simply use:
gcc -o part2b ta_marking_part2b_101263718.c -pthread

How to run:

Both programs require one argument. This argument is the number of TA processes.

Example run for Part 2A with two TAs:
./part2a 2

Example run for Part 2B with three TAs:
./part2b 3

Test cases:

Test case 1
Run Part 2A with two TAs:
./part2a 2
Expected behavior: TAs read the rubric, mark questions, and load each exam. Race conditions may appear. This is normal.

Test case 2
Run Part 2B with three TAs:
./part2b 3
Expected behavior: Semaphores prevent race conditions. Program behaves consistently.

Test case 3
Termination test:
Make sure there is a file named exam_9999.txt containing the line
9999
Run:
./part2b 4
The TAs stop when exam 9999 is reached.

Notes:
Part 2A is allowed to show race conditions.
Part 2B should not show race conditions if the semaphores are used correctly.
All TA actions print to the terminal so you can observe program behavior.
reportPartC.pdf contains the writeup for Part 2C.
