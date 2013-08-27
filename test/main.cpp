/* FastDirectoryEnumerator
Enumerates very, very large directories quickly by directly using kernel syscalls. For POSIX and Windows.
(C) 2013 Niall Douglas http://www.nedprod.com/
File created: Aug 2013
*/

#define NUMBER_OF_FILES 100000

#define _CRT_SECURE_NO_WARNINGS

#include "../FastDirectoryEnumerator/FastDirectoryEnumerator.hpp"
#include <unordered_map>
#include <chrono>
#include <iostream>

#include <fcntl.h>
#ifdef WIN32
#include <io.h>
#include <direct.h>
#define POSIX_MKDIR(path, mode) _wmkdir(path)
#define POSIX_RMDIR _wrmdir
#define POSIX_STAT _wstat64
#define POSIX_STAT_STRUCT struct __stat64
#define POSIX_S_ISREG(m) ((m) & _S_IFREG)
#define POSIX_S_ISDIR(m) ((m) & _S_IFDIR)
#define POSIX_OPEN _wopen
#define POSIX_CLOSE _close
#define POSIX_UNLINK _wunlink
#define POSIX_FSYNC _commit
#define _L(f) L##f
#define POSIX_SPRINTF swprintf
#else
#include <sys/uio.h>
#include <limits.h>
#define POSIX_MKDIR mkdir
#define POSIX_RMDIR ::rmdir
#define POSIX_STAT_STRUCT struct stat 
#define POSIX_STAT stat
#define POSIX_OPEN open
#define POSIX_CLOSE ::close
#define POSIX_UNLINK unlink
#define POSIX_FSYNC fsync
#define POSIX_S_ISREG S_ISREG
#define POSIX_S_ISDIR S_ISDIR
#define _L(f) f
#define POSIX_SPRINTF sprintf
#endif

int main(void)
{
	using namespace FastDirectoryEnumerator;
	namespace chrono = std::chrono;
    typedef chrono::duration<double, std::ratio<1>> secs_type;

	std::unordered_map<std::filesystem::path::string_type, bool> tocheck;
	POSIX_MKDIR(_L("testdir"), 0x1f8/*770*/);

	// Create many files
	std::cout << "Creating " << NUMBER_OF_FILES << " files. This may take a while ..." << std::endl;
	std::filesystem::path::string_type name(_L("testdir/"));
	for(size_t n=0; n<NUMBER_OF_FILES; n++)
	{
		std::filesystem::path::value_type buffer[16];
		POSIX_SPRINTF(buffer, _L("%012u"), (unsigned) n);
		std::filesystem::path::string_type _buffer(buffer, 12);
		tocheck.insert(std::make_pair(_buffer, false));
		name.replace(8, std::string::npos, _buffer);
		int fh=POSIX_OPEN(name.c_str(), O_CREAT|O_RDWR, 0x1b0/*660*/);
		if(-1==fh) abort();
		POSIX_CLOSE(fh);
	}

	// Enumerate
	std::cout << "Enumerating " << NUMBER_OF_FILES << " files. This hopefully will be exceptionally swift ..." << std::endl;
    auto begin=chrono::high_resolution_clock::now();
	void *h=begin_enumerate_directory(_L("testdir"));
	std::shared_ptr<std::vector<directory_entry>> enumeration, chunk;
	while((chunk=enumerate_directory(h, NUMBER_OF_FILES)))
		if(!enumeration)
			enumeration=chunk;
		else
			enumeration->insert(enumeration->end(), std::make_move_iterator(chunk->begin()), std::make_move_iterator(chunk->end()));
	end_enumerate_directory(h);
    auto end=chrono::high_resolution_clock::now();
    auto diff=chrono::duration_cast<secs_type>(end-begin);
    std::cout << "It took " << diff.count() << " secs to enumerate " << NUMBER_OF_FILES << " entries which is " << NUMBER_OF_FILES/diff.count() << " entries per second." << std::endl;

	// Check results
	std::cout << "Checking enumeration and deleting " << NUMBER_OF_FILES << " files. This may also take a while ..." << std::endl;
	if(enumeration->size()!=NUMBER_OF_FILES)
		std::cerr << "ERROR: enumeration returned " << enumeration->size() << " items when it should have returned " << NUMBER_OF_FILES << " items." << std::endl;
	for(auto &entry : *enumeration)
	{
		auto it=tocheck.find(entry.name().native());
		if(it==tocheck.end())
			std::cerr << "ERROR: '" << entry.name() << "' shouldn't be there!" << std::endl;
		else if(it->second)
			std::cerr << "ERROR: '" << entry.name() << "' found more than once!" << std::endl;
		else
			it->second=true;
		name.replace(8, std::string::npos, entry.name().native());
		POSIX_UNLINK(name.c_str());
	}
	for(auto &item : tocheck)
		if(!item.second)
			std::cerr << "ERROR: '" << std::filesystem::path(item.first) << "' was not enumerated!" << std::endl;

	// Delete directory
	POSIX_RMDIR(_L("testdir"));
#ifdef _MSC_VER
	std::cout << "Press Return to exit ..." << std::endl;
	getchar();
#endif
	return 0;
}
