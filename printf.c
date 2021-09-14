#include <stdarg.h> /* Provided by the compiler. */
#include <stddef.h> /* Provided by the compiler. */
#include <limits.h> /* Provided by the compiler. */

#include <string.h>

#include <stdio.h>
#undef putc

#include <unistd.h>

/* Octal will always produce the most amount of digits. */
#define NBPRINTBUF ((CHAR_BIT/3 + 1)*sizeof(union value))

#if CHAR_BIT < 10
#define NBPRINTBUFDEC ((2*CHAR_BIT/10 + 1)*sizeof(union value))
#else
#define NBPRINTBUFDEC ((CHAR_BIT/10 + 1)*sizeof(union value))
#endif

/* Length */
enum {INT, CHAR, SHORT, LONG, VOID, SIZE_T, PTRDIFF_T};

union value {
	unsigned int i;
	unsigned char hh;
	unsigned short h;
	unsigned long l;
	void *p;
	size_t z;
	ptrdiff_t t;
};

/* Base */
enum {STR, DEC, OCT, HEX}; 

struct flag {
	unsigned int base   : 2; /* hex, oct, dec, or str? */
	unsigned int len    : 3; /* hh, h, l, z, t, or p? */
	unsigned int zero   : 1; /* 0 flag set? */
	unsigned int plus   : 1; /* + flag set? */
	unsigned int nums   : 1; /* # flag set? */
	unsigned int minus  : 1; /* - flag set? */
	unsigned int low    : 1; /* Lowercase? */
	unsigned int wflag  : 1; /* Is width set? */
	unsigned int unsign : 1; /* Is it unsigned? */
	unsigned int sign   : 1; /* Is the value negative? */
	unsigned int width;
};

static const char *digit = "0123456789ABCDEF0123456789abcdef"; 
static const struct {
	unsigned int boff;
	unsigned int eoff;
} ltab[8] = {
	{offsetof(union value, i), sizeof(unsigned int)},
	{offsetof(union value, hh), sizeof(unsigned char)},
	{offsetof(union value, h), sizeof(unsigned short)},
	{offsetof(union value, l), sizeof(unsigned long)},
	{offsetof(union value, p), sizeof(void *)},
	{offsetof(union value, z), sizeof(size_t)},
	{offsetof(union value, t), sizeof(ptrdiff_t)}
};

/* Caluculations at compile time for remainder calculations. */
static const struct {
	unsigned int cnt;   /* How many fit into a byte. */
	unsigned int tshft; /* How much to shift down. */
	unsigned int cary;  /* How many bits to carry over. */
	unsigned int mask;
	unsigned int shft;
} calcs[] = {
	{CHAR_BIT/3, 3*(CHAR_BIT/3), CHAR_BIT - 3*(CHAR_BIT/3), 0x7, 3},
	{CHAR_BIT/4, 4*(CHAR_BIT/4), CHAR_BIT - 4*(CHAR_BIT/4), 0xf, 4}
};

static char *
printdec(char buf[NBPRINTBUFDEC + 1], union value *val, struct flag *flags)
{
	const unsigned char *srt = (const unsigned char *) val + ltab[flags->len].boff, *end;
	unsigned long dec = 0;
	unsigned char *tmp = (unsigned char *) &dec;

	/* Copy the value into a long so we can perform arithmetic on it. */
	for (end = srt + ltab[flags->len].eoff; srt < end; ++srt)
		*tmp++ = *srt;

	do {
		*buf++ = digit[dec % 10];
		dec /= 10;
	} while (dec);

	if (flags->sign)
		*buf++ = '-';

	return buf;
}

/* C89 compliant hex dump.. */
static char *
printhexoct(char buf[NBPRINTBUF], union value *val, struct flag *flags)
{
	unsigned char *srt = (unsigned char *) val + ltab[flags->len].boff, *end;
	unsigned int upper = flags->low << 4, base = flags->base == HEX;

	/* Ignore leading 0 bytes. */
	end = srt + ltab[flags->len].eoff;
	if (flags->len != VOID)
		for (; end > srt + 1; --end)
			if (*(end - 1))
				break;

	/* Print bytes up to sizeof(type) - 1. */
	while (end > srt + 1) {
		unsigned char *shft;
		unsigned int j;

		/* Print all digits that fit in a byte. */
		for (j = calcs[base].cnt; j; --j) {
			*buf++ = digit[upper + (*srt & calcs[base].mask)];
			*srt >>= calcs[base].shft;
		}

		/* Shift the remaining bytes down and in. */
		for (shft = srt; shft + 1 < end; ++shft) {
			*shft |= *(shft + 1) << calcs[base].cary;
			*(shft + 1) >>= calcs[base].tshft;
		}

		/* If the last byte is now zero we can ignore it. */
		if (!*(end - 1))
			--end;
	}

	/* Always print last byte. */
	do {
		*buf++ = digit[upper + (*srt & calcs[base].mask)];
		*srt >>= calcs[base].shft;
	} while (*srt);

	/* Print format of # flag. */
	if (flags->nums) {
		const char *nums = !base + flags->low ? "x0" : "X0";
		if (base || *(buf - 1) != '0')
			while (*nums)
				*buf++ = *nums++;
	}

	return buf;
}

