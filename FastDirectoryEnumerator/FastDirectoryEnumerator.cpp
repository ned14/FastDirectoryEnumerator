/* FastDirectoryEnumerator
Enumerates very, very large directories quickly by directly using kernel syscalls. For POSIX and Windows.
(C) 2013 Niall Douglas http://www.nedprod.com/
File created: Aug 2013
*/

#define __USE_XOPEN2K8 // Turns on timespecs in Linux
#include "FastDirectoryEnumerator.hpp"
#include "Undoer.hpp"
#include <sys/stat.h>
#ifdef WIN32
#include <Windows.h>
#else
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <dirent.h>     /* Defines DT_* constants */
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/syscall.h>
#endif

namespace FastDirectoryEnumerator
{

#ifdef WIN32
#define TIMESPEC_TO_FILETIME_OFFSET (((long long)27111902 << 32) + (long long)3577643008)

static inline struct timespec to_timespec(LARGE_INTEGER time)
{
	struct timespec ts;
	ts.tv_sec = ((time.QuadPart - TIMESPEC_TO_FILETIME_OFFSET) / 10000000);
	ts.tv_nsec = (long)((time.QuadPart - TIMESPEC_TO_FILETIME_OFFSET - ((long long)ts.tv_sec * (long long)10000000)) * 100);
	return ts;
}
#endif

have_metadata_flags directory_entry::metadata_supported() BOOST_NOEXCEPT_OR_NOTHROW
{
	have_metadata_flags ret; ret.value=0;
#ifdef WIN32
	ret.have_dev=0;
	ret.have_ino=1;
	ret.have_type=1;
	ret.have_mode=0;
	ret.have_nlink=1;
	ret.have_uid=0;
	ret.have_gid=0;
	ret.have_rdev=0;
	ret.have_atim=1;
	ret.have_mtim=1;
	ret.have_ctim=1;
	ret.have_size=1;
	ret.have_allocated=1;
	ret.have_blocks=0;
	ret.have_blksize=0;
	ret.have_flags=0;
	ret.have_gen=1;
	ret.have_birthtim=1;
#elif defined(__linux__)
	ret.have_dev=1;
	ret.have_ino=1;
	ret.have_type=1;
	ret.have_mode=1;
	ret.have_nlink=1;
	ret.have_uid=1;
	ret.have_gid=1;
	ret.have_rdev=1;
	ret.have_atim=1;
	ret.have_mtim=1;
	ret.have_ctim=1;
	ret.have_size=1;
	ret.have_allocated=1;
	ret.have_blocks=1;
	ret.have_blksize=1;
	// Sadly these must wait until someone fixes the Linux stat() call e.g. the xstat() proposal.
	ret.have_flags=0;
	ret.have_gen=0;
	ret.have_birthtim=0;
	// According to http://computer-forensics.sans.org/blog/2011/03/14/digital-forensics-understanding-ext4-part-2-timestamps
	// ext4 keeps birth time at offset 144 to 151 in the inode. If we ever got round to it, birthtime could be hacked.
#else
	// Kinda assumes FreeBSD or OS X really ...
	ret.have_dev=1;
	ret.have_ino=1;
	ret.have_type=1;
	ret.have_mode=1;
	ret.have_nlink=1;
	ret.have_uid=1;
	ret.have_gid=1;
	ret.have_rdev=1;
	ret.have_atim=1;
	ret.have_mtim=1;
	ret.have_ctim=1;
	ret.have_size=1;
	ret.have_allocated=1;
	ret.have_blocks=1;
	ret.have_blksize=1;
#define HAVE_STAT_FLAGS
	ret.have_flags=1;
#define HAVE_STAT_GEN
	ret.have_gen=1;
#define HAVE_BIRTHTIMESPEC
	ret.have_birthtim=1;
#endif
	return ret;
}

void directory_entry::_int_fetch(have_metadata_flags wanted, std::filesystem::path prefix)
{
	prefix/=leafname;
#ifdef WIN32
	throw std::runtime_error("Not implemented yet");
#else
	struct stat s={0};
	if(-1!=lstat(prefix.c_str(), &s))
	{
		if(wanted.have_dev) { stat.st_dev=s.st_dev; have_metadata.have_dev=1; }
		if(wanted.have_ino) { stat.st_ino=s.st_ino; have_metadata.have_ino=1; }
		if(wanted.have_type) { stat.st_type=s.st_mode; have_metadata.have_type=1; }
		if(wanted.have_mode) { stat.st_mode=s.st_mode; have_metadata.have_mode=1; }
		if(wanted.have_nlink) { stat.st_nlink=s.st_nlink; have_metadata.have_nlink=1; }
		if(wanted.have_uid) { stat.st_uid=s.st_uid; have_metadata.have_uid=1; }
		if(wanted.have_gid) { stat.st_gid=s.st_gid; have_metadata.have_gid=1; }
		if(wanted.have_rdev) { stat.st_rdev=s.st_rdev; have_metadata.have_rdev=1; }
		if(wanted.have_atim) { stat.st_atim.tv_sec=s.st_atim.tv_sec; stat.st_atim.tv_nsec=s.st_atim.tv_nsec; have_metadata.have_atim=1; }
		if(wanted.have_mtim) { stat.st_mtim.tv_sec=s.st_mtim.tv_sec; stat.st_mtim.tv_nsec=s.st_mtim.tv_nsec; have_metadata.have_mtim=1; }
		if(wanted.have_ctim) { stat.st_ctim.tv_sec=s.st_ctim.tv_sec; stat.st_ctim.tv_nsec=s.st_ctim.tv_nsec; have_metadata.have_ctim=1; }
		if(wanted.have_size) { stat.st_size=s.st_size; have_metadata.have_size=1; }
		if(wanted.have_allocated) { stat.st_allocated=s.st_blocks*s.st_blksize; have_metadata.have_allocated=1; }
		if(wanted.have_blocks) { stat.st_blocks=s.st_blocks; have_metadata.have_blocks=1; }
		if(wanted.have_blksize) { stat.st_blksize=s.st_blksize; have_metadata.have_blksize=1; }
#ifdef HAVE_STAT_FLAGS
		if(wanted.have_flags) { stat.st_flags=s.st_flags; have_metadata.have_flags=1; }
#endif
#ifdef HAVE_STAT_GEN
		if(wanted.have_gen) { stat.st_gen=s.st_gen; have_metadata.have_gen=1; }
#endif
#ifdef HAVE_BIRTHTIMESPEC
		if(wanted.have_birthtim) { stat.st_birthtim.tv_sec=s.st_birthtim.tv_sec; stat.st_birthtim.tv_nsec=s.st_birthtim.tv_nsec; have_metadata.have_birthtim=1; }
#endif
	}
#endif
}

void *begin_enumerate_directory(std::filesystem::path path)
{
#ifdef WIN32
	HANDLE ret=CreateFile(path.c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, NULL,
		OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
	return (INVALID_HANDLE_VALUE==ret) ? 0 : ret;
#else
	int ret=open(path.c_str(), O_RDONLY | O_DIRECTORY);
	return (-1==ret) ? 0 : (void *)(size_t)ret;
#endif
}

void end_enumerate_directory(void *h)
{
#ifdef WIN32
	CloseHandle(h);
#else
	close((int)(size_t)h);
#endif
}

std::unique_ptr<std::vector<directory_entry>> enumerate_directory(void *h, size_t maxitems, std::filesystem::path glob, bool namesonly)
{
	std::unique_ptr<std::vector<directory_entry>> ret;
#ifdef WIN32
	// From http://undocumented.ntinternals.net/UserMode/Undocumented%20Functions/NT%20Objects/File/FILE_INFORMATION_CLASS.html
	typedef enum _FILE_INFORMATION_CLASS {
		FileDirectoryInformation                 = 1,
		FileFullDirectoryInformation,
		FileBothDirectoryInformation,
		FileBasicInformation,
		FileStandardInformation,
		FileInternalInformation,
		FileEaInformation,
		FileAccessInformation,
		FileNameInformation,
		FileRenameInformation,
		FileLinkInformation,
		FileNamesInformation,
		FileDispositionInformation,
		FilePositionInformation,
		FileFullEaInformation,
		FileModeInformation,
		FileAlignmentInformation,
		FileAllInformation,
		FileAllocationInformation,
		FileEndOfFileInformation,
		FileAlternateNameInformation,
		FileStreamInformation,
		FilePipeInformation,
		FilePipeLocalInformation,
		FilePipeRemoteInformation,
		FileMailslotQueryInformation,
		FileMailslotSetInformation,
		FileCompressionInformation,
		FileObjectIdInformation,
		FileCompletionInformation,
		FileMoveClusterInformation,
		FileQuotaInformation,
		FileReparsePointInformation,
		FileNetworkOpenInformation,
		FileAttributeTagInformation,
		FileTrackingInformation,
		FileIdBothDirectoryInformation,
		FileIdFullDirectoryInformation,
		FileValidDataLengthInformation,
		FileShortNameInformation,
		FileIoCompletionNotificationInformation,
		FileIoStatusBlockRangeInformation,
		FileIoPriorityHintInformation,
		FileSfioReserveInformation,
		FileSfioVolumeInformation,
		FileHardLinkInformation,
		FileProcessIdsUsingFileInformation,
		FileNormalizedNameInformation,
		FileNetworkPhysicalNameInformation,
		FileIdGlobalTxDirectoryInformation,
		FileIsRemoteDeviceInformation,
		FileAttributeCacheInformation,
		FileNumaNodeInformation,
		FileStandardLinkInformation,
		FileRemoteProtocolInformation,
		FileMaximumInformation
	} FILE_INFORMATION_CLASS, *PFILE_INFORMATION_CLASS;

	// From http://msdn.microsoft.com/en-us/library/windows/hardware/ff540310(v=vs.85).aspx
	typedef struct _FILE_ID_FULL_DIR_INFORMATION  {
	  ULONG         NextEntryOffset;
	  ULONG         FileIndex;
	  LARGE_INTEGER CreationTime;
	  LARGE_INTEGER LastAccessTime;
	  LARGE_INTEGER LastWriteTime;
	  LARGE_INTEGER ChangeTime;
	  LARGE_INTEGER EndOfFile;
	  LARGE_INTEGER AllocationSize;
	  ULONG         FileAttributes;
	  ULONG         FileNameLength;
	  ULONG         EaSize;
	  LARGE_INTEGER FileId;
	  WCHAR         FileName[1];
	} FILE_ID_FULL_DIR_INFORMATION, *PFILE_ID_FULL_DIR_INFORMATION;

	// From http://msdn.microsoft.com/en-us/library/windows/hardware/ff540329(v=vs.85).aspx
	typedef struct _FILE_NAMES_INFORMATION {
	  ULONG NextEntryOffset;
	  ULONG FileIndex;
	  ULONG FileNameLength;
	  WCHAR FileName[1];
	} FILE_NAMES_INFORMATION, *PFILE_NAMES_INFORMATION;

#ifndef NTSTATUS
#define NTSTATUS LONG
#endif

	// From http://msdn.microsoft.com/en-us/library/windows/hardware/ff550671(v=vs.85).aspx
	typedef struct _IO_STATUS_BLOCK {
		union {
			NTSTATUS Status;
			PVOID    Pointer;
		};
		ULONG_PTR Information;
	} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;

	// From http://msdn.microsoft.com/en-us/library/windows/desktop/aa380518(v=vs.85).aspx
	typedef struct _LSA_UNICODE_STRING {
	  USHORT Length;
	  USHORT MaximumLength;
	  PWSTR  Buffer;
	} LSA_UNICODE_STRING, *PLSA_UNICODE_STRING, UNICODE_STRING, *PUNICODE_STRING;

	// From http://undocumented.ntinternals.net/UserMode/Undocumented%20Functions/NT%20Objects/File/NtQueryDirectoryFile.html
	// and http://msdn.microsoft.com/en-us/library/windows/hardware/ff567047(v=vs.85).aspx
	typedef NTSTATUS (NTAPI *NtQueryDirectoryFile_t)(
		/*_In_*/      HANDLE FileHandle,
		/*_In_opt_*/  HANDLE Event,
		/*_In_opt_*/  void *ApcRoutine,
		/*_In_opt_*/  PVOID ApcContext,
		/*_Out_*/     PIO_STATUS_BLOCK IoStatusBlock,
		/*_Out_*/     PVOID FileInformation,
		/*_In_*/      ULONG Length,
		/*_In_*/      FILE_INFORMATION_CLASS FileInformationClass,
		/*_In_*/      BOOLEAN ReturnSingleEntry,
		/*_In_opt_*/  PUNICODE_STRING FileName,
		/*_In_*/      BOOLEAN RestartScan
		);

	static NtQueryDirectoryFile_t NtQueryDirectoryFile;
	if(!NtQueryDirectoryFile)
		if(!(NtQueryDirectoryFile=(NtQueryDirectoryFile_t) GetProcAddress(GetModuleHandleA("NTDLL.DLL"), "NtQueryDirectoryFile")))
			abort();

	IO_STATUS_BLOCK isb={ 0 };
	UNICODE_STRING _glob;
	if(!glob.empty())
	{
		_glob.Buffer=const_cast<std::filesystem::path::value_type *>(glob.c_str());
		_glob.Length=_glob.MaximumLength=(USHORT) glob.native().size();
	}

	if(namesonly)
	{
		FILE_NAMES_INFORMATION *buffer=(FILE_NAMES_INFORMATION *) malloc(sizeof(FILE_NAMES_INFORMATION)*maxitems);
		if(!buffer) throw std::bad_alloc();
		auto unbuffer=detail::Undoer([&buffer] { free(buffer); });

		if(0/*STATUS_SUCCESS*/!=NtQueryDirectoryFile(h, NULL, NULL, NULL, &isb, buffer, (ULONG)(sizeof(FILE_NAMES_INFORMATION)*maxitems),
			FileNamesInformation, FALSE, glob.empty() ? NULL : &_glob, FALSE))
		{
			return ret;
		}
		ret=std::unique_ptr<std::vector<directory_entry>>(new std::vector<directory_entry>);
		std::vector<directory_entry> &_ret=*ret;
		_ret.reserve(maxitems);
		directory_entry item;
		bool done=false;
		for(FILE_NAMES_INFORMATION *ffdi=buffer; !done; ffdi=(FILE_NAMES_INFORMATION *)((size_t) ffdi + ffdi->NextEntryOffset))
		{
			size_t length=ffdi->FileNameLength/sizeof(std::filesystem::path::value_type);
			if(length<=2 && '.'==ffdi->FileName[0])
				if(1==length || '.'==ffdi->FileName[1]) continue;
			std::filesystem::path::string_type leafname(ffdi->FileName, length);
			item.leafname=std::move(leafname);
			_ret.push_back(std::move(item));
			if(!ffdi->NextEntryOffset) done=true;
		}
	}
	else
	{
		FILE_ID_FULL_DIR_INFORMATION *buffer=(FILE_ID_FULL_DIR_INFORMATION *) malloc(sizeof(FILE_ID_FULL_DIR_INFORMATION)*maxitems);
		if(!buffer) throw std::bad_alloc();
		auto unbuffer=detail::Undoer([&buffer] { free(buffer); });

		if(0/*STATUS_SUCCESS*/!=NtQueryDirectoryFile(h, NULL, NULL, NULL, &isb, buffer, (ULONG)(sizeof(FILE_ID_FULL_DIR_INFORMATION)*maxitems),
			FileIdFullDirectoryInformation, FALSE, glob.empty() ? NULL : &_glob, FALSE))
		{
			return ret;
		}
		ret=std::unique_ptr<std::vector<directory_entry>>(new std::vector<directory_entry>);
		std::vector<directory_entry> &_ret=*ret;
		_ret.reserve(maxitems);
		directory_entry item;
		// This is what windows returns with each enumeration
		item.have_metadata.have_ino=1;
		item.have_metadata.have_type=1;
		item.have_metadata.have_atim=1;
		item.have_metadata.have_mtim=1;
		item.have_metadata.have_ctim=1;
		item.have_metadata.have_size=1;
		item.have_metadata.have_allocated=1;
		item.have_metadata.have_birthtim=1;
		bool done=false;
		for(FILE_ID_FULL_DIR_INFORMATION *ffdi=buffer; !done; ffdi=(FILE_ID_FULL_DIR_INFORMATION *)((size_t) ffdi + ffdi->NextEntryOffset))
		{
			size_t length=ffdi->FileNameLength/sizeof(std::filesystem::path::value_type);
			if(length<=2 && '.'==ffdi->FileName[0])
				if(1==length || '.'==ffdi->FileName[1]) continue;
			std::filesystem::path::string_type leafname(ffdi->FileName, length);
			item.leafname=std::move(leafname);
			item.stat.st_ino=ffdi->FileId.QuadPart;
			item.stat.st_type=(ffdi->FileAttributes&FILE_ATTRIBUTE_DIRECTORY) ? S_IFDIR : S_IFREG;
			item.stat.st_atim=to_timespec(ffdi->LastAccessTime);
			item.stat.st_mtim=to_timespec(ffdi->LastWriteTime);
			item.stat.st_ctim=to_timespec(ffdi->ChangeTime);
			item.stat.st_size=ffdi->EndOfFile.QuadPart;
			item.stat.st_allocated=ffdi->AllocationSize.QuadPart;
			item.stat.st_birthtim=to_timespec(ffdi->CreationTime);
			_ret.push_back(std::move(item));
			if(!ffdi->NextEntryOffset) done=true;
		}
	}
	return ret;
#else
#ifdef __linux__
	// Linux kernel defines a weird dirent with type packed at the end of d_name, so override default dirent
	struct dirent {
	   long           d_ino;
	   off_t          d_off;
	   unsigned short d_reclen;
	   char           d_name[];
	};
	// Unlike FreeBSD, Linux doesn't define a getdents() function, so we'll do that here.
	typedef int (*getdents_t)(int, char *, int);
	getdents_t getdents=(getdents_t)([](int fd, char *buf, int count) -> int { return syscall(SYS_getdents, fd, buf, count); });
#endif
	dirent *buffer=(dirent *) malloc(sizeof(dirent)*maxitems);
	if(!buffer) throw std::bad_alloc();
	auto unbuffer=detail::Undoer([&buffer] { free(buffer); });

	int bytes=getdents((int)(size_t)h, (char *) buffer, sizeof(dirent)*maxitems);
	if(bytes<=0)
		return ret;
	ret=std::unique_ptr<std::vector<directory_entry>>(new std::vector<directory_entry>);
	std::vector<directory_entry> &_ret=*ret;
	_ret.reserve(maxitems);
	directory_entry item;
	// This is what POSIX returns with getdents()
	item.have_metadata.have_ino=1;
	item.have_metadata.have_type=1;
	bool done=false;
	for(dirent *dent=buffer; !done; dent=(dirent *)((size_t) dent + dent->d_reclen))
	{
		if(!(bytes-=dent->d_reclen)) done=true;
		if(!dent->d_ino)
			continue;
		size_t length=strchr(dent->d_name, 0)-dent->d_name;
		if(length<=2 && '.'==dent->d_name[0])
			if(1==length || '.'==dent->d_name[1]) continue;
		std::filesystem::path::string_type leafname(dent->d_name, length);
		item.leafname=std::move(leafname);
		item.stat.st_ino=dent->d_ino;
		char d_type=
#ifdef __linux__
				*((char *) dent + dent->d_reclen - 1)
#else
				dent->d_type
#endif
				;
		if(DT_UNKNOWN==d_type)
			item.have_metadata.have_type=0;
		else
		{
			item.have_metadata.have_type=1;
			switch(d_type)
			{
			case DT_BLK:
				item.stat.st_type=S_IFBLK;
				break;
			case DT_CHR:
				item.stat.st_type=S_IFCHR;
				break;
			case DT_DIR:
				item.stat.st_type=S_IFDIR;
				break;
			case DT_FIFO:
				item.stat.st_type=S_IFIFO;
				break;
			case DT_LNK:
				item.stat.st_type=S_IFLNK;
				break;
			case DT_REG:
				item.stat.st_type=S_IFREG;
				break;
			case DT_SOCK:
				item.stat.st_type=S_IFSOCK;
				break;
			default:
				item.have_metadata.have_type=0;
				item.stat.st_type=0;
				break;
			}
		}
		_ret.push_back(std::move(item));
	}
	return ret;
#endif
}

} // namespace
