#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gb18030.h"
/*
 * test : 大S => b4f3 53
 * test : 休息的时候, => d0dd cfa2 b5c4 cab1 baf2 2c
 * test : 有个蠤在墙角 => d3d0 b8f6 d040 d4da c7bd bdc7
 */
int main(int argc, char ** argv)
{
	char buf[1024];
	int len = 0;
	if (argc < 2) {
		fgets(buf, sizeof(buf), stdin);
		buf[sizeof(buf)-1] = '\0';
		len = strlen(buf);
		if (len > sizeof(buf)-3) {
			len = sizeof(buf) - 3;
		}
		buf[len++] = 'O';
		buf[len++] = 'K';
		buf[len] = '\0';
	} else {
		len = snprintf(buf, sizeof(buf), "%sOK", argv[1]);
	}
	printf("string[%d](%s)\n", len, buf);
	int rv = is_valid_gb18030_chars(buf, len);
	printf("is_valid [%s]\n", rv==0?"false":"true");
	const char *p = str_valid_gb18030_head(buf, len - 2, 0);
	printf("\nhead cbl[%d] len[%d](%s) pos[%d]\n", len - 2, len, p, p - buf);

	p = str_valid_gb18030_tail(buf, len);
	printf("tail len[%d] pos[%d]\n", len, p - buf);
	return 1;
}

