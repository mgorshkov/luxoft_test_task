//
//  Main.cpp
//  luxoft_test_task
//
//  Created by Mikhail Gorshkov on 15.11.2021.
//

#include <iostream>
#include <string.h>

static const char* HELP = "luxoft_test_task increment <num_threads> <max_increment>\nluxoft_test_task knights <num_threads>";

extern void increment(int, int);
extern void knights(int);

int main(int argc, const char* argv[]) {
    if (argc < 2) {
        std::cerr << HELP << std::endl;
        return 1;
    }
    
    if (!strcmp("increment", argv[1])) {
        if (argc < 3) {
            std::cerr << HELP << std::endl;
            return 1;
        }
        int num_threads = atoi(argv[2]);
        int max_value = atoi(argv[3]);
        increment(num_threads, max_value);
    } else if (!strcmp("knights", argv[1])) {
        int num_threads = atoi(argv[2]);
        knights(num_threads);
    }
    return 0;
}
