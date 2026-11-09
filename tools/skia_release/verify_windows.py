#! /usr/bin/env python3

import os
import pathlib
import re
import subprocess
import tempfile

import common


def main():
  skia_dir = common.skia_dir()
  build_type = common.build_type()
  out = skia_dir / 'out' / (build_type + '-windows-x64')
  runtime = '/MDd' if build_type == 'Debug' else '/MD'
  library_names = ('skparagraph', 'skshaper', 'skunicode_icu', 'skunicode_core',
                   'skia', 'harfbuzz', 'icu', 'skcms')

  symbols = subprocess.check_output(
      ['dumpbin.exe', '/nologo', '/linkermember:1', str(out / 'skia.lib')], text=True)
  names = set(re.findall(r'^\s*[0-9A-F]+\s+(\S+)\s*$', symbols, re.MULTILINE))
  mapping = (skia_dir / 'third_party/externals/libjpeg-turbo/src/jpeglibmangler.h').read_text()
  jpeg_symbols = set(re.findall(r'^#define\s+(\w+)\s+chromium_\w+', mapping, re.MULTILINE))
  if not jpeg_symbols:
    raise RuntimeError('The pinned JPEG symbol mapping is empty')
  jpeg_symbols.update(('jinit_arith_decoder', 'jinit_arith_encoder',
                       'jpeg_aritab', 'jpeg_natural_order'))
  if names.intersection(jpeg_symbols):
    raise RuntimeError('JPEG symbols are not completely isolated: ' +
                       ', '.join(sorted(names.intersection(jpeg_symbols))))
  for symbol in ('jpeg_start_decompress', 'jpeg_read_scanlines', 'jpeg_CreateCompress',
                 'jpeg12_read_scanlines', 'jpeg16_read_scanlines',
                 'jinit_arith_decoder', 'jinit_arith_encoder',
                 'jpeg_aritab', 'jpeg_natural_order'):
    if 'chromium_' + symbol not in names:
      raise RuntimeError('Missing prefixed JPEG symbol: ' + symbol)

  with tempfile.TemporaryDirectory(prefix='sdk-check-', dir=out) as directory:
    scratch = pathlib.Path(directory)
    exe = scratch / 'verify.exe'
    subprocess.check_call([
        'cl.exe', '/nologo', '/std:c++20', '/EHsc', '/utf-8', runtime,
        '/DSK_USING_THIRD_PARTY_ICU', '/DU_STATIC_IMPLEMENTATION', '/DU_DISABLE_RENAMING=1',
        '/I' + str(skia_dir),
        '/I' + str(skia_dir / 'third_party/externals/icu/source/common'),
        str(pathlib.Path(__file__).with_suffix('.cpp')),
        '/Fo' + str(scratch / 'verify.obj'), '/Fe' + str(exe), '/link',
        *[str(out / (name + '.lib')) for name in library_names],
        'dwrite.lib', 'usp10.lib', 'gdi32.lib', 'user32.lib', 'ole32.lib',
        'windowscodecs.lib', 'fontsub.lib', 'advapi32.lib',
    ], cwd=scratch)

    font = pathlib.Path(os.environ['WINDIR']) / 'Fonts/segoeui.ttf'
    cwd = scratch / 'different-cwd'
    cwd.mkdir()
    env = os.environ.copy()
    env['ICU_DATA'] = str(cwd)
    subprocess.check_call([str(exe), str(font)], cwd=cwd, env=env)

    for directory in (scratch, cwd):
      (directory / 'icudtl.dat').write_bytes(b'not an ICU data file')
    subprocess.check_call([str(exe), str(font)], cwd=cwd, env=env)

  print('PASS: JPEG symbol isolation; fresh processes without or with invalid external ICU data')


if __name__ == '__main__':
  main()
