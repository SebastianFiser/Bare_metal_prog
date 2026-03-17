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

void _start(void) {
    //const char msg[] = "Vlozte vase jmeno (max 64 znaku)\n";
    //sys_write(1, msg, sizeof(msg) - 1);
    //char buffer[64];
    //isize n = sys_read(0, buffer, 64);
    //const char msg2[] = "Vase jmeno je : ";
    //sys_write(1, msg2, sizeof(msg2) - 1);
    //sys_write(1, buffer, n);
    isize fd = sys_open("/home/sebastian/Bare_metal_prog/nolib_projects/text.txt", 0, 0);
    if (fd < 0) sys_exit(1);
    char buffer[1024];
    isize n = sys_read(fd, buffer, 1024);
    sys_write(1, buffer, n);
    sys_exit(0);
}