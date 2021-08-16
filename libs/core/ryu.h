// Copyright 2018 Ulf Adams
//
// The contents of this file may be used under the terms of the Apache License,
// Version 2.0.
//
//    (See accompanying file LICENSE-Apache or copy at
//     http://www.apache.org/licenses/LICENSE-2.0)
//
// Alternatively, the contents of this file may be used under the terms of
// the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE-Boost or copy at
//     https://www.boost.org/LICENSE_1_0.txt)
//
// Unless required by applicable law or agreed to in writing, this software
// is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.
#ifndef VAVOOM_CORELIB_RYU_COMMON_HEADER
#define VAVOOM_CORELIB_RYU_COMMON_HEADER

#ifdef __cplusplus
extern "C" {
#endif

/* 16 bytes are enough */
/* returns string length, puts trailing zero */
int f2s_buffered (float f, char result[16]);
/* returns string length, doesn't put trailing zero */
int f2s_buffered_n (float f, char *result);

/* 25 bytes are enough */
/* returns string length, puts trailing zero */
int d2s_buffered (double f, char result[25]);
/* returns string length, doesn't put trailing zero */
int d2s_buffered_n (double f, char *result);

// for parsing
#define RYU_SUCCESS          (0)
#define RYU_INPUT_TOO_SHORT  (1)
#define RYU_INPUT_TOO_LONG   (2)
#define RYU_MALFORMED_INPUT  (3)
#define RYU_INVALID_ARGS     (4)  /* something is NULL */

// returs RYU_xxx
int ryu_s2f (const char *buffer, const int len, float *result);

#ifdef __cplusplus
}
#endif
#endif
