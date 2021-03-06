# Copyright (c) 2015, the Fletch project authors. Please see the AUTHORS file
# for details. All rights reserved. Use of this source code is governed by a
# BSD-style license that can be found in the LICENSE.md file.

# TODO(ahe): Move this file elsewhere?

{
  'includes': [
    'common.gypi'
  ],

  'variables': {
    'LK_PROJECT%': 'vexpress-a9-test',

    'LK_CPU%': 'cortex-a9',
  },

  'target_defaults': {

    'configurations': {

      'fletch_lk_custom': {
        'abstract': 1,

        'inherit_from': ['fletch_lk'],

        'target_conditions': [
          ['_toolset=="target"', {
            'cflags': [
              '-mcpu=<(LK_CPU)',
              '-include',
              'build-<(LK_PROJECT)/config.h',
            ],
          }],
        ],
      },

      'DebugLK': {
        'inherit_from': [
          'fletch_base', 'fletch_debug', 'fletch_lk_custom',
          'fletch_disable_live_coding', 'fletch_disable_ffi',
          'fletch_disable_print_interceptors',
        ],
      },

      'ReleaseLK': {
        'inherit_from': [
          'fletch_base', 'fletch_release', 'fletch_lk_custom',
          'fletch_disable_live_coding', 'fletch_disable_ffi',
          'fletch_disable_print_interceptors',
        ],
      },

      'fletch_lk_vexpress': {
        'abstract': 1,

        'inherit_from': ['fletch_lk'],

        'target_conditions': [
          ['_toolset=="target"', {
            'cflags': [
              '-mcpu=cortex-a9',
              '-include',
              'build-vexpress-a9-fletch/config.h',
            ],
          }],
        ],
      },

      'DebugLKVExpress': {
        'inherit_from': [
          'fletch_base', 'fletch_debug', 'fletch_lk_vexpress',
          'fletch_disable_live_coding', 'fletch_disable_ffi',
          'fletch_disable_print_interceptors',
        ],
      },

      'ReleaseLKVExpress': {
        'inherit_from': [
          'fletch_base', 'fletch_release', 'fletch_lk_vexpress',
          'fletch_disable_live_coding', 'fletch_disable_ffi',
          'fletch_disable_print_interceptors',
        ],
      },
    },
  },
}
