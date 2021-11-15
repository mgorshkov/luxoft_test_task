//
//  Knights.cpp
//  luxoft_test_task
// 2. Knight's tour – Please improve on the knights tour problem by improving scalability. Feel free to start with any example that can be found online.
//  Created by Mikhail Gorshkov on 13.09.2021.
//
// Not scaled version:
// main: 447373 micros passed
// Program ended with exit code: 0
//
// Scaled version (2 threads):
// main: 188 micros passed
// Program ended with exit code: 0

// C++ program for Knight's tour problem using
// Warnsdorff's algorithm with several threads
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <future>
#include <vector>

#include "TimeLogger.hpp"

//#define DEBUG_LOG

#define SCALED

#define N 8

// Move pattern on basis of the change of
// x coordinates and y coordinates respectively
static int cx[N] = {1,1,2,2,-1,-1,-2,-2};
static int cy[N] = {2,-2,1,-1,2,-2,1,-1};

// function restricts the knight to remain within
// the 8x8 chessboard
bool limits(int x, int y)
{
    return ((x >= 0 && y >= 0) && (x < N && y < N));
}

/* Checks whether a square is valid and empty or not */
bool isempty(int a[], int x, int y)
{
    return (limits(x, y)) && (a[y*N+x] < 0);
}

/* Returns the number of empty squares adjacent
to (x, y) */
int getDegree(int a[], int x, int y)
{
    int count = 0;
    for (int i = 0; i < N; ++i)
        if (isempty(a, (x + cx[i]), (y + cy[i])))
            count++;

    return count;
}

// Picks next point using Warnsdorff's heuristic.
// Returns false if it is not possible to pick
// next point.
std::pair<int, int> nextMove(int a[], int x, int y)
{
    int min_deg_idx = -1, c, min_deg = (N+1), nx, ny;

    // Try all N adjacent of (*x, *y) starting
    // from a random adjacent. Find the adjacent
    // with minimum degree.
    int start = rand()%N;
    for (int count = 0; count < N; ++count)
    {
        int i = (start + count)%N;
        nx = x + cx[i];
        ny = y + cy[i];
        if ((isempty(a, nx, ny)) &&
        (c = getDegree(a, nx, ny)) < min_deg)
        {
            min_deg_idx = i;
            min_deg = c;
        }
    }
    // IF we could not find a next cell
    if (min_deg_idx == -1)
        return std::make_pair(-1, -1);

    // Store coordinates of next point
    nx = x + cx[min_deg_idx];
    ny = y + cy[min_deg_idx];

    // Mark next move
    a[ny*N + nx] = a[y*N + x]+1;

    return std::make_pair(nx, ny);
}

/* displays the chessboard with all the
legal knight's moves */
void print(int a[])
{
#ifdef DEBUG_LOG
    for (int i = 0; i < N; ++i)
    {
        for (int j = 0; j < N; ++j)
            printf("%d\t",a[j*N+i]);
        printf("\n");
    }
#endif
}

/* checks its neighbouring squares */
/* If the knight ends on a square that is one
knight's move from the beginning square,
then tour is closed */
bool neighbour(int x, int y, int xx, int yy)
{
    for (int i = 0; i < N; ++i)
        if (((x+cx[i]) == xx)&&((y + cy[i]) == yy))
            return true;

    return false;
}

/* Generates the legal moves using warnsdorff's
heuristics. Returns false if not possible */
bool findClosedTour()
{
    // Filling up the chessboard matrix with -1's
    int a[N*N];
    for (int i = 0; i< N*N; ++i)
        a[i] = -1;

    // Randome initial position
    int sx = rand()%N;
    int sy = rand()%N;

    // Current points are same as initial points
    int x = sx, y = sy;
    a[y*N+x] = 1; // Mark first move.

    // Keep picking next points using
    // Warnsdorff's heuristic
    for (int i = 0; i < N*N-1; ++i) {
        auto next = nextMove(a, x, y);
        if (next.first == -1 && next.second == -1) {
            return false;
        }
        x = next.first;
        y = next.second;
    }
    // Check if tour is closed (Can end
    // at starting point)
    if (!neighbour(x, y, sx, sy)) {
        return false;
    }

    print(a);
    return true;
}

void knights(int num_threads)
{
    TimeLogger logger;
 
    // make 1000 iterations of the algorithm to get more precise measurements
    for (int i = 0; i < 1000; ++i) {
        // To make sure that different random
        // initial positions are picked.
        srand(time(NULL));

#ifdef SCALED
        while (true) {
            std::vector<std::future<bool>> calcs;
            for (int i = 0; i < num_threads; ++i) {
                calcs.emplace_back(std::async(std::launch::async, &findClosedTour));
            }
            for (auto&& fut : calcs) {
                if (fut.get())
                    return;
            }
        }
#else
        // While we don't get a solution
        while (!findClosedTour())
        {
        ;
        }
#endif
    }
}

