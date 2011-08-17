#ifndef AS_GB18030_H_DEFINE
# define AS_GB18030_H_DEFINE

/*
 * single charset [0x00, 0x7F]
 * two    charset 1st.[0x81, 0xFE] 2nd.[0x40, 0x7E] || [0x80, 0xFE]
 * four   charset 1st.[0x81, 0xFE] 2nd.[0x30, 0x39] 3rd.[0x81, 0xFE] 4th.[0x30, 0x39]
 * ==================================================================================
 * 1.[0x00,                 0x7F]
 * 2.                            [0x81, 0xFE]
 * 2.                [0x40, !0x7F     , 0xFE]
 * 4.                            [0x81, 0xFE]
 * 4.  [0x30, 0x39]
 * 4.                            [0x81, 0xFE]
 * 4.  [0x30, 0x39]
 * ============================================
 * test : 大S => b4f3 53
 * test : 休息的时候, => d0dd cfa2 b5c4 cab1 baf2 2c
 * test : 有个蠤在墙角 => d3d0 b8f6 d040 d4da c7bd bdc7
 * test : 山东潍坊no5
 */
#define VAL_C(c) ((unsigned char)(c))

#define IS_ASCII(c) ((VAL_C(c) & ~0x7f) == 0)
#define MUST_IS_ASCII(c) ((VAL_C(c) < 0x30) || (VAL_C(c) > 0x39 && VAL_C(c) < 0x40) || VAL_C(c) == 0x7f)

#define IS_GB_DWORD_HIGH(c) (VAL_C(c) & 0x80)

#define IS_GB_DWORD_LOW(c) (VAL_C(c) > 0x3f && VAL_C(c) != 0xff && VAL_C(c)!=0x7f)
#define IS_GB_DWORD_LOW_HIGH_PART IS_GB_DWORD_HIGH
#define IS_GB_DWORD_LOW_LOW_PART(c) (VAL_C(c) > 0x3f && VAL_C(c) < 0x7f)

#define IS_GB_DDWORD_LOW(c) (VAL_C(c) > 0x2f && VAL_C(c) < 0x3a)
#define IS_GB_HALF_DDWORD(p_c) (IS_GB_DWORD_HIGH(*(p_c)) && IS_GB_DDWORD_LOW(*((p_c)+1)))
#define IS_GB_DDWORD_3LOW(p_c) (IS_GB_DDWORD_LOW(*(p_c)) && IS_GB_HALF_DDWORD((p_c)+1))

#define IS_GBK(p_c) (IS_ASCII(*(p_c)) || (IS_GB_DWORD_HIGH(*(p_c)) && IS_GB_DWORD_LOW(*((p_c)+1))))

#define IS_GB_DDWORD(p_c) (IS_GB_HALF_DDWORD(p_c) && IS_GB_HALF_DDWORD((p_c)+2))

#define IS_GB18030(p_c) (IS_GBK(p_c) || (IS_GB_HALF_DDWORD(p_c) && IS_GB_HALF_DDWORD((p_c)+2)))

#define VAL_BETWEEN(v, vl, vr) ( VAL_C(v) > VAL_C(vl) && VAL_C(v) < VAL_C(vr) )
#define IS_GBK_1(p_c) (VAL_BETWEEN(*(p_c), 0xa0, 0xaa) && VAL_BETWEEN(*((p_c)+1), 0xa0, 0xff))
#define IS_GBK_2(p_c) (VAL_BETWEEN(*(p_c), 0xaf, 0xf8) && VAL_BETWEEN(*((p_c)+1), 0xa0, 0xff))
#define IS_GBK_3(p_c) (VAL_BETWEEN(*(p_c), 0x80, 0xa1) && VAL_BETWEEN(*((p_c)+1), 0x3f, 0xff) && *((p_c)+1) != 0x7f)
#define IS_GBK_4(p_c) (VAL_BETWEEN(*(p_c), 0xa9, 0xff) && VAL_BETWEEN(*((p_c)+1), 0x3f, 0xa1) && *((p_c)+1) != 0x7f)
#define IS_GBK_5(p_c) (VAL_BETWEEN(*(p_c), 0xa7, 0xaa) && VAL_BETWEEN(*((p_c)+1), 0x3f, 0xa1) && *((p_c)+1) != 0x7f)
#define IS_GBK_U1(p_c) (VAL_BETWEEN(*(p_c), 0xa9, 0xb0) && VAL_BETWEEN(*((p_c)+1), 0xa0, 0xff))
#define IS_GBK_U2(p_c) (VAL_BETWEEN(*(p_c), 0xf7, 0xff) && VAL_BETWEEN(*((p_c)+1), 0xa0, 0xff))
#define IS_GBK_U3(p_c) (VAL_BETWEEN(*(p_c), 0xa0, 0xa8) && VAL_BETWEEN(*((p_c)+1), 0x3f, 0xa1) && *((p_c)+1) != 0x7f)
#define IS_GBK_DWORD_NU(p_c) (IS_GBK_1(p_c) || IS_GBK_2(p_c) || IS_GBK_3(p_c) || IS_GBK_4(p_c) || IS_GBK_5(p_c))

int is_valid_gb18030_chars(const char *s, const int slen)
{
	int i=0;
	for (i=slen; i--; ) {
		if (i > 3) {
			if (IS_GB18030(s)) {
				i-=3;
				s+=4;
			} else {
				return 0;
			}
		} else if (i > 1) {
			if (IS_GBK(s)) {
				i-=1;
				s+=2;
			} else {
				return 0;
			}
		} else {
			if (IS_GB_DWORD_HIGH(s[i])) {
				return 0;
			}
		}
	}
	return 1;
}

