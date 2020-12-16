/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "base/unix_file/fd_file.h"
#include "base/unix_file/random_access_file_test.h"
#include "common_runtime_test.h"  // For ScratchFile
#include "gtest/gtest.h"

namespace unix_file {

class FdFileTest : public RandomAccessFileTest {
 protected:
  virtual RandomAccessFile* MakeTestFile() {
    return new FdFile(fileno(tmpfile()), false);
  }
};

TEST_F(FdFileTest, Read) {
  TestRead();
}

TEST_F(FdFileTest, SetLength) {
  TestSetLength();
}

TEST_F(FdFileTest, Write) {
  TestWrite();
}

TEST_F(FdFileTest, UnopenedFile) {
  FdFile file;
  EXPECT_EQ(-1, file.Fd());
  EXPECT_FALSE(file.IsOpened());
  EXPECT_TRUE(file.GetPath().empty());
}

TEST_F(FdFileTest, OpenClose) {
  std::string good_path(GetTmpPath("some-file.txt"));
  FdFile file(good_path, O_CREAT | O_WRONLY, true);
  ASSERT_TRUE(file.IsOpened());
  EXPECT_GE(file.Fd(), 0);
  EXPECT_TRUE(file.IsOpened());
  EXPECT_FALSE(file.ReadOnlyMode());
  EXPECT_EQ(0, file.Flush());
  EXPECT_EQ(0, file.Close());
  EXPECT_EQ(-1, file.Fd());
  EXPECT_FALSE(file.IsOpened());
  FdFile file2(good_path, O_RDONLY, true);
  EXPECT_TRUE(file2.IsOpened());
  EXPECT_TRUE(file2.ReadOnlyMode());
  EXPECT_GE(file2.Fd(), 0);

  ASSERT_EQ(file2.Close(), 0);
  ASSERT_EQ(unlink(good_path.c_str()), 0);
}

TEST_F(FdFileTest, ReadFullyEmptyFile) {
  // New scratch file, zero-length.
  art::ScratchFile tmp;
  FdFile file(tmp.GetFilename(), O_RDONLY, false);
  ASSERT_TRUE(file.IsOpened());
  EXPECT_TRUE(file.ReadOnlyMode());
  EXPECT_GE(file.Fd(), 0);
  uint8_t buffer[16];
  EXPECT_FALSE(file.ReadFully(&buffer, 4));
}

template <size_t Size>
static void NullTerminateCharArray(char (&array)[Size]) {
  array[Size - 1] = '\0';
}

TEST_F(FdFileTest, ReadFullyWithOffset) {
  // New scratch file, zero-length.
  art::ScratchFile tmp;
  FdFile file(tmp.GetFilename(), O_RDWR, false);
  ASSERT_TRUE(file.IsOpened());
  EXPECT_GE(file.Fd(), 0);
  EXPECT_FALSE(file.ReadOnlyMode());

  char ignore_prefix[20] = {'a', };
  NullTerminateCharArray(ignore_prefix);
  char read_suffix[10] = {'b', };
  NullTerminateCharArray(read_suffix);

  off_t offset = 0;
  // Write scratch data to file that we can read back into.
  EXPECT_TRUE(file.Write(ignore_prefix, sizeof(ignore_prefix), offset));
  offset += sizeof(ignore_prefix);
  EXPECT_TRUE(file.Write(read_suffix, sizeof(read_suffix), offset));

  ASSERT_EQ(file.Flush(), 0);

  // Reading at an offset should only produce 'bbbb...', since we ignore the 'aaa...' prefix.
  char buffer[sizeof(read_suffix)];
  EXPECT_TRUE(file.PreadFully(buffer, sizeof(read_suffix), offset));
  EXPECT_STREQ(&read_suffix[0], &buffer[0]);

  ASSERT_EQ(file.Close(), 0);
}

TEST_F(FdFileTest, ReadWriteFullyWithOffset) {
  // New scratch file, zero-length.
  art::ScratchFile tmp;
  FdFile file(tmp.GetFilename(), O_RDWR, false);
  ASSERT_GE(file.Fd(), 0);
  EXPECT_TRUE(file.IsOpened());
  EXPECT_FALSE(file.ReadOnlyMode());

  const char* test_string = "This is a test string";
  size_t length = strlen(test_string) + 1;
  const size_t offset = 12;
  std::unique_ptr<char[]> offset_read_string(new char[length]);
  std::unique_ptr<char[]> read_string(new char[length]);

  // Write scratch data to file that we can read back into.
  EXPECT_TRUE(file.PwriteFully(test_string, length, offset));
  ASSERT_EQ(file.Flush(), 0);

  // Test reading both the offsets.
  EXPECT_TRUE(file.PreadFully(&offset_read_string[0], length, offset));
  EXPECT_STREQ(test_string, &offset_read_string[0]);

  EXPECT_TRUE(file.PreadFully(&read_string[0], length, 0u));
  EXPECT_NE(memcmp(&read_string[0], test_string, length), 0);

  ASSERT_EQ(file.Close(), 0);
}

TEST_F(FdFileTest, Copy) {
  art::ScratchFile src_tmp;
  FdFile src(src_tmp.GetFilename(), O_RDWR, false);
  ASSERT_GE(src.Fd(), 0);
  ASSERT_TRUE(src.IsOpened());

  char src_data[] = "Some test data.";
  ASSERT_TRUE(src.WriteFully(src_data, sizeof(src_data)));  // Including the zero terminator.
  ASSERT_EQ(0, src.Flush());
  ASSERT_EQ(static_cast<int64_t>(sizeof(src_data)), src.GetLength());

  art::ScratchFile dest_tmp;
  FdFile dest(src_tmp.GetFilename(), O_RDWR, false);
  ASSERT_GE(dest.Fd(), 0);
  ASSERT_TRUE(dest.IsOpened());

  ASSERT_TRUE(dest.Copy(&src, 0, sizeof(src_data)));
  ASSERT_EQ(0, dest.Flush());
  ASSERT_EQ(static_cast<int64_t>(sizeof(src_data)), dest.GetLength());

  char check_data[sizeof(src_data)];
  ASSERT_TRUE(dest.PreadFully(check_data, sizeof(src_data), 0u));
  CHECK_EQ(0, memcmp(check_data, src_data, sizeof(src_data)));

  ASSERT_EQ(0, dest.Close());
  ASSERT_EQ(0, src.Close());
}

TEST_F(FdFileTest, MoveConstructor) {
  // New scratch file, zero-length.
  art::ScratchFile tmp;
  FdFile file(tmp.GetFilename(), O_RDWR, false);
  ASSERT_TRUE(file.IsOpened());
  EXPECT_GE(file.Fd(), 0);

  int old_fd = file.Fd();

  FdFile file2(std::move(file));
  EXPECT_FALSE(file.IsOpened());
  EXPECT_TRUE(file2.IsOpened());
  EXPECT_EQ(old_fd, file2.Fd());

  ASSERT_EQ(file2.Flush(), 0);
  ASSERT_EQ(file2.Close(), 0);
}

TEST_F(FdFileTest, OperatorMoveEquals) {
  // Make sure the read_only_ flag is correctly copied
  // over.
  art::ScratchFile tmp;
  FdFile file(tmp.GetFilename(), O_RDONLY, false);
  ASSERT_TRUE(file.ReadOnlyMode());

  FdFile file2(tmp.GetFilename(), O_RDWR, false);
  ASSERT_FALSE(file2.ReadOnlyMode());

  file2 = std::move(file);
  ASSERT_TRUE(file2.ReadOnlyMode());
}

TEST_F(FdFileTest, EraseWithPathUnlinks) {
  // New scratch file, zero-length.
  art::ScratchFile tmp;
  std::string filename = tmp.GetFilename();
  tmp.Close();  // This is required because of the unlink race between the scratch file and the
                // FdFile, which leads to close-guard breakage.
  FdFile file(filename, O_RDWR, false);
  ASSERT_TRUE(file.IsOpened());
  EXPECT_GE(file.Fd(), 0);
  uint8_t buffer[16] = { 0 };
  EXPECT_TRUE(file.WriteFully(&buffer, sizeof(buffer)));
  EXPECT_EQ(file.Flush(), 0);

  EXPECT_TRUE(file.Erase(true));

  EXPECT_FALSE(file.IsOpened());

  EXPECT_FALSE(art::OS::FileExists(filename.c_str())) << filename;
}

TEST_F(FdFileTest, Compare) {
  std::vector<uint8_t> buffer;
  constexpr int64_t length = 17 * art::KB;
  for (size_t i = 0; i < length; ++i) {
    buffer.push_back(static_cast<uint8_t>(i));
  }

  auto reset_compare = [&](art::ScratchFile& a, art::ScratchFile& b) {
    a.GetFile()->ResetOffset();
    b.GetFile()->ResetOffset();
    return a.GetFile()->Compare(b.GetFile());
  };

  art::ScratchFile tmp;
  EXPECT_TRUE(tmp.GetFile()->WriteFully(&buffer[0], length));
  EXPECT_EQ(tmp.GetFile()->GetLength(), length);

  art::ScratchFile tmp2;
  EXPECT_TRUE(tmp2.GetFile()->WriteFully(&buffer[0], length));
  EXPECT_EQ(tmp2.GetFile()->GetLength(), length);

  // Basic equality check.
  tmp.GetFile()->ResetOffset();
  tmp2.GetFile()->ResetOffset();
  // Files should be the same.
  EXPECT_EQ(reset_compare(tmp, tmp2), 0);

  // Change a byte near the start.
  ++buffer[2];
  art::ScratchFile tmp3;
  EXPECT_TRUE(tmp3.GetFile()->WriteFully(&buffer[0], length));
  --buffer[2];
  EXPECT_NE(reset_compare(tmp, tmp3), 0);

  // Change a byte near the middle.
  ++buffer[length / 2];
  art::ScratchFile tmp4;
  EXPECT_TRUE(tmp4.GetFile()->WriteFully(&buffer[0], length));
  --buffer[length / 2];
  EXPECT_NE(reset_compare(tmp, tmp4), 0);

  // Change a byte near the end.
  ++buffer[length - 5];
  art::ScratchFile tmp5;
  EXPECT_TRUE(tmp5.GetFile()->WriteFully(&buffer[0], length));
  --buffer[length - 5];
  EXPECT_NE(reset_compare(tmp, tmp5), 0);

  // Reference check
  art::ScratchFile tmp6;
  EXPECT_TRUE(tmp6.GetFile()->WriteFully(&buffer[0], length));
  EXPECT_EQ(reset_compare(tmp, tmp6), 0);
}

TEST_F(FdFileTest, PipeFlush) {
  int pipefd[2];
  ASSERT_EQ(0, pipe2(pipefd, O_CLOEXEC));

  FdFile file(pipefd[1], true);
  ASSERT_TRUE(file.WriteFully("foo", 3));
  ASSERT_EQ(0, file.Flush());
  ASSERT_EQ(0, file.FlushCloseOrErase());
  close(pipefd[0]);
}

}  // namespace unix_file
