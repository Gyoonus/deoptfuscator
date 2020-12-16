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

from common.logger import Logger
from common.mixins import PrintableMixin

class C1visualizerFile(PrintableMixin):

  def __init__(self, fileName):
    self.fileName = fileName
    self.passes = []

  def addPass(self, new_pass):
    self.passes.append(new_pass)

  def findPass(self, name):
    for entry in self.passes:
      if entry.name == name:
        return entry
    return None

  def __eq__(self, other):
    return isinstance(other, self.__class__) \
       and self.passes == other.passes


class C1visualizerPass(PrintableMixin):

  def __init__(self, parent, name, body, startLineNo):
    self.parent = parent
    self.name = name
    self.body = body
    self.startLineNo = startLineNo

    if not self.name:
      Logger.fail("C1visualizer pass does not have a name", self.fileName, self.startLineNo)
    if not self.body:
      Logger.fail("C1visualizer pass does not have a body", self.fileName, self.startLineNo)

    self.parent.addPass(self)

  @property
  def fileName(self):
    return self.parent.fileName

  def __eq__(self, other):
    return isinstance(other, self.__class__) \
       and self.name == other.name \
       and self.body == other.body
