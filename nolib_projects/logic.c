typedef unsigned long usize;
typedef long isize;

__asm__(
    ".global _start\n"
    "_start:\n"
    "    mov %rsp, %rdi\n"
    "    call start_c\n"
);

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

static inline isize sys_open( const char *pathname, int flags, int mode ) {
    isize ret;
    __asm__ volatile (
        "syscall"
        :"=a"(ret)
        : "a"(2), "D"(pathname), "S"(flags), "d"(mode)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline isize sys_close(int fd) {
    isize ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(3), "D"(fd)
        : "rcx", "r11", "memory"
    );
    return ret;
}


__attribute__((noreturn))
static inline void sys_exit(int code) {
    if (code == 1) {
        const char err_msg[] = "ERROR program exited with code 1.\n";
        sys_write(1, err_msg, sizeof(err_msg) - 1);
    }
    else {
        const char ext_msg[] = " Program exited successfully\n ";
        sys_write( 1, ext_msg, sizeof(ext_msg) - 1);
    }
    __asm__ volatile (
        "syscall"
        :
        : "a" (60), "D" (code)
        : "rcx", "r11", "memory"
    );
    
     __builtin_unreachable();
}

static inline char *start_arg(usize *sp) {
    usize argc = *sp;
    char **argv = (char **)(sp + 1);
    if (argc < 2) {
        const char err[] = "ERROR: argc < 2\n";
        sys_write(1, err, sizeof(err) - 1);
        sys_exit(1);
    }
    return argv[1];
}

static inline isize sys_orp(const char *filepath) {
    isize fd = sys_open(filepath, 0, 0);
    if (fd < 0) {
        const char fail_msg[] = "ERROR: sys_open failed\n";
        sys_write(1, fail_msg, sizeof(fail_msg) - 1);
        sys_exit(1);
    }

    char fileread[1024];
    isize n = sys_read(fd, fileread, 1024);
    if (n > 0) sys_write(1, fileread, (usize)n);
    sys_close((int)fd);
    return n;
}

__attribute__((noreturn))
void start_c(usize *sp) {
    char *filename = start_arg(sp);
    sys_orp(filename);
    sys_exit(0);
}