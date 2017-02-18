#!/usr/bin/env python

# Script to test how much bloating a large project will suffer when using
# different formatting methods.
# Based on bloat_test.sh from https://github.com/c42f/tinyformat.

from __future__ import print_function
import os, re, sys
from contextlib import nested
from glob import glob
from subprocess import check_call, check_output, Popen, PIPE
from timeit import timeit

template = r'''
#include <string>

#if defined(USE_NONE)
void doFormat_a() {
}
#elif defined(USE_FMTXX)
#ifdef USE_DLL
#define FMTXX_SHARED 1
#endif
#include "../src/Format.h"
void doFormat_a() {
  fmtxx::Format(stdout, "{}\n", "somefile.cpp");
  fmtxx::Format(stdout, "{}:{}\n", "somefile1.cpp", 42);
  fmtxx::Format(stdout, "{}:{}:{}\n", "somefile2.cpp", 42.1, "asdf");
  fmtxx::Format(stdout, "{}:{}:{}:{}\n", "somefile3.cpp", 42.12, 1u, "asdf1");
  fmtxx::Format(stdout, "{}:{}:{}:{}:{}\n", "somefile4.cpp", 42.123, 1l, 2ll, "asdf2");
}
#elif defined(USE_FMT)
#ifdef USE_DLL
#define FMT_SHARED 1
#endif
#include "../test/ext/fmt/fmt/format.h"
void doFormat_a() {
  fmt::print(stdout, "{}\n", "somefile.cpp");
  fmt::print(stdout, "{}:{}\n", "somefile1.cpp", 42);
  fmt::print(stdout, "{}:{}:{}\n", "somefile2.cpp", 42.1, "asdf");
  fmt::print(stdout, "{}:{}:{}:{}\n", "somefile3.cpp", 42.12, 1u, "asdf1");
  fmt::print(stdout, "{}:{}:{}:{}:{}\n", "somefile4.cpp", 42.123, 1l, 2ll, "asdf2");
}
#elif defined(USE_IOSTREAMS)
#include <iostream>
void doFormat_a() {
  std::cout << "somefile.cpp" << '\n';
  std::cout << "somefile1.cpp:" << 42 << '\n';
  std::cout << "somefile2.cpp:" << 42.1 << ":asdf" << '\n';
  std::cout << "somefile3.cpp:" << 42.12 << ':' << 1u << ":asdf1" << '\n';
  std::cout << "somefile4.cpp:" << 42.123 << ':' << 1l << ':' << 2ll << ":asdf2" << '\n';
}
#else
#include <cstdio>
void doFormat_a() {
  fprintf(stdout, "%s\n", "somefile.cpp");
  fprintf(stdout, "%s:%d\n", "somefile1.cpp", 42);
  fprintf(stdout, "%s:%g:%s\n", "somefile2.cpp", 42.1, "asdf");
  fprintf(stdout, "%s:%g:%u:%s\n", "somefile3.cpp", 42.12, 1u, "asdf1");
  fprintf(stdout, "%s:%g:%ld:%lld:%s\n", "somefile4.cpp", 42.123, 1l, 2ll, "asdf2");
}
#endif
'''

prefix = '_bloat_test_tmp_'
num_translation_units = 100

def delete_temp_files():
  filenames = glob(prefix + '*')
  for f in filenames:
    if os.path.exists(f):
      os.remove(f)

# Remove old files
delete_temp_files()

# Generate all the files.
main_source = prefix + 'main.cc'
main_header = prefix + 'all.h'
sources = [main_source]
with nested(open(main_source, 'w'), open(main_header, 'w')) as \
     (main_file, header_file):
  main_file.write(re.sub('^ +', '', '''
    #include "{}all.h"
    int main() {{
    '''.format(prefix), 0, re.MULTILINE))
  for i in range(num_translation_units):
    n = '{:03}'.format(i)
    func_name = 'doFormat_a' + n
    source = prefix + n + '.cc'
    sources.append(source)
    with open(source, 'w') as f:
      f.write(template.replace('doFormat_a', func_name).replace('42', str(i)))
    main_file.write(func_name + '();\n')
    header_file.write('void ' + func_name + '();\n')
  main_file.write('}')

