FastDirectoryEnumerator
-=-=-=-=-=-=-=-=-=-=-=-
(C) 2013 Niall Douglas

Herein lies C++11 code which enumerates very, very large directories quickly
by directly using kernel syscalls. For POSIX and Windows.

On POSIX directly uses the getdents() syscall (http://man7.org/linux/man-pages/man2/getdents.2.html).
This syscall returns the leafname and st_rdev fields only.

On Windows directly uses the NtQueryDirectoryFile() syscall (http://msdn.microsoft.com/en-us/library/windows/hardware/ff567047(v=vs.85).aspx).
This syscall returns the leafname and the following stat fields:

* st_ino (yes NTFS does provide a unique POSIX compliant inode number at the kernel level, it's 64 bits though)
* st_rdev
* st_atimespec
* st_mtimespec
* st_ctimespec (yes, NTFS also provides this at the kernel level)
* st_size
* st_allocated (this is st_blksize * st_blocks)
* st_birthtimespec


Some quick benchmarks:
----------------------

Intel Core i7-3770K CPU @ 3.50Ghz:

Windows 8 x64, NTFS with most common members of struct stat: approx 1.5m
entries enumerated per second max (i.e. from RAM cache). Makes little difference
if SSD or conventional magnetic.

Linux 3.2 x64, ext4: to come.
