#ifndef FILEPACKAGE_H_INCLUDED
#define FILEPACKAGE_H_INCLUDED

#include <string>
#include <map>
#include <vector>
#include <iosfwd>

#ifdef _XBOX
	#include <direct.h>
	#include <xtl.h>
#else
	#include <windows.h>
#endif

class FilePackage{
public:
    FilePackage();
    ~FilePackage();

    bool Pack(const std::string& sourceDir, const std::string& outputFile);
    bool Load(const std::string& packageFile);

    const unsigned char* GetFile(const std::string& fileName, std::size_t& outSize);
    bool AddFileToDisk(const std::string& fileName, const unsigned char* data, std::size_t dataSize, const std::string& text = "");
    std::string GetFileText(const std::string& fileName);
    bool SetFileText(const std::string& fileName, const std::string& text);

    void GetFileNames(std::vector<std::string>& names) const;
    int GetFileCount() const;

private:
    struct FileEntry{
        std::string fileName;
        std::string text;
        size_t offset; // byte offset within the data blob (file position = HEADER_SIZE + offset)
        size_t size;
    };

    bool appendToDisk(const std::string& fileName, const unsigned char* data, std::size_t dataSize, const std::string& text);
    void writeIndex(std::ostream& out) const;

    std::map<std::string, FileEntry> m_entries;
    std::vector<unsigned char> m_readBuf; // buffer reused by GetFile; pointer valid until next GetFile on this instance
    std::string m_filePath;
    mutable CRITICAL_SECTION m_cs;
};

#endif
