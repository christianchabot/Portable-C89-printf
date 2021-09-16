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

/* Lengths. */
enum {INT, CHAR, SHORT, LONG, VOID, SIZE_T, PTRDIFF_T, UNDEFINED = 0};

union value {
	unsigned int i;
	unsigned char hh;
	unsigned short h;
	unsigned long l;
	void *p;
	size_t z;
	ptrdiff_t t;
};

/* Base's */
enum {STR, DEC, OCT, HEX}; 

struct flag {
	unsigned int base   : 2; /* hex, oct, or dec? */
	unsigned int len    : 3; /* hh, h, l, z, t, or p? */
	unsigned int zero   : 1; /* 0 flag set? */
	unsigned int minus  : 1; /* - flag set? */
	unsigned int plus   : 1; /* + flag set? */
	unsigned int space  : 1; /* space flag set? */
	unsigned int nums   : 1; /* # flag set? */
	unsigned int low    : 1; /* Lowercase? */
	unsigned int pflag  : 1; /* Is precision set? */
	unsigned int unsign : 1; /* Is it unsigned? */
	unsigned int sign   : 1; /* Is the value negative? */
	unsigned int width;
	unsigned int prec;
};

static const char *digit = "0123456789ABCDEF0123456789abcdef"; 
static const struct {
	unsigned int soff;
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
	const unsigned char *srt = (const unsigned char *) val + ltab[flags->len].soff, *end;
	unsigned long dec = 0;
	unsigned char *tmp = (unsigned char *) &dec;

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
	unsigned char *srt = (unsigned char *) val + ltab[flags->len].soff, *end;
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

	return buf;
}