static void
print(void (*putc) (int), void *val, struct flag *flags)
{
	char buf[NBPRINTBUF], *end, *srt;
	unsigned pad = 0;
	int incr = 0;

	switch (flags->base) {
	case STR:
		if (!(srt = val))
			srt = "(null)";

		end = srt + strlen(srt);
		break;
	case HEX:
	case OCT:
		end = printhexoct(srt = buf, val, flags);
		break;
	case DEC:
		end = printdec(srt = buf, val, flags);
		if (flags->sign && flags->plus)
			*end++ = '+';
	}

	/* Calculate padding. */
	if (flags->wflag)
		if (flags->width > end - srt)
			pad = flags->width - (end - srt);
		else if(!flags->base)
			end = srt + flags->width;
	flags->width = end - srt; /* Amount to print. */

	if (!flags->minus) {
		int pbyte = !flags->nums && flags->zero && flags->base != 0 ? '0' : ' ';
		for (; pad; --pad)
			putc(pbyte);
	}

	if (flags->base) {
		srt = end - 1;
		incr = -1;
	} else
		incr = 1;

	for (; flags->width; --flags->width, srt += incr)
		putc(*srt);

	if (flags->minus)
		for (; pad; --pad)
			putc(' ');
}

static void
kvprintf(void (*putc) (int), const char *fmt, va_list ap)
{
	static const union value vzero;
	static const struct flag fzero;
	for (; *fmt; ++fmt) {
		const char *str;
		union value val;
		struct flag flags;
		if (*fmt != '%') {
			putc(*fmt);
			continue;
		}

		val = vzero;
		flags = fzero;
	flag:
		switch (*++fmt) {
		case '0':
			flags.zero = 1;
			goto flag;
		case '+':
			flags.plus = 1;
			goto flag;
		case '#':
			flags.nums = 1;
			goto flag;
		case '-':
			flags.minus = 1;
			goto flag;
		case ' ':
			flags.zero = 0;
			goto flag;
		}

	width:
		switch (*fmt) {
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			flags.wflag = 1;
			flags.width = ((flags.width << 2) + flags.width << 1);
			flags.width += *fmt++ - '0';
			if (flags.width <= 256)
				goto width;
		}

		/* Length */
		switch (*fmt) {
		case 'l':
			flags.len = LONG;
			++fmt;
			break;
		case 'h':
			flags.len = SHORT;
			if (*(fmt + 1) == 'h') {
				flags.len = CHAR;
				++fmt;
			}
			++fmt;
			break;
		case 'z':
			flags.len = SIZE_T;
			++fmt;
			break;
		case 't':
			flags.len = PTRDIFF_T;
			++fmt;
			break;
		}

		/* Conversion specifier */
		switch (*fmt) {
		case 'p':
			if (flags.zero || flags.plus || flags.nums || flags.minus || flags.len || flags.wflag)
				break;

			flags.len = VOID;
			flags.zero = 1;
			flags.wflag = 1;
			//flags.width = sizeof(void *) << 1;
		case 'x':
			flags.low = 1;
		case 'X':
			flags.base = 1;
		case 'o':
			flags.base += 1;
		case 'u':
			flags.unsign = 1;
		case 'i':
		case 'd':
			flags.base += 1;
			switch (flags.len) {
			case INT:
				val.i = (unsigned int) va_arg(ap, unsigned int);
				if (!flags.unsign && (flags.sign = (int) val.i < 0))
					val.i = -(int) val.i;
				break;
			case CHAR:
				val.hh = (unsigned char) va_arg(ap, unsigned int);
				if (!flags.unsign && (flags.sign = (char) val.hh < 0))
					val.hh = -(char) val.hh;
				break;
			case SHORT:
				val.h = (unsigned short) va_arg(ap, unsigned int);
				if (!flags.unsign && (flags.sign = (short) val.h < 0))
					val.h = -(short) val.h;
				break;
			case LONG:
				val.l = (unsigned long) va_arg(ap, unsigned long);
				if (!flags.unsign && (flags.sign = (long) val.l < 0))
					val.l = -(long) val.l;
				break;
			case VOID:
				val.p = (void *) va_arg(ap, void *);
				break;
			case SIZE_T:
				val.z = (size_t) va_arg(ap, size_t);
				if (!flags.unsign && (flags.sign = (size_t) val.z < 0))
					val.z = -(size_t) val.z;
				break;
			case PTRDIFF_T:
				val.t = (ptrdiff_t) va_arg(ap, ptrdiff_t);
				if (!flags.unsign && (flags.sign = (ptrdiff_t) val.t < 0))
					val.t = -(ptrdiff_t) val.t;
				break;
			}
			print(putc, &val, &flags);
			break;
		case 's':
			if (flags.zero || flags.plus || flags.nums || flags.len)
				break;

			str = (const char *) va_arg(ap, const char *);
			print(putc, (void *) str, &flags);
			break;
		case '%':
			if (flags.zero || flags.plus || flags.nums || flags.minus || flags.len || flags.wflag)
				break;

			putc('%');
			break;
		default:
			putc(*fmt);
			break;
		case '\0':
			return;
		}
	}
}

