void m5_init();
void simBeginRegionOfInterest();
void simEndRegionOfInterest();
void simSetLogBase(void *ptr);
void simWorkBegin();
void simWorkEnd();
void simBarrierBegin();
void simBarrierEnd();
void simBackoffBegin();
void simBackoffEnd();
void simCodeRegionBegin(unsigned long int codeRegionId);
void simCodeRegionEnd(unsigned long int codeRegionId);

#if defined AARCH64


#define simEndLogUnroll(ptr) ({                                         \
            __asm__ volatile ("mov    x1, %0\n\t"                       \
                              "movz x0, #0xcafe \n\t"                    \
                              "movk x0, #0xbaad, lsl 16 \n\t"            \
                              "movk x0, #0xc0de, lsl 32 \n\t"            \
                              "movk x0, #0xdead, lsl 48 \n\t"            \
                              "str    x0, [x1]\n\t"                     \
                              :                                         \
                              : "r"(ptr)                                \
                              : "x0", "x1");                            \
        })

#elif defined X86

#define simEndLogUnroll(ptr) ({                                         \
            __asm__ volatile ("mov    %0,%%rdi\n\t"                     \
                              "movabs $0xdeadc0debaadcafe,%%rax\n\t"    \
                              "mov    %%rax,(%%rdi)\n\t"                \
                              :                                         \
                              : "r"(ptr)                                \
                              : "%rdi", "rax");                         \
        })
#endif
