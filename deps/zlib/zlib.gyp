# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'use_system_zlib%': 0,
    'arm_fpu%': '',
    'llvm_version%': '0.0',
  },
  'conditions': [
    ['use_system_zlib==0', {
      'targets': [
        {
          'target_name': 'zlib',
          'type': 'static_library',
          'sources': [
            'adler32.c',
            'compress.c',
            'crc32.c',
            'crc32.h',
            'deflate.c',
            'deflate.h',
            'gzclose.c',
            'gzguts.h',
            'gzlib.c',
            'gzread.c',
            'gzwrite.c',
            'infback.c',
            'inffast.c',
            'inffast.h',
            'inffixed.h',
            'inflate.c',
            'inflate.h',
            'inftrees.c',
            'inftrees.h',
            'trees.c',
            'trees.h',
            'uncompr.c',
            'zconf.h',
            'zlib.h',
            'zutil.c',
            'zutil.h',
          ],
          'include_dirs': [
            '.',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '.',
            ],
          },
          'conditions': [
            ['target_arch=="arm64"', {
              'cflags': [ '-march=armv8-a+crc' ],
              'defines': [
                'ADLER32_SIMD_NEON',
                'INFLATE_CHUNK_SIMD_NEON',
                'INFLATE_CHUNK_READ_64LE',
                'CRC32_ARMV8_CRC32',
              ],
              'sources': [
                'adler32_simd.c',
                'adler32_simd.h',
                'chunkcopy.h',
                'inffast_chunk.c',
                'inffast_chunk.h',
              ],
            }],
            ['target_arch=="x64"', {
              'cflags': [ '-msse2', '-mssse3', ' -msse4.2', '-mpclmul' ],
              'defines': [
                'ADLER32_SIMD_SSSE3',
                'INFLATE_CHUNK_SIMD_SSE2',
                'INFLATE_CHUNK_READ_64LE',
                'CRC32_SIMD_SSE42_PCLMUL',
              ],
              'sources': [
                'adler32_simd.c',
                'adler32_simd.h',
                'chunkcopy.h',
                'crc32_simd.c',
                'crc32_simd.h',
                'inffast_chunk.c',
                'inffast_chunk.h',
              ],
            }],
          ],
        },
      ],
    }, {
      'targets': [
        {
          'target_name': 'zlib',
          'type': 'static_library',
          'direct_dependent_settings': {
            'defines': [
              'USE_SYSTEM_ZLIB',
            ],
          },
          'defines': [
            'USE_SYSTEM_ZLIB',
          ],
          'link_settings': {
            'libraries': [
              '-lz',
            ],
          },
        },
      ],
    }],
  ],
}
