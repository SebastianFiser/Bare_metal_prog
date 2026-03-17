typedef unsigned long usize;
typedef long isize;

static inline isize sys_write( int fd, const void *buf, usize len ) {
    isize ret;
    __asm__ volatile (
        "syscall"
        :"=a"(ret)
        :"a"(1), "D"(fd), "S"(buf), "d"(len)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline isize sys_read( int fd, void *buf, usize len) {
    isize ret;
    __asm__ volatile (
        "syscall"
        :"=a"(ret)
        :"a"(0), "D"(fd), "S"(buf), "d"(len)
        : "rcx", "r11", "memory"
    );
    return ret;
}


__attribute__((noreturn))
static inline void sys_exit(int code) {
    __asm__ volatile (
        "syscall"
        :
        : "a" (60), "D" (code)
        : "rcx", "r11", "memory"
    );
     __builtin_unreachable();
}

void _start(void) {
    const char msg[] = "Vlozte vase jmeno (max 64 znaku)\n";
    sys_write(1, msg, sizeof(msg) - 1);
    char buffer[64];
    isize n = sys_read(0, buffer, 64);
    const char msg2[] = "Vase jmeno je : ";
    sys_write(1, msg2, sizeof(msg2) - 1);
    sys_write(1, buffer, n);
    sys_exit(0);
}