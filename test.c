#include <stdio.h>

#if defined(__i386__)

static __inline__ unsigned long long rdtsc(void)
{
        unsigned long long int x;

        __asm__ __volatile__ (".byte 0x0f, 0x31" : "=A" (x));
        return x;
}

#elif defined(__x86_64__)

static __inline__ unsigned long long rdtsc(void)
{
        unsigned hi, lo;

        __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
        return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}

#endif


// Borrowed and slightly modifed from bitcoin's RNG
// https://github.com/bitcoin/bitcoin/blob/d9007f51a7480246abe4c16f2e3d190988470bec/src/random.cpp#L109
static unsigned long long  GetRdRand()
{
    // RdRand may very rarely fail. Invoke it up to 10 times in a loop to reduce this risk.
#ifdef __i386__
    unsigned char ok;
    // Initialize to 0 to silence a compiler warning that r1 or r2 may be used
    // uninitialized. Even if rdrand fails (!ok) it will set the output to 0,
    // but there is no way that the compiler could know that.
    unsigned integer r1 = 0, r2 = 0;
    for (int i = 0; i < 10; ++i) {
        __asm__ volatile (".byte 0x0f, 0xc7, 0xf0; setc %1" : "=a"(r1), "=q"(ok) :: "cc"); // rdrand %eax
        if (ok) break;
    }
    for (int i = 0; i < 10; ++i) {
        __asm__ volatile (".byte 0x0f, 0xc7, 0xf0; setc %1" : "=a"(r2), "=q"(ok) :: "cc"); // rdrand %eax
        if (ok) break;
    }
    return (((uint64_t)r2) << 32) | r1;
#elif defined(__x86_64__) || defined(__amd64__)
    unsigned char ok;
    unsigned long long  r1 = 0; // See above why we initialize to 0.
    for (int i = 0; i < 10; ++i) {
        __asm__ volatile (".byte 0x48, 0x0f, 0xc7, 0xf0; setc %1" : "=a"(r1), "=q"(ok) :: "cc"); // rdrand %rax
        if (ok) break;
    }
    return r1;
#else
#error "RdRand is only supported on x86 and x86_64"
#endif
}


int true = 1;

static unsigned long long GetRdSeed()
{
    // RdSeed may fail when the HW RNG is overloaded. Loop indefinitely until enough entropy is gathered,
    // but pause after every failure.
#ifdef __i386__
    unsigned char ok;
    unsigned integer r1, r2;
    do {
        __asm__ volatile (".byte 0x0f, 0xc7, 0xf8; setc %1" : "=a"(r1), "=q"(ok) :: "cc"); // rdseed %eax
        if (ok) break;
        __asm__ volatile ("pause");
    } while(true);
    do {
        __asm__ volatile (".byte 0x0f, 0xc7, 0xf8; setc %1" : "=a"(r2), "=q"(ok) :: "cc"); // rdseed %eax
        if (ok) break;
        __asm__ volatile ("pause");
    } while(true);
    return (((uint64_t)r2) << 32) | r1;
#elif defined(__x86_64__) || defined(__amd64__)
    unsigned char ok;
    unsigned long long r1;
    do {
        __asm__ volatile (".byte 0x48, 0x0f, 0xc7, 0xf8; setc %1" : "=a"(r1), "=q"(ok) :: "cc"); // rdseed %rax
        if (ok) break;
        __asm__ volatile ("pause");
    } while(true);
    return r1;
// MSVC support from https://github.com/bitcoin/bitcoin/pull/22158
#elif defined(_MSC_VER) && defined(_M_X64)
    uint64_t r1 = 0; // See above why we initialize to 0.
    do {
        uint8_t ok = _rdseed64_step(&r1);
        if (ok) break;
    } while(true);
    return r1;
#elif defined(_MSC_VER) && defined(_M_IX86)
    uint32_t r1 = 0; // See above why we initialize to 0.
    uint32_t r2 = 0;
    for (int i = 0; i < 10; ++i) {
        uint8_t ok = _rdseed32_step(&r1);
        if (ok) break;
    }
    for (int i = 0; i < 10; ++i) {
        int ok = _rdseed32_step(&r2);
        if (ok) break;
    }
    return (((uint64_t)r2) << 32) | r1;
#else
#error "RdSeed is only supported on x86 and x86_64"
#endif
}

int
main(void)
{
        long i;
        unsigned long long d;

        d = 0;
        for (i = 0; i < 1000000; i ++) {
                unsigned long long b, e;

                b = rdtsc();
                e = rdtsc();
                d += e - b;
        }
        printf("rdtsc average : %.3f\n", (double)d / 1000000.0);

        // rdrand
        int samples = 10;
        d = 0;
        for (i = 0; i < samples; i ++) {
                unsigned long long b, e;

                b = GetRdRand();
                printf("rdrand: 0x%llx\n", b);
                e = GetRdRand();
                d ^= e ^ b;
        }
        printf("rdrand xor average : %llx \n", d);

        // rdseed
        d = 0;
        for (i = 0; i < samples; i ++) {
                unsigned long long b, e;

                b = GetRdSeed();
                printf("rdseed: 0x%llx\n", b);
                e = GetRdSeed();
                d ^= e ^ b;
        }
        printf("rdseed xor average : %llx \n", d);


        return 0;
}
