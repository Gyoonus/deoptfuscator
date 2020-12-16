# Copyright (C) 2014 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

def ToUnicode(string):
  """ Converts a string into Unicode.

  This is a delegate function for the built-in `unicode`. It checks if the input
  is not `None`, because `unicode` turns it into an actual "None" string.
  """
  assert string is not None
  return unicode(string)
