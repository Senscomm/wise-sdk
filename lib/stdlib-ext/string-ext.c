/*
 * Copyright 2018-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <string.h>
#include <malloc.h>
#include <hal/types.h>

/* To avoid a name conflict.
 */
char *os_strndup2(const char *s, size_t n)
{
	int l = n > strlen(s) ? strlen(s) + 1 : n + 1;
	char *d = malloc(l);

	if (!d)
		return NULL;

	memcpy(d, s, l);
	d[l - 1] = '\0';

	return d;
}

static int toupper(int c)
{
	return (c >= 'a' && c <= 'z') ? c - 'a' + 'A' : c;
}

static char *strcasechr(const char *s, int uc)
{
	char ch;

	for (; *s != '\0'; s++) {
		ch = *s;
		if (toupper(ch) == uc) {
			return (char *)s;
		}
	}

	return NULL;
}

char *os_strcasestr2(const char *str, const char *substr)
{
	const char *candidate; /* Candidate in str with matching start character */
	char ch;                   /* First character of the substring */
	size_t len;                /* The length of the substring */

	/* Special case the empty substring */

	len = strlen(substr);
	ch  = *substr;

	if (!ch) {
		/* We'll say that an empty substring matches at the beginning of
		 * the string
		 */
		return (char *)str;
	}

	/* Search for the substring */

	candidate = str;
	ch = toupper(ch);

	while (1) {
		/* strcasechr() will return a pointer to the next occurrence of the
		 * character ch in the string (ignoring case)
		 */

		candidate = strcasechr(candidate, ch);
		if (!candidate || strlen(candidate) < len) {
			/* First character of the substring does not appear in the string
			 * or the remainder of the string is not long enough to contain the
			 * substring.
			 */
			break;
		}

		/* Check if this is the beginning of a matching substring
		 * (ignoring case)
		 */

		if (strncasecmp(candidate, substr, len) == 0) {
			/* Yes.. return the pointer to the first occurrence of the matching
			 * substring.
			 */

			return (char *)candidate;
		}

		/* No, find the next candidate after this one */

		candidate++;
	}

	/* Won't get here, but some compilers might complain.  Others might
	* complain about this code being unreachable too.
	*/

	return NULL;
}
