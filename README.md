# Portable-C89-printf
Portable C89 printf for embedded computers.

Feel free to take any code. I wrote this so you do not have too..

Benefits:
- Strictly C89 compliant (only uses the headers defined by a conforming freestanding implementation <stdarg.h>, <stddef.h>, and <limits.h>).
- Does not rely on implementation defined or undefined behaviour.
- Does not rely on a C library (except uses <string.h> for the strlen() function, which can easily be replaced).
- Will work for any value of CHAR_BIT (for values such as 6, 8, 16, etc). C89 specifies CHAR_BIT is at least 8 but this is not a requirement for this printf implementation.

Drawbacks:
- Does not have floating point functionality.

Extra:
- %p prints all bytes including 0s because it is a memory location and all digits are significant.
