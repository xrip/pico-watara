#ifndef __TYPES_H__
#define __TYPES_H__

#include <stdint.h>
#include <string.h> /* For memcpy() */

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _uint8_DEFINED
#define _uint8_DEFINED
typedef uint8_t   uint8;
typedef uint16_t  uint16;
typedef uint32_t  uint32;
typedef int8_t     int8;
typedef int16_t    int16;
typedef int32_t    int32;
#endif

#ifndef TRUE
#define TRUE  1
#define FALSE 0
#endif

#ifndef BOOL
#define BOOL int
#endif

#ifdef SV_USE_FLOATS
typedef float real;
#else 
typedef double real;
#endif

/*
 * Buffer
 */

#define WRITE_BUF_BOOL(x, buf) do { \
    uint8 _ = x ? 1 : 0; \
    memcpy(buf, &_, 1); \
    buf += 1; } while (0)
#define  READ_BUF_BOOL(x, buf) do { \
    uint8 _; \
    memcpy(&_, buf, 1); \
    buf += 1; \
    x = _ ? TRUE : FALSE; } while (0)

#define WRITE_BUF_uint8(x, buf)        do { \
    memcpy(buf, &x, sizeof(x)); \
    buf += sizeof(x); } while (0)
#define  READ_BUF_uint8(x, buf)        do { \
    memcpy(&x, buf, sizeof(x)); \
    buf += sizeof(x); } while (0)

#define WRITE_BUF_int8(x, buf)  WRITE_BUF_uint8(x, buf)
#define  READ_BUF_int8(x, buf)   READ_BUF_uint8(x, buf)

#define WRITE_BUF_uint16(x, buf)       do { \
    memcpy(buf, &x, sizeof(x)); \
    buf += sizeof(x); } while (0)
#define  READ_BUF_uint16(x, buf)       do { \
    memcpy(&x, buf, sizeof(x)); \
    buf += sizeof(x); } while (0)

#define WRITE_BUF_int16(x, buf)  WRITE_BUF_uint16(x, buf)
#define  READ_BUF_int16(x, buf)   READ_BUF_uint16(x, buf)

#define WRITE_BUF_uint32(x, buf)       do { \
    memcpy(buf, &x, sizeof(x)); \
    buf += sizeof(x); } while (0)
#define  READ_BUF_uint32(x, buf)       do { \
    memcpy(&x, buf, sizeof(x)); \
    buf += sizeof(x); } while (0)

#define WRITE_BUF_int32(x, buf)  WRITE_BUF_uint32(x, buf)
#define  READ_BUF_int32(x, buf)   READ_BUF_uint32(x, buf)

#define WRITE_BUF_real(x, buf)         do { \
    memcpy(buf, &x, sizeof(x)); \
    buf += sizeof(x); } while (0)
#define  READ_BUF_real(x, buf)         do { \
    memcpy(&x, buf, sizeof(x)); \
    buf += sizeof(x); } while (0)

#ifdef __cplusplus
}
#endif

#endif
