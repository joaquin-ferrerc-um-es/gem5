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
            __asm__ volatile (""                                        \
                              :                                         \
                              :                                         \
                              :                                             ); \
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
