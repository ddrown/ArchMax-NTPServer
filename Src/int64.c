// this code started from https://stackoverflow.com/questions/190229/where-is-the-itoa-function-in-linux
#include "main.h"
#include "int64.h"

#include <string.h>

/* reverse:  reverse string s in place */
static void reverse_string(char s[]) {
     int i, j;
     char c;

     for (i = 0, j = strlen(s)-1; i<j; i++, j--) {
         c = s[i];
         s[i] = s[j];
         s[j] = c;
     }
}

/* i64toa:  convert n to characters in s */
void i64toa(int64_t n, char s[]) {
     int i;
     int64_t sign;

     if ((sign = n) < 0LL)  /* record sign */
         n = -n;          /* make n positive, fails at max negative */
     i = 0;
     do {       /* generate digits in reverse order */
         s[i++] = (n % 10LL) + '0';   /* get next digit */
         n = n / 10LL;
     } while (n > 0LL);     /* delete it */
     if (sign < 0LL)
         s[i++] = '-';
     s[i] = '\0';
     reverse_string(s);
}

/* u64toa:  convert n to characters in s */
void u64toa(uint64_t n, char s[]) {
     int i;

     i = 0;
     do {       /* generate digits in reverse order */
         s[i++] = (n % 10ULL) + '0';   /* get next digit */
         n = n / 10ULL;
     } while (n > 0ULL);     /* delete it */
     s[i] = '\0';
     reverse_string(s);
}