void
kprintf(void (*putc) (int), const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	kvprintf(putc, fmt, ap);
	va_end(ap);
}

int
main(int argc, char *argv[])
{
	printf("NBPRINTBUFDEC: %lu\n", (unsigned long) NBPRINTBUFDEC);

	kprintf((void (*)(int)) putchar, "%%x %x|\n", 0xdead);
	printf("%%x %x|\n", 0xdead);
	kprintf((void (*)(int)) putchar, "%%x %x|\n", 0x0);
	printf("%%x %x|\n", 0x0);
	kprintf((void (*)(int)) putchar, "%%x %x|\n", 0xdbeef);
	printf("%%x %x|\n", 0xdbeef);

	kprintf((void (*)(int)) putchar, "%%X %X|\n", 0xdeadbeef);
	printf("%%X %X|\n", 0xdeadbeef);
	kprintf((void (*)(int)) putchar, "%%X %X|\n", 0xff);
	printf("%%X %X|\n", 0xff);

	kprintf((void (*)(int)) putchar, "%%p %p|\n", (void *) 0xbeef);
	printf("%%p %p|\n", (void *) 0xbeef);

	kprintf((void (*)(int)) putchar, "%%o %o|\n", 0xf);
	printf("%%o %o|\n", 0xf);
	kprintf((void (*)(int)) putchar, "%%o %o|\n", 0xdeadbeef);
	printf("%%o %o|\n", 0xdeadbeef);
	kprintf((void (*)(int)) putchar, "%%o %o|\n", 0);
	printf("%%o %o|\n", 0);
	kprintf((void (*)(int)) putchar, "%%o %o|\n", 7);
	printf("%%o %o|\n", 7);

	kprintf((void (*)(int)) putchar, "%%-8s: %-8s|\n", "beef");
	printf("%%-8s: %-8s|\n", "beef");
	kprintf((void (*)(int)) putchar, "%%-8s: %-8s|\n", "");
	printf("%%-8s: %-8s|\n", "");
	kprintf((void (*)(int)) putchar, "%%s: %s|\n", (const char *) NULL);
	printf("%%s: %s|\n", (const char *) NULL);
	kprintf((void (*)(int)) putchar, "%%256s: %256s|\n", "beef");
	printf("%%256s: %256s|\n", "beef");
	kprintf((void (*)(int)) putchar, "%%16s: %-16s|\n", "beef");
	printf("%%16s: %-16s|\n", "beef");
	kprintf((void (*)(int)) putchar, "%%2s: %-2s|\n", "beef");
	printf("%%2s: %-2s|\n", "beef");

	kprintf((void (*)(int)) putchar, "%%d: %d|\n", -1234);
	printf("%%d: %d|\n", -1234);

	kprintf((void (*)(int)) putchar, "%%u: %u|\n", -1234);
	printf("%%u: %u|\n", -1234);

	kprintf((void (*)(int)) putchar, "%%0i: %0i|\n", 0);
	printf("%%0i: %0i|\n", 0);

	kprintf((void (*)(int)) putchar, "%%#08X: %#08X|\n", 0xdead);
	printf("%%#08X: %#08X|\n", 0xdead);
	kprintf((void (*)(int)) putchar, "%%-#08X: %-#08X|\n", 0xcfee1);
	printf("%%-#08X: %-#08X|\n", 0xcfee1);
	kprintf((void (*)(int)) putchar, "%%# 8X: %# 8x|\n", 0xefff3);
	kprintf((void (*)(int)) putchar, "%%# 8X: %# 8x|\n", 0xefff3);

	kprintf((void (*)(int)) putchar, "%%: %%\n");
	printf("%%: %%\n");
	return 0;
}
