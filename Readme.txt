FastDirectoryEnumerator
-=-=-=-=-=-=-=-=-=-=-=-
(C) 2013 Niall Douglas

Herein lies C++11 code which enumerates very, very large directories quickly
by directly using kernel syscalls. For POSIX and Windows.

Note that st_type contains the S_IF* flags part of the st_mode field only. st_mode is the same as stat.
st_allocated is st_blksize multiplied by st_blocks and is also available on Windows.

On POSIX directly uses the getdents() syscall (http://man7.org/linux/man-pages/man2/getdents.2.html).
This syscall returns the leafname, st_ino and st_type fields only.

On Windows directly uses the NtQueryDirectoryFile() syscall (http://msdn.microsoft.com/en-us/library/windows/hardware/ff567047(v=vs.85).aspx).
This syscall returns the leafname and the following stat fields:

* st_ino (yes NTFS does provide a unique POSIX compliant inode number at the kernel level, it's 64 bits though)
* st_type
* st_atimespec
* st_mtimespec
* st_ctimespec (yes, NTFS also provides this at the kernel level)
* st_size
* st_allocated (this is st_blksize * st_blocks)
* st_birthtimespec


Some quick benchmarks:
----------------------

Intel Core i7-3770K CPU @ 3.50Ghz:

Windows 8 x64, NTFS: approx 1.5m entries enumerated per second max (i.e. from RAM cache)
with common stat fields listed above. Makes little difference if SSD or conventional magnetic.
Note that setting namesonly=true is actually much slower, only 1m per second max. I assume
this is due to an less optimised code path.

Linux 3.2 x64, ext4: approx 2.5m entries enumerated per second max (i.e. from RAM cache)
with st_ino and st_type fields only on SSD. lstat() can do 1.1m entries per sec max to
fill in the remaining fields also on SSD.
