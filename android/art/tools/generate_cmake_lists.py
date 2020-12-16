#!/usr/bin/env python
#
# Copyright 2017, The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


"""
./generate_cmake_lists.py --project-name <project-name> --arch <arch>

- project-name - name of the new project
- arch - arch type. make generates seperate CMakeLists files for
         each architecture. To avoid collision in targets, only one of
         them can be included in the super project.

The primary objective of this file is to generate CMakeLists files
for CLion setup.

Steps to setup CLion.
1) Open the generated CMakeList file in CLion as a project.
2) Change the project root ANDROID_BUILD_TOP.
(Also, exclude projects that you don't bother about. This will make
the indexing faster).
"""

import sys
import os
import subprocess
import argparse

def get_android_build_top():
  path_to_top = os.environ.get('ANDROID_BUILD_TOP')
  if not path_to_top:
    # nothing set. try to guess it based on the relative path of this env.py file.
    this_file_path = os.path.realpath(__file__)
    path_to_top = os.path.join(os.path.dirname(this_file_path), '../..')
    path_to_top = os.path.realpath(path_to_top)

  if not os.path.exists(os.path.join(path_to_top, 'build/envsetup.sh')):
    print path_to_top
    raise AssertionError("geneate_cmake_lists.py must be located inside an android source tree")

  return path_to_top

def main():
  # Parse arguments
  parser = argparse.ArgumentParser(description="Generate CMakeLists files for ART")
  parser.add_argument('--project-name', dest="project_name", required=True,
                      help='name of the project')
  parser.add_argument('--arch', dest="arch", required=True, help='arch')
  args = parser.parse_args()
  project_name = args.project_name
  arch = args.arch

  # Invoke make to generate CMakeFiles
  os.environ['SOONG_GEN_CMAKEFILES']='1'
  os.environ['SOONG_GEN_CMAKEFILES_DEBUG']='1'

  ANDROID_BUILD_TOP = get_android_build_top()

  subprocess.check_output(('make -j64 -C %s') % (ANDROID_BUILD_TOP), shell=True)

  out_art_cmakelists_dir = os.path.join(ANDROID_BUILD_TOP,
                                        'out/development/ide/clion/art')

  # Prepare a list of directories containing generated CMakeLists files for sub projects.
  cmake_sub_dirs = set()
  for root, dirs, files in os.walk(out_art_cmakelists_dir):
    for name in files:
      if name == 'CMakeLists.txt':
        if (os.path.samefile(root, out_art_cmakelists_dir)):
          continue
        if arch not in root:
          continue
        cmake_sub_dir = cmake_sub_dirs.add(root.replace(out_art_cmakelists_dir,
                                                        '.'))

  # Generate CMakeLists file.
  f = open(os.path.join(out_art_cmakelists_dir, 'CMakeLists.txt'), 'w')
  f.write('cmake_minimum_required(VERSION 3.6)\n')
  f.write('project(%s)\n' % (project_name))

  for dr in cmake_sub_dirs:
    f.write('add_subdirectory(%s)\n' % (dr))


if __name__ == '__main__':
  main()
