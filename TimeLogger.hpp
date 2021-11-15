//
//  TimeLogger.hpp
//  luxoft_test_task
//
//  Created by Mikhail Gorshkov on 14.09.2021.
//

#ifndef TimeLogger_h
#define TimeLogger_h

#include <iostream>
#include <chrono>

struct TimeLogger
{
    const char* m_blockName;
    const std::chrono::time_point<std::chrono::high_resolution_clock> m_start;

    TimeLogger(const char* blockName = "main")
        : m_blockName(blockName)
        , m_start(std::chrono::high_resolution_clock::now())
    {
    }
    
    ~TimeLogger()
    {
        auto finish = std::chrono::high_resolution_clock::now();
        auto diff = finish - m_start;
        std::cout << m_blockName << ": " << std::chrono::duration_cast<std::chrono::microseconds>(diff).count()
              << " micros passed" << std::endl;
    }
};

#endif /* TimeLogger_h */