static void
print(void (*putc) (int), void *val, struct flag *flags)
{
	const char *nstr = "0X0x", *estr = nstr + sizeof(char);
	char buf[NBPRINTBUF], *end, *srt;
	unsigned pad, prec, nums = 0;

	switch (flags->base) {
	case STR:
		if (!(srt = val))
			srt = "(NULL)";

		end = srt + strlen(srt);
		if (flags->pflag && flags->prec < end - srt)
			end = srt + flags->prec;
		break;
	case HEX:
		estr += sizeof(char);
		if (flags->low) {
			nstr += sizeof("0X") - sizeof(char);
			estr += sizeof("0X") - sizeof(char);
		}
	case OCT:
		end = printhexoct(srt = buf, val, flags);
		nums = estr - nstr;
		break;
	case DEC:
		end = printdec(srt = buf, val, flags);
		if (!flags->unsign && flags->sign || flags->plus || flags->space) {
			nstr = flags->plus ? "+" : flags->space ? " " : "-";
			estr = nstr + sizeof(char);
			nums = nstr - estr;
		}
	}

	/* Corner case: value is 0 and precision is 0 (do not print anything). */
	if (flags->base && !flags->nums)
		if (flags->pflag)
			if (!flags->prec && end - srt <= 1 && *srt == '0')
				end = srt;

	prec = flags->prec > end - srt ? flags->prec - (end - srt) : 0;
	pad = flags->width > end - srt + prec + nums ? flags->width - (end - srt + prec + nums) : 0;
	if (pad && !flags->minus) {
		int pbyte = !flags->nums && flags->zero && flags->base ? '0' : ' ';
		do {
			putc(pbyte);
		} while (--pad);
	}

	/* print -, +, 0x, or 0 */
	if (nums)
		if (end - srt > 1 || *srt != '0' || flags->plus || flags->space)
			while (nstr < estr)
				putc(*nstr++);

	/* Corner case: ignore precision if negative. */
	if (!flags->minus || flags->width)
		for (; prec; --prec)
			putc('0');

	if (flags->base)
		while (end > srt)
			putc(*--end);
	else
		for (; srt < end; ++srt)
			putc(*srt);

	if (pad && flags->minus)
		do {
			putc(' ');
		} while (--pad);
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
			if (!flags.minus)
				flags.zero = 1;
			goto flag;
		case '-':
			flags.minus = 1;
			flags.zero = 0;
			goto flag;
		case ' ':
			if (!flags.plus)
				flags.space = 1;
			goto flag;
		case '+':
			flags.plus = 1;
			flags.space = 0;
			goto flag;
		case '#':
			flags.nums = 1;
			goto flag;
		}

		/* Width */
		if (*fmt == '*') {
			flags.width = (unsigned int) va_arg(ap, int);
			++fmt;
		} else
	width:
		switch (*fmt) {
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			flags.width = ((flags.width << 2) + flags.width << 1);
			flags.width += *fmt++ - '0';
			if (flags.width <= 256)
				goto width;
		}

		/* Precision */
		if (*fmt == '.') {
			flags.pflag = 1;

			if (*++fmt == '*') {
				flags.prec = (unsigned int) va_arg(ap, int);
				++fmt;
			} else	
		precision:
				switch (*fmt) {
				case '0': case '1': case '2': case '3': case '4':
				case '5': case '6': case '7': case '8': case '9':
					flags.prec = ((flags.prec << 2) + flags.prec << 1);
					flags.prec += *fmt++ - '0';
					if (flags.prec <= 256)
						goto precision;
				}
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
			if (flags.zero || flags.plus || flags.nums || flags.minus || flags.len || flags.pflag || flags.width)
				break;

			flags.nums = 1;
			flags.len = VOID;
		case 'x':
			flags.low = 1;
		case 'X':
			flags.base = 1;
		case 'o':
			flags.base += 1;
			if (flags.plus)
				break;
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

			str = va_arg(ap, const char *);
			print(putc, (void *) str, &flags);
			break;
		case '%':
			if (flags.zero || flags.plus || flags.nums || flags.minus || flags.len || flags.pflag)
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
	int tmp = 0xbeef;
	kprintf((void (*)(int)) putchar, "%%+.d: %+.d|\n", 0);
	printf("%%+.d: %+.d|\n", 0);
	kprintf((void (*)(int)) putchar, "%%.0d: %.0d|\n", 0);
	printf("%%.0d: %.0d|\n", 0);
	kprintf((void (*)(int)) putchar, "%%+.d: %+.d|\n", 1);
	printf("%%+.d: %+.d|\n", 1);
	kprintf((void (*)(int)) putchar, "%%+.d: %+.d|\n", tmp);
	printf("%%+.d: %+.d|\n", tmp);
	kprintf((void (*)(int)) putchar, "%%2.8d: %2.8d|\n", tmp);
	printf("%%2.8d: %2.8d|\n", tmp);
	kprintf((void (*)(int)) putchar, "%%-.8d: %-.8d|\n", tmp);
	printf("%%-.8d: %-.8d| (glibc produces incorrect output?)\n", tmp);

	kprintf((void (*)(int)) putchar, "%%p: %p| (implementation defined)\n", NULL);
	printf("%%p: %p| (implementation defined)\n", NULL);
	kprintf((void (*)(int)) putchar, "%%p: %p| (implementation defined)\n", &tmp);
	printf("%%p: %p| (implementation defined)\n", &tmp);

	kprintf((void (*)(int)) putchar, "%%#*.*x: %#*.*x|\n", 12, 8, tmp);
	printf("%%#*.*x: %#*.*x|\n", 12, 8, tmp);
	kprintf((void (*)(int)) putchar, "%%#.3X: %#.3X|\n", tmp);
	printf("%%#.3X: %#.3X|\n", tmp);

	kprintf((void (*)(int)) putchar, "%%#o: %#o|\n", 0);
	printf("%%#o: %#o|\n", 0);
	kprintf((void (*)(int)) putchar, "%%#o: %#o|\n", 7);
	printf("%%#o: %#o|\n", 7);
	kprintf((void (*)(int)) putchar, "%%-10o: %-10o|\n", tmp);
	printf("%%-10o: %-10o|\n", tmp);

	return 0;
}
