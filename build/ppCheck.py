#!/usr/bin/env python

from __future__ import print_function
import os, sys

if not len(sys.argv) is 2 or not os.path.exists(sys.argv[1]):
  print("\nYou did not supply a valid path to check.")
  exit(1)
else:
  print("\nChecking for un-preprocessed files...", end = ' ')
  DIST_PATH = sys.argv[1]

PP_FILE_TYPES = (
  '.css',
  '.dtd',
  '.html',
  '.js',
  '.jsm',
  '.xhtml',
  '.xml',
  '.xul',
  '.manifest',
  '.properties',
  '.rdf'
)

PP_SPECIAL_TYPES = ('.css')

PP_DIRECTIVES = [
  'define',
  'if',
  'ifdef',
  'ifndef',
  'elif',
  'elifdef',
  'endif',
  'error',
  'expand',
  'filter',
  'include',
  'literal',
  'undef',
  'unfilter'
]

PP_FILES = []
PP_BAD_FILES = []

for root, directories, filenames in os.walk(DIST_PATH):
  for filename in filenames: 
    if filename.endswith(PP_FILE_TYPES):
      PP_FILES += [ os.path.join(root, filename).replace(os.sep, '/') ]

for file in PP_FILES:
  with open(file) as fp:
    marker = '%' if file.endswith(PP_SPECIAL_TYPES) else '#'
    directives = tuple(marker + directive for directive in PP_DIRECTIVES)
    for line in fp:
      if line.startswith(directives):
        PP_BAD_FILES += [ file.replace(DIST_PATH + '/', '') ]
  fp.close()

PP_BAD_FILES = list(dict.fromkeys(PP_BAD_FILES))

print('Done!')

if len(PP_BAD_FILES) > 0:
  print("\nWARNING: The following {0} file(s) in {1} may require preprocessing:\n".format(len(PP_BAD_FILES), DIST_PATH))
  for file in PP_BAD_FILES:
    print(file)

exit(0)