# Find compiler.
compiler_paths = [
  'g++',
]

class Result:
  pass

# Measure compile time and executable size.
expected_output = None
def benchmark(compiler, method, flags, lib_path):
  output_filename = prefix + '.exe'
  if os.path.exists(output_filename):
    os.remove(output_filename)
  include_dir = '-I' + os.path.dirname(os.path.realpath(__file__))
  command = 'check_call({})'.format(
    [compiler, '-std=c++14', '-o', output_filename, include_dir] + sources + flags)
  result = Result()
  result.time = timeit(
    command, setup = 'from subprocess import check_call', number = 1)
  print('Compile time: {:.2f}s'.format(result.time))
  result.size = os.stat(output_filename).st_size
  print('Size: {}'.format(result.size))
  check_call(['strip', output_filename])
  result.stripped_size = os.stat(output_filename).st_size
  print('Stripped size: {}'.format(result.stripped_size))
  if method != 'none':
    output = check_output(['./' + output_filename], env={'LD_LIBRARY_PATH': lib_path})
    global expected_output
    if not expected_output:
      expected_output = output
    elif output != expected_output:
      print('Output:\n{}'.format(output))
      print('Expected:\n{}'.format(expected_output))
      raise Exception("output doesn't match")
    sys.stdout.flush()
  return result

configs = [
#  ('opt-3',     ['-O3', '-DNDEBUG', '-L../build/gmake/bin/Release'], '../build/gmake/bin/Release'),
   ('opt-2',     ['-O2', '-DNDEBUG', '-L../build/gmake/bin/Release'], '../build/gmake/bin/Release'),
#  ('debug',     ['-L../build/gmake/bin/Debug'], '../build/gmake/bin/Debug')
]

methods = [
  ('none',              ['-DUSE_NONE']),
  ('printf',            []),
  ('fmtxx',             ['-DUSE_FMTXX', '-DUSE_DLL', '-lfmtxx']),
  ('fmt',               ['-DUSE_FMT', '-DUSE_DLL', '-lfmt']),
]

def format_field(field, format = '', width = ''):
  return '{:{}{}}'.format(field, width, format)

def print_rulers(widths):
  for w in widths:
    print('=' * w, end = ' ')
  print()

# Prints a reStructuredText table.
def print_table(table, *formats):
  widths = [len(i) for i in table[0]]
  for row in table[1:]:
    for i in range(len(row)):
      widths[i] = max(widths[i], len(format_field(row[i], formats[i])))
  print_rulers(widths)
  row = table[0]
  for i in range(len(row)):
    print(format_field(row[i], '', widths[i]), end = ' ')
  print()
  print_rulers(widths)
  for row in table[1:]:
    for i in range(len(row)):
      print(format_field(row[i], formats[i], widths[i]), end = ' ')
    print()
  print_rulers(widths)

# Converts n to kibibytes.
def to_kib(n):
  return int(round(n / 1024.0))

NUM_RUNS = 1
for compiler in compiler_paths:
  for config, flags, lib_path in configs:
    results = {}
    for i in range(NUM_RUNS):
      for method, method_flags in methods:
        print('Benchmarking', compiler, config, method)
        sys.stdout.flush()
        new_result = benchmark(compiler, method, flags + method_flags + sys.argv[1:], lib_path)
        if method not in results:
          results[method] = new_result
          continue
        old_result = results[method]
        old_result.time = min(old_result.time, new_result.time)
        if new_result.size != old_result.size or \
           new_result.stripped_size != old_result.stripped_size:
          raise Exception('size mismatch')
    print(config, 'Results:')
    table = [
      ('Method', 'Compile Time, s', 'Executable size, KiB', 'Stripped size, KiB')
    ]
    for method, method_flags in methods:
      result = results[method]
      table.append(
        (method, result.time, to_kib(result.size), to_kib(result.stripped_size)))
    print_table(table, '', '.1f', '', '')

delete_temp_files()
