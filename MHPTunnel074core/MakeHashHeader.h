#pragma once

#include <string.h>
#include <stdlib.h>
#include <assert.h>

#define ASSERT assert
typedef int BOOL;
#define TRUE 1
#define FALSE 0
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned long DWORD;
typedef unsigned __int64 QWORD;

//#define CDECL __cdecl   // VC�̏ꍇ�͂��̍s���`
#define CDECL           // VC�ȊO�̏ꍇ�͂��̍s���`���邩�A�K�X��`������

// ROTATE
//#define ROTATE_L32(x,n)     ( _rotl(x,n) )  // _rotl()���g����ꍇ�͂������D��
//#define ROTATE_R32(x,n)     ( _rotr(x,n) )  // _rotr()���g����ꍇ�͂������D��
#define ROTATE_L32(x,n)     ( ((x) << (n)) | ((x) >> (32 - (n))) )  // _rotl()���g���Ȃ��ꍇ�͂�������g��
#define ROTATE_R32(x,n)     ( ((x) >> (n)) | ((x) << (32 - (n))) )  // _rotr()���g���Ȃ��ꍇ�͂�������g��


// ReverseEndian�ŃA�Z���u���ł��g���ꍇ��1�A�g��Ȃ��ꍇ��0
#define USE_REVERSEENDIAN_ASSEMBLER 0

// �e�n�b�V���ŃA�Z���u���ł��g���ꍇ��1�A�g��Ȃ��ꍇ��0
#define USE_ASSEMBLER_CRC           0
#define USE_ASSEMBLER_MD5           0
#define USE_ASSEMBLER_SHA1          0
#define USE_ASSEMBLER_SHA256        0
#define USE_ASSEMBLER_SHA384        0
#define USE_ASSEMBLER_SHA512        0
#define USE_ASSEMBLER_RIPEMD128     0
#define USE_ASSEMBLER_RIPEMD160     0
#define USE_ASSEMBLER_RIPEMD256     0
#define USE_ASSEMBLER_RIPEMD320     0


#if (USE_REVERSEENDIAN_ASSEMBLER==0)
    void ReverseEndian32(void *dest, const void *src, DWORD len);
    void ReverseEndian64(void *dest, const void *src, DWORD len);
#else
    extern "C" void CDECL ReverseEndian32(void *dest, const void *src, DWORD len);
    extern "C" void CDECL ReverseEndian64(void *dest, const void *src, DWORD len);
#endif

