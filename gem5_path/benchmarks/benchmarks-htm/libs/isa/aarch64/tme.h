// TME instructions

// tstart x2 d5233062
#define M5_TSTART(xid,addr) ({                          \
            __asm__ volatile (".long 0xd5233062 \n"     \
                              );                        \
        })

// tcommit  d503307f
#define M5_TCOMMIT(xid) ({                              \
            __asm__ volatile (".long 0xd503307f \n"     \
                              );                        \
        })

// tcancel #65535 d47fffe0
#define M5_TCANCEL(xid) ({                              \
            __asm__ volatile (".long 0xd47fffe0 \n"     \
                              );                        \
        })

