#ifndef IO_H
#define IO_H

#define inb(port) ({ \
    unsigned char _r; \
    __asm__ volatile ("inb %w1, %b0" : "=a"(_r) : "Nd"(port)); \
    _r; \
})

#define outb(port, val) \
    __asm__ volatile ("outb %b0, %w1" : : "a"(val), "Nd"(port))

#define inw(port) ({ \
    unsigned short _r; \
    __asm__ volatile ("inw %w1, %w0" : "=a"(_r) : "Nd"(port)); \
    _r; \
})

#define outw(port, val) \
    __asm__ volatile ("outw %w0, %w1" : : "a"(val), "Nd"(port))

#endif
