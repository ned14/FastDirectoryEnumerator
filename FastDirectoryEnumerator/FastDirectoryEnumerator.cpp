/* FastDirectoryEnumerator
Fixme
(C) 2013 Niall Douglas http://www.nedprod.com/
File created: Aug 2013
*/

#include "FastDirectoryEnumerator.hpp"
#include "Undoer.hpp"
#include <sys/stat.h>
#ifdef WIN32
#include <Windows.h>
#else
#define _GNU_SOURCE
#include <dirent.h>     /* Defines DT_* constants */
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/syscall.h>
#endif

namespace FastDirectoryEnumerator
{

#define TIMESPEC_TO_FILETIME_OFFSET (((long long)27111902 << 32) + (long long)3577643008)

static inline struct timespec to_timespec(LARGE_INTEGER time)
{
	struct timespec ts;
	ts.tv_sec = ((time.QuadPart - TIMESPEC_TO_FILETIME_OFFSET) / 10000000);
	ts.tv_nsec = (long)((time.QuadPart - TIMESPEC_TO_FILETIME_OFFSET - ((long long)ts.tv_sec * (long long)10000000)) * 100);
	return ts;
}

void directory_entry::_int_fetch(have_metadata_flags wanted)
{
	throw std::runtime_error("Not implemented yet");
}

have_metadata_flags directory_entry::metadata_supported() BOOST_NOEXCEPT_OR_NOTHROW
{
	have_metadata_flags ret; ret.value=0;
#ifdef WIN32
	ret.have_dev=0;
	ret.have_ino=1;
	ret.have_mode=0;
	ret.have_nlink=1;
	ret.have_uid=0;
	ret.have_gid=0;
	ret.have_rdev=1;
	ret.have_atimespec=1;
	ret.have_mtimespec=1;
	ret.have_ctimespec=1;
	ret.have_size=1;
	ret.have_allocated=1;
	ret.have_flags=0;
	ret.have_gen=1;
	ret.have_birthtimespec=1;
#else
#error fixme
#endif
	return ret;
}

void *begin_enumerate_directory(std::filesystem::path path)
{
#ifdef WIN32
	HANDLE ret=CreateFile(path.c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, NULL,
		OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
	return (INVALID_HANDLE_VALUE==ret) ? 0 : ret;
#else
#endif
}

void end_enumerate_directory(void *h)
{
#ifdef WIN32
	CloseHandle(h);
#else
#endif
}

std::shared_ptr<std::vector<directory_entry>> enumerate_directory(void *h, size_t maxitems, std::filesystem::path glob)
{
	std::shared_ptr<std::vector<directory_entry>> ret;
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

	FILE_ID_FULL_DIR_INFORMATION *buffer=(FILE_ID_FULL_DIR_INFORMATION *) malloc(sizeof(FILE_ID_FULL_DIR_INFORMATION)*maxitems);
	if(!buffer) throw std::bad_alloc();
	auto unbuffer=detail::Undoer([&buffer] { free(buffer); });

	IO_STATUS_BLOCK isb={ 0 };
	UNICODE_STRING _glob;
	if(!glob.empty())
	{
		_glob.Buffer=const_cast<std::filesystem::path::value_type *>(glob.c_str());
		_glob.Length=_glob.MaximumLength=(USHORT) glob.native().size();
	}
	if(0/*STATUS_SUCCESS*/!=NtQueryDirectoryFile(h, NULL, NULL, NULL, &isb, buffer, (ULONG)(sizeof(FILE_ID_FULL_DIR_INFORMATION)*maxitems),
		FileIdFullDirectoryInformation, FALSE, glob.empty() ? NULL : &_glob, FALSE))
	{
		return ret;
	}
	ret=std::make_shared<std::vector<directory_entry>>();
	std::vector<directory_entry> &_ret=*ret;
	_ret.reserve(maxitems);
	directory_entry item;
	// This is what windows returns with each enumeration
	item.have_metadata.have_ino=1;
	item.have_metadata.have_rdev=1;
	item.have_metadata.have_atimespec=1;
	item.have_metadata.have_mtimespec=1;
	item.have_metadata.have_size=1;
	item.have_metadata.have_allocated=1;
	item.have_metadata.have_birthtimespec=1;
	bool done=false;
	for(FILE_ID_FULL_DIR_INFORMATION *ffdi=buffer; !done; ffdi=(FILE_ID_FULL_DIR_INFORMATION *)((size_t) ffdi + ffdi->NextEntryOffset))
	{
		size_t length=ffdi->FileNameLength/sizeof(std::filesystem::path::value_type);
		if(length<=2 && '.'==ffdi->FileName[0])
			if(1==length || '.'==ffdi->FileName[1]) continue;
		std::filesystem::path::string_type leafname(ffdi->FileName, length);
		item.leafname=std::move(leafname);
		item.stat.st_ino=ffdi->FileId.QuadPart;
		item.stat.st_rdev=(ffdi->FileAttributes&FILE_ATTRIBUTE_DIRECTORY) ? S_IFDIR : S_IFREG;
		item.stat.st_atimespec=to_timespec(ffdi->LastAccessTime);
		item.stat.st_mtimespec=to_timespec(ffdi->LastWriteTime);
		item.stat.st_ctimespec=to_timespec(ffdi->ChangeTime);
		item.stat.st_size=ffdi->EndOfFile.QuadPart;
		item.stat.st_allocated=ffdi->AllocationSize.QuadPart;
		item.stat.st_birthtimespec=to_timespec(ffdi->CreationTime);
		_ret.push_back(std::move(item));
		if(!ffdi->NextEntryOffset) done=true;
	}
	return ret;
#else
#endif
}

} // namespace