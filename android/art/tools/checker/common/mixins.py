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

class EqualityMixin:
  """ Object equality via equality of dictionaries. """

  def __eq__(self, other):
    return isinstance(other, self.__class__) \
       and self.__dict__ == other.__dict__

class PrintableMixin:
  """ Prints object as name-dictionary pair. """

  def __repr__(self):
    return "<%s: %s>" % (type(self).__name__, str(self.__dict__))
