FastDirectoryEnumerator
-=-=-=-=-=-=-=-=-=-=-=-
(C) 2013 Niall Douglas

Herein lies C++11 code which enumerates and stat()'s very, very large directories quickly
by directly using kernel syscalls. For POSIX and Windows.

Differences from struct stat:

* st_type contains the S_IF* flags part of the st_mode field only. st_mode is the same as stat.
* st_allocated is st_blksize multiplied by st_blocks.

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

Therefore fields missing from enumeration are:

* st_nlink
* st_blocks
* st_blksize

Fetching these invoke a slow code path: Windows gives you no choice but to open a handle
to the file being queried, something which is very slow on NT. Expect a max of 30k slow
path queries per second as Windows can only open 30k HANDLEs per second.

There is something else you should be aware of: class directory_entry optimises same-directory
metadata lookups by caching per-thread a handle to the containing directory. Given that
metadata lookups tend to be from the same directory, and assuming that none of the slow
code path fields above have been requested, directory_entry can fetch metadata by doing
a globbed enumeration instead, yielding about 250k lookups per second max. As soon as you
change contained directory, or ask for any slow code path fields, expect 30k lookups per
second max.

Some quick benchmarks:
----------------------

Intel Core i7-3770K CPU @ 3.50Ghz:

Windows 8 x64, NTFS: approx 1.8m entries enumerated per second max (i.e. from RAM cache)
with common stat fields listed above. Makes little difference if SSD or conventional magnetic.
Note that setting namesonly=true is actually a bit slower, only 1.5m per second max. I assume
this is due to an less optimised code path.

Linux 3.2 x64, ext4: approx 2.5m entries enumerated per second max (i.e. from RAM cache)
with st_ino and st_type fields only on SSD. lstat() can do 1.1m entries per sec max to
fill in the remaining fields also on SSD.
