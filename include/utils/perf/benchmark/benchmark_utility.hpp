#pragma once

inline uint64_t rdtsc_start()
{
    unsigned int hi, lo;
    __asm__ volatile(
        "cpuid\n\t"
        "rdtsc\n\t"
        : "=a"(lo), "=d"(hi)
        : "a"(0)
        : "ebx", "ecx");
    return ((uint64_t)hi << 32) | lo;
}

inline uint64_t rdtsc_end()
{
    unsigned int hi, lo;
    __asm__ volatile(
        "rdtscp\n\t"
        "mov %%edx, %0\n\t"
        "mov %%eax, %1\n\t"
        "cpuid\n\t"
        : "=r"(hi), "=r"(lo)
        :
        : "rax", "rbx", "rcx", "rdx");
    return ((uint64_t)hi << 32) | lo;
}