const char *str_valid_gb18030_head(const char *s /*point to begin*/, const int need_check_len, int strict)
{
	int i = 0;
	int min_valid = need_check_len;

	{
#define IS_HAS_MORE_CHAR(num) (min_valid - i > (num))
		for (i=0; i<min_valid; i++) {
			if (MUST_IS_ASCII(s[i])) {
				min_valid = i;
				break;
			} else if (IS_GB_DWORD_LOW_LOW_PART(s[i])) {
				if (min_valid > i + 1) {
					min_valid = i + 1;
				}
				break;
			} else if (IS_GB_DDWORD_LOW(s[i])) {
				if (IS_HAS_MORE_CHAR(1) && !IS_GB_DWORD_HIGH(s[i+1])) {
					min_valid = i + 1;
					break;
				}
				if (i > 0 && !IS_GB_DWORD_HIGH(s[i-1])) {
					min_valid = i;
					break;
				}
				if (!IS_HAS_MORE_CHAR(2)) {
					min_valid = i + 1;
					break;
				}
				if (/* IS_HAS_MORE_CHAR(2) && */ !IS_GB_HALF_DDWORD(s + i + 1)) {
					// must not be the 2nd char of a DDWORD
					if (i > 2) {
						if (IS_GB_DDWORD(s + i - 3)) {
							min_valid = i - 3;
						} else {
							min_valid = i;
						}
					} else {
						min_valid = i + 1;
					}
					break;
				}
				// 后面必定是4字节汉字的半个汉字 _HALF_DDWORD
				if (i > 0) {
					if (/* IS_HAS_MORE_CHAR(2) && */ IS_GB_DDWORD(s + i - 1)) {
						// is 2/4 DDWORD
						min_valid = i - 1;
					} else {
						min_valid = i;
					}
					break;
				}
				// i == 0
			}
		}
#undef IS_HAS_MORE_CHAR
		for (i=min_valid; i--;) {
			if (IS_GB_DDWORD_LOW(s[i])) {
				if (i > 2 && IS_GB_DDWORD(s + i - 3)) {
					i -= 3;
				}
				min_valid = i;
				continue;
			}
		}
		for (i=min_valid; i--;) {
			if (IS_GB_DWORD_LOW_HIGH_PART(s[i]) && i>0 && IS_GB_DWORD_HIGH(s[i-1])) {
				i -= 1;
				continue;
			} else {
				break;
			}
		}
		min_valid = i + 1;
		if (min_valid < 1) {
			return s;
		}
	}

	if (strict) {
		return s + min_valid;
	}
	//printf("min_valid %d( %s )\n", min_valid, s + min_valid);
	for (i=min_valid; i--;) {
		//printf("%d %#x ", i, VAL_C(s[i]));
		if (IS_GB_DWORD_LOW_LOW_PART(s[i]) && i>0 && IS_GB_DWORD_HIGH(s[i-1])) {
			min_valid = i + 1;
			//printf("DWORD HL\n");
			/* 有个蠤在墙角 => d3d0 b8f6 d040 d4da c7bd bdc7 */
			/* 在墙h => d4 da c7 bd 68 */
			/* 谇絟 => da c7 bd 68 */
			/* 山东潍坊no5 => c9 bd b6 ab ce ab b7 bb 6e 6f 35 */
			if (IS_GBK_DWORD_NU(s + i - 1)) {
				//printf("in IS_GBK_DWORD_NU %d %#x %#x\n", i-1, VAL_C(s[i-1]), VAL_C(s[i]));
				int j = 0;
				for (j=0; j<i; j+=2) {
					if (!IS_GBK_DWORD_NU(s + j)) {
						break;
					}
				}
				if (j <= i) {
					continue;
				}
				i -= 1;
			}
			//i -= 1; // it's just may be a DWORD
			continue;
		} else if (IS_GB_DWORD_LOW_HIGH_PART(s[i]) && i>0 && IS_GB_DWORD_HIGH(s[i-1])) {
			i -= 1;
			continue;
		} else if (!IS_ASCII(s[i])) {
			if (i!=0) {
				//printf("!IS_ASCII may be catch some mistake ind[%d]\n", i);
				i = min_valid;
			}
			break;
		}
	}

	if (i < min_valid) {
		i++;
	}
	if (i < 0) {
		i = 0;
	}
	return s + i;
}

const char *str_valid_gb18030_tail(const char *s, const int slen)
{
	int i = 0;
	for (i=0; i<slen; i++) {
		if (s[i] & 0x80) {
			if (i+1 == slen) {
				return s + i;
			}
			i++;
		}
	}
	return s + slen;
}

int is_valid_gb18030_word(char *s, int len)
{
	if (len < 1) {
		return 0;
	} else if (len == 1) {
		if (IS_ASCII(*s)) {
			return 1;
		}
	} else if (len == 2) {
		if (IS_ASCII(*s)) {
			return 1;
		}
		if (IS_GBK(s)) {
			return 2;
		}
	} else if (len > 2) {
		if (IS_ASCII(*s)) {
			return 1;
		}
		if (IS_GBK(s)) {
			return 2;
		}
		if (IS_GB18030(s)) {
			return 4;
		}
	}
	return 0;
}

#endif /* AS_GB18030_H_DEFINE */
/* vim: set fdl=0: */
