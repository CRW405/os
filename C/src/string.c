#include "string.h"

// =====================================================================
// String Helpers
// =====================================================================

int strlen(const char *s) {
	int len = 0;
	while (s[len]) {
		len++;
	}
	return len;
}

int strcmp(const char *s1, const char *s2) {
	while (*s1 && (*s1 == *s2)) {
		s1++;
		s2++;
	}
	return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

// check if s1 starts with s2, and if so return a pointer to the first
// character after the match or the first space after the match, otherwise null.
// differentiates between 'echoes' and 'echo'
const char *str_match_prefix(const char *s1, const char *s2) {
	while (*s2) {
		if (*s1 != *s2)
			return 0;
		s1++;
		s2++;
	}
	if (*s1 == ' ')
		return s1 + 1;
	if (*s1 == 0)
		return s1;
	return 0;
}

// lowercase a alphabetical character
char clower(char c) {
	if (c >= 'A' && c <= 'Z') {
		return c + ('a' - 'A');
	}
	return c;
}

// lowercase a string
const char *strlower(const char *s) {
	static char buf[256];
	int         i;
	for (i = 0; s[i]; i++) {
		buf[i] = clower(s[i]);
	}
	buf[i] = 0;
	return buf;
}

const char *itoa(int value, char *buffer, int base) {
	if (base < 2 || base > 36) {
		buffer[0] = '\0';
		return buffer;
	}

	char *ptr = buffer, *ptr1 = buffer, tmp_char;
	int   tmp_value;

	do {
		tmp_value = value;
		value /= base;
		*ptr++ = "0123456789abcdefghijklmnopqrstuvwxyz"[tmp_value - value * base];
	} while (value);

	// Apply negative sign for base 10
	if (tmp_value < 0 && base == 10) {
		*ptr++ = '-';
	}
	*ptr-- = '\0';

	while (ptr1 < ptr) {
		tmp_char = *ptr;
		*ptr--   = *ptr1;
		*ptr1++  = tmp_char;
	}
	return buffer;
}
