/* Fast and accurate double to string conversion based on Florian Loitsch's
 * Grisu-algorithm ( http://florian.loitsch.com/publications/dtoa-pldi2010.pdf ).
 * based on the code from https://github.com/night-shift/fpconv
 */
/*
Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/
/*
 * Input:
 * fp -> the double to convert, dest -> destination buffer.
 * The generated string will never be longer than 24 characters.
 * Make sure to pass a pointer to at least 24 bytes of memory.
 * The emitted string will not be null terminated.
 *
 * Output:
 * The number of written characters.
 *
 * Exemplary usage:
 *
 * void print(double d)
 * {
 *      char buf[24 + 1] // plus null terminator
 *      int str_len = fpconv_dtoa(d, buf);
 *
 *      buf[str_len] = '\0';
 *      printf("%s", buf);
 * }
 *
 */
#ifndef HEADER_GRISU2
#define HEADER_GRISU2

#ifdef __cplusplus
extern "C" {
#endif


// there is no 32-bit fp printer there, so the results are worser for `float` type
// puts trailing zero
extern int grisu2_dtoa (const double fp, char dest[25]);

// stupid hack by ketmar
// puts trailing zero
extern int grisu2_ftoa (const float fp, char dest[25]);


#ifdef __cplusplus
}
#endif
#endif
