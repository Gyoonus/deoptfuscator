#!/usr/bin/env python
#
# Copyright (C) 2014 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Script that parses a trace filed produced in streaming mode. The file is broken up into
   a header and body part, which, when concatenated, make up a non-streaming trace file that
   can be used with traceview."""

import sys

class MyException(Exception):
  pass

class BufferUnderrun(Exception):
  pass

def ReadShortLE(f):
  byte1 = f.read(1)
  if not byte1:
    raise BufferUnderrun()
  byte2 = f.read(1)
  if not byte2:
    raise BufferUnderrun()
  return ord(byte1) + (ord(byte2) << 8);

def WriteShortLE(f, val):
  bytes = [ (val & 0xFF), ((val >> 8) & 0xFF) ]
  asbytearray = bytearray(bytes)
  f.write(asbytearray)

def ReadIntLE(f):
  byte1 = f.read(1)
  if not byte1:
    raise BufferUnderrun()
  byte2 = f.read(1)
  if not byte2:
    raise BufferUnderrun()
  byte3 = f.read(1)
  if not byte3:
    raise BufferUnderrun()
  byte4 = f.read(1)
  if not byte4:
    raise BufferUnderrun()
  return ord(byte1) + (ord(byte2) << 8) + (ord(byte3) << 16) + (ord(byte4) << 24);

def WriteIntLE(f, val):
  bytes = [ (val & 0xFF), ((val >> 8) & 0xFF), ((val >> 16) & 0xFF), ((val >> 24) & 0xFF) ]
  asbytearray = bytearray(bytes)
  f.write(asbytearray)

def Copy(input, output, length):
  buf = input.read(length)
  if len(buf) != length:
    raise BufferUnderrun()
  output.write(buf)

class Rewriter:

  def PrintHeader(self, header):
    header.write('*version\n');
    header.write('3\n');
    header.write('data-file-overflow=false\n');
    header.write('clock=dual\n');
    header.write('vm=art\n');

  def ProcessDataHeader(self, input, body):
    magic = ReadIntLE(input)
    if magic != 0x574f4c53:
      raise MyException("Magic wrong")

    WriteIntLE(body, magic)

    version = ReadShortLE(input)
    if (version & 0xf0) != 0xf0:
      raise MyException("Does not seem to be a streaming trace: %d." % version)
    version = version ^ 0xf0

    if version != 3:
      raise MyException("Only support version 3")

    WriteShortLE(body, version)

    # read offset
    offsetToData = ReadShortLE(input) - 16
    WriteShortLE(body, offsetToData + 16)

    # copy startWhen
    Copy(input, body, 8)

    if version == 1:
      self._mRecordSize = 9;
    elif version == 2:
      self._mRecordSize = 10;
    else:
      self._mRecordSize = ReadShortLE(input)
      WriteShortLE(body, self._mRecordSize)
      offsetToData -= 2;

    # Skip over offsetToData bytes
    Copy(input, body, offsetToData)

  def ProcessMethod(self, input):
    stringLength = ReadShortLE(input)
    str = input.read(stringLength)
    self._methods.append(str)
    print 'New method: %s' % str

  def ProcessThread(self, input):
    tid = ReadShortLE(input)
    stringLength = ReadShortLE(input)
    str = input.read(stringLength)
    self._threads.append('%d\t%s\n' % (tid, str))
    print 'New thread: %d/%s' % (tid, str)

  def ProcessTraceSummary(self, input):
    summaryLength = ReadIntLE(input)
    str = input.read(summaryLength)
    self._summary = str
    print 'Summary: \"%s\"' % str

  def ProcessSpecial(self, input):
    code = ord(input.read(1))
    if code == 1:
      self.ProcessMethod(input)
    elif code == 2:
      self.ProcessThread(input)
    elif code == 3:
      self.ProcessTraceSummary(input)
    else:
      raise MyException("Unknown special!")

  def Process(self, input, body):
    try:
      while True:
        threadId = ReadShortLE(input)
        if threadId == 0:
          self.ProcessSpecial(input)
        else:
          # Regular package, just copy
          WriteShortLE(body, threadId)
          Copy(input, body, self._mRecordSize - 2)
    except BufferUnderrun:
      print 'Buffer underrun, file was probably truncated. Results should still be usable.'

  def Finalize(self, header):
    # If the summary is present in the input file, use it as the header except
    # for the methods section which is emtpy in the input file. If not present,
    # apppend header with the threads that are recorded in the input stream.
    if (self._summary):
      # Erase the contents that's already written earlier by PrintHeader.
      header.seek(0)
      header.truncate()
      # Copy the lines from the input summary to the output header until
      # the methods section is seen.
      for line in self._summary.splitlines(True):
        if line == "*methods\n":
          break
        else:
          header.write(line)
    else:
      header.write('*threads\n')
      for t in self._threads:
        header.write(t)
    header.write('*methods\n')
    for m in self._methods:
      header.write(m)
    header.write('*end\n')

  def ProcessFile(self, filename):
    input = open(filename, 'rb')                     # Input file
    header = open(filename + '.header', 'w')         # Header part
    body = open(filename + '.body', 'wb')            # Body part

    self.PrintHeader(header)

    self.ProcessDataHeader(input, body)

    self._methods = []
    self._threads = []
    self._summary = None
    self.Process(input, body)

    self.Finalize(header)

    input.close()
    header.close()
    body.close()

def main():
  Rewriter().ProcessFile(sys.argv[1])
  header_name = sys.argv[1] + '.header'
  body_name = sys.argv[1] + '.body'
  print 'Results have been written to %s and %s.' % (header_name, body_name)
  print 'Concatenate the files to get a result usable with traceview.'
  sys.exit(0)

if __name__ == '__main__':
  main()