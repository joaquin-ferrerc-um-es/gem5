// TSX instructions

/* NOTE: Need to set eax to -1 before executing xbegin (following what
   _xbegin() intrinsic does), otherwise the comparison against
   XBEGIN_STARTED is always false!
*/
#define M5_XBEGIN(xid,addr) ({                                  \
            uint64_t ret;                                      \
            __asm__ volatile ("mov %1, %%rdi\n\t"               \
                              "mov %2, %%rsi\n\t"               \
                              "mov $0xffffffff,%%eax\n\t"       \
                              "xbegin   .+6 \n\t"               \
                              "mov %%rax, %0\n\t"               \
                              : "=r"(ret)                       \
                              : "r"(xid), "r"(addr)             \
                              : "%rdi", "%rsi", "rax");         \
            ret;                                                \
        })

#define M5_XBEGIN_MFENCE(xid,addr) ({                   \
            uint64_t ret;                              \
            __asm__ volatile ("mov %1, %%rdi\n\t"       \
                              "mov %2, %%rsi\n\t"       \
                              "mfence       \n\t"       \
                              "xbegin   .+6 \n\t"       \
                              "mov %%rax, %0\n\t"       \
                              : "=r"(ret)               \
                              : "r"(xid), "r"(addr)     \
                              : "%rdi", "%rsi", "rax"); \
            ret;                                        \
        })

#define M5_XEND(xid) ({                                 \
            __asm__ volatile ("mov %0,%%rdi\n\t"        \
                              "xend\n\t"                \
                              :                         \
                              : "r"(xid)                \
                              : "%rdi","%rax");         \
        })

#define M5_XTEST() ({                                   \
            uint64_t ret;                              \
            __asm__ volatile ("xtest\n\t"               \
                              "mov %%rax, %0\n\t"       \
                              : "=r"(ret)               \
                              :                         \
                              :"%rax");                 \
            ret;                                        \
        })

#define M5_TRY_XEND(xid) ({                             \
            uint64_t ret;                              \
            __asm__ volatile ("mov %1,%%rdi\n\t"        \
                              "xend\n\t"                \
                              "mov %%rax, %0\n\t"       \
                              : "=r"(ret)               \
                              : "r"(xid)                \
                              : "%rdi","%rax");         \
            ret;                                        \
        })

#define M5_XARBITRATE() ({                              \
            uint64_t ret;                              \
            __asm__ volatile (".byte 0x0F, 0x04\n\t"    \
                              ".word 0x17\n\t"          \
                              "mov %%rax, %0\n\t"       \
                              : "=r"(ret)               \
                              :                         \
                              : "%rax");                \
            ret;                                        \
        })

#define M5_XABORT(code) ({                              \
            __asm__ volatile ("mov %0,%%rdi\n\t"        \
                              "xabort $0x0\n\t"         \
                              :                         \
                              : "r"((uint64_t)code)    \
                              : "%rdi");                \
        })

// addr to be released from read set, addr==0 will clear the read set
#define XRELEASE(addr) ({                               \
            __asm__ volatile ("mov %0,%%rdi\n\t"        \
                              ".byte 0x0F, 0x04\n\t"    \
                              ".word 0x13\n\t"		\
                              :                         \
                              : "r"(addr)               \
                              : "%rdi");                \
        })

#define DAE_ACCESS( ) ({                                \
            __asm__ volatile (".byte 0x0F, 0x04\n\t"	\
                              ".word 0x14\n\t"		\
                              :				\
                              : );			\
        })


#define M5_XPRIV( ) ({                                  \
            __asm__ volatile (".byte 0x0F, 0x04\n\t"	\
                              ".word 0x15\n\t"		\
                              :				\
                              : );			\
        })

#define M5_XPUBL( ) ({                                  \
            __asm__ volatile (".byte 0x0F, 0x04\n\t"	\
                              ".word 0x16\n\t"		\
                              :				\
                              : );			\
        })


// | 4 bytes |  NOP DWORD ptr [EAX + 00H]     |  0F 1F 40 00H
#define NOP_4BYTES( ) ({                                \
            __asm__ volatile (".byte 0x0F, 0x1F\n\t"	\
                              ".byte 0x40, 0x00\n\t"    \
                              :				\
                              : );			\
        })

