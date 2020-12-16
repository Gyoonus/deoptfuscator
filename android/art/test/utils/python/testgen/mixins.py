#!/usr/bin/python3
#
# Copyright (C) 2015 The Android Open Source Project
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
Common mixins and abstract base classes (ABCs) useful for writing test generators in python
"""

import abc
import collections.abc
import functools

class Named(metaclass=abc.ABCMeta):
  """
  An abc that defines a get_name method.
  """

  @abc.abstractmethod
  def get_name(self):
    """
    Returns a unique name to use as the identity for implementing comparisons.
    """
    pass

class FileLike(metaclass=abc.ABCMeta):
  """
  An abc that defines get_file_name and get_file_extension methods.
  """

  @abc.abstractmethod
  def get_file_name(self):
    """Returns the filename this object represents"""
    pass

  @abc.abstractmethod
  def get_file_extension(self):
    """Returns the file extension of the file this object represents"""
    pass

@functools.lru_cache(maxsize=None)
def get_file_extension_mixin(ext):
  """
  Gets a mixin that defines get_file_name(self) in terms of get_name(self) with the
  given file extension.
  """

  class FExt(object):
    """
    A mixin defining get_file_name(self) in terms of get_name(self)
    """

    def get_file_name(self):
      return self.get_name() + ext

    def get_file_extension(self):
      return ext

  # Register the ABCs
  Named.register(FExt)
  FileLike.register(FExt)

  return FExt

class SmaliFileMixin(get_file_extension_mixin(".smali")):
  """
  A mixin that defines that the file this class belongs to is get_name() + ".smali".
  """
  pass

class JavaFileMixin(get_file_extension_mixin(".java")):
  """
  A mixin that defines that the file this class belongs to is get_name() + ".java".
  """
  pass

class NameComparableMixin(object):
  """
  A mixin that defines the object comparison and related functionality in terms
  of a get_name(self) function.
  """

  def __lt__(self, other):
    return self.get_name() < other.get_name()

  def __gt__(self, other):
    return self.get_name() > other.get_name()

  def __eq__(self, other):
    return self.get_name() == other.get_name()

  def __le__(self, other):
    return self.get_name() <= other.get_name()

  def __ge__(self, other):
    return self.get_name() >= other.get_name()

  def __ne__(self, other):
    return self.get_name() != other.get_name()

  def __hash__(self):
    return hash(self.get_name())

Named.register(NameComparableMixin)
collections.abc.Hashable.register(NameComparableMixin)

class DumpMixin(metaclass=abc.ABCMeta):
  """
  A mixin to add support for dumping the string representation of an object to a
  file. Requires the get_file_name(self) method be defined.
  """

  @abc.abstractmethod
  def __str__(self):
    """
    Returns the data to be printed to a file by dump.
    """
    pass

  def dump(self, directory):
    """
    Dump this object to a file in the given directory
    """
    out_file = directory / self.get_file_name()
    if out_file.exists():
      out_file.unlink()
    with out_file.open('w') as out:
      print(str(self), file=out)

FileLike.register(DumpMixin)
