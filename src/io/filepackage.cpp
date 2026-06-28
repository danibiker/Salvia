#include "filepackage.h"

#include <cstring>
#include <fstream>
#include <sstream>
#include <stdint.h>
#include <sys/stat.h>
#include <const/constant.h>
#include <io/dirutil.h>

#ifdef _MSC_VER
#include <direct.h>
#ifdef _XBOX
#include <xtl.h>
#define PATH_MAX MAX_PATH
#else
#include "../compat/dirent.h"
#endif
#else
#include <dirent.h>
#endif

// On-disk layout (version 2):
//   [4]  magic
//   [4]  version
//   [N]  data blob (concatenated binary contents of all entries)
//   [4]  entry count
//   [..] entries: [4]nameLen [nameLen]name [4]textLen [textLen]text [8]offset [8]size
//   [4]  indexOffset  (== HEADER_SIZE + data blob size)  -- last 4 bytes of file
//
// In memory we keep ONLY the index (m_entries). Binary data lives exclusively
// on disk and is read on demand by GetFile into m_readBuf.
static const uint32_t FILEPACKAGE_MAGIC   = 0x46474B50;
static const uint32_t FILEPACKAGE_VERSION = 2;
static const uint32_t HEADER_SIZE  = 8;
static const uint32_t FOOTER_SIZE  = 4;
static const uint32_t MAX_NAME_LEN = 65536;
static const uint32_t MAX_TEXT_LEN = 1u << 24; // 16 MiB
static const uint32_t MAX_COUNT    = 1000000;
static const std::streamsize COPY_CHUNK = 65536;

namespace {

struct LockGuard {
    CRITICAL_SECTION& cs;
    explicit LockGuard(CRITICAL_SECTION& c) : cs(c) { EnterCriticalSection(&cs); }
    ~LockGuard() { LeaveCriticalSection(&cs); }
private:
    LockGuard(const LockGuard&);
    LockGuard& operator=(const LockGuard&);
};

template <typename T>
inline void writeBin(std::ostream& os, const T& value) {
    os.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

template <typename T>
inline bool readBin(std::istream& is, T& value) {
    is.read(reinterpret_cast<char*>(&value), sizeof(T));
    return is.good();
}

void collectFiles(const std::string& dirPath, const std::string& basePath, std::vector<std::string>& fileList) {
#ifdef _XBOX
    std::string searchPath = dirPath;
    if (!searchPath.empty() && searchPath[searchPath.size() - 1] != '/' && searchPath[searchPath.size() - 1] != '\\'){
        searchPath += Constant::getFileSep();
    }
    std::string parentDir = searchPath;
    searchPath += '*';

    WIN32_FIND_DATA findData;
    HANDLE hFind = FindFirstFile(searchPath.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0) continue;

        std::string fullPath = parentDir + findData.cFileName;
        std::string subBase = basePath;
        if (!subBase.empty()) subBase += Constant::getFileSep();
        subBase += findData.cFileName;

        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY){
            collectFiles(fullPath, subBase, fileList);
        } else {
            fileList.push_back(subBase);
        }
    } while (FindNextFile(hFind, &findData) != 0);

    FindClose(hFind);
#else
    DIR* dp = opendir(dirPath.c_str());
    if (!dp) return;

    struct dirent* entry;
    while ((entry = readdir(dp)) != NULL){
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        std::string fullPath = dirPath;
        if (!fullPath.empty() && fullPath[fullPath.size() - 1] != '/' && fullPath[fullPath.size() - 1] != '\\'){
            fullPath += Constant::getFileSep();
        }
        fullPath += entry->d_name;

        struct stat st;
        if (stat(fullPath.c_str(), &st) != 0) continue;

        std::string subBase = basePath;
        if (!subBase.empty()) subBase += Constant::getFileSep();
        subBase += entry->d_name;

        if (S_ISDIR(st.st_mode)){
            collectFiles(fullPath, subBase, fileList);
        } else if (S_ISREG(st.st_mode)){
            fileList.push_back(subBase);
        }
    }
    closedir(dp);
#endif
}

bool streamCopy(std::istream& in, std::ostream& out, std::streamsize bytes) {
    char buf[COPY_CHUNK];
    while (bytes > 0){
        std::streamsize toRead = (bytes > COPY_CHUNK) ? COPY_CHUNK : bytes;
        in.read(buf, toRead);
        if (!in) return false;
        out.write(buf, toRead);
        if (!out) return false;
        bytes -= toRead;
    }
    return true;
}

} // namespace

FilePackage::FilePackage()
{
    InitializeCriticalSection(&m_cs);
}

FilePackage::~FilePackage()
{
    DeleteCriticalSection(&m_cs);
}

void FilePackage::writeIndex(std::ostream& out) const
{
    uint32_t count = (uint32_t)m_entries.size();
    writeBin(out, count);

    for (std::map<std::string, FileEntry>::const_iterator it = m_entries.begin(); it != m_entries.end(); ++it){
        const FileEntry& e = it->second;
        uint32_t nl  = (uint32_t)e.fileName.size();
        uint32_t tl  = (uint32_t)e.text.size();
        uint64_t off = (uint64_t)e.offset;
        uint64_t sz  = (uint64_t)e.size;

        writeBin(out, nl);
        if (nl) out.write(e.fileName.data(), nl);
        writeBin(out, tl);
        if (tl) out.write(e.text.data(), tl);
        writeBin(out, off);
        writeBin(out, sz);
    }
}

bool FilePackage::Pack(const std::string& sourceDir, const std::string& outputFile)
{
    LockGuard lock(m_cs);
    m_entries.clear();
    m_readBuf.clear();
    m_filePath.clear();

    std::vector<std::string> files;
    collectFiles(sourceDir, "", files);
    if (files.empty()) return false;

    std::ofstream out(outputFile.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
    if (!out.is_open()) return false;

    uint32_t magic   = FILEPACKAGE_MAGIC;
    uint32_t version = FILEPACKAGE_VERSION;
    writeBin(out, magic);
    writeBin(out, version);

    size_t dataBlobSize = 0;
    dirutil dir;

    for (size_t i = 0; i < files.size(); i++){
        std::string fullPath = sourceDir;
        if (!fullPath.empty() && fullPath[fullPath.size() - 1] != '/' && fullPath[fullPath.size() - 1] != '\\'){
            fullPath += Constant::getFileSep();
        }
        fullPath += files[i];

        FileEntry entry;
        entry.fileName = files[i];
        entry.offset = dataBlobSize;
        entry.size = 0;

        std::string ext = dir.getExtension(files[i]);
        Constant::lowerCase(&ext);

        if (ext == ".txt"){
            std::ifstream inTxt(fullPath.c_str());
            if (inTxt.is_open()){
                std::stringstream buffer;
                buffer << inTxt.rdbuf();
                entry.text = buffer.str();
            }
        } else {
            std::ifstream in(fullPath.c_str(), std::ios::binary);
            if (!in.is_open()){
                m_entries.clear();
                return false;
            }

            in.seekg(0, std::ios::end);
            std::streamsize fileSize = (std::streamsize)in.tellg();
            in.seekg(0, std::ios::beg);

            if (fileSize > 0 && !streamCopy(in, out, fileSize)){
                m_entries.clear();
                return false;
            }

            entry.size = (size_t)fileSize;
            dataBlobSize += (size_t)fileSize;
        }

        m_entries[entry.fileName] = entry;
    }

    uint32_t indexOffset = HEADER_SIZE + (uint32_t)dataBlobSize;
    writeIndex(out);
    writeBin(out, indexOffset);

    if (!out.good()){
        m_entries.clear();
        return false;
    }

    m_filePath = outputFile;
    return true;
}

bool FilePackage::Load(const std::string& packageFile)
{
    LockGuard lock(m_cs);
    m_entries.clear();
    m_readBuf.clear();
    m_filePath.clear();

    std::ifstream in(packageFile.c_str(), std::ios::binary);
    if (!in.is_open()){
        // No existing package -> create an empty one so subsequent Add* calls work.
        std::ofstream out(packageFile.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
        if (!out.is_open()) return false;
        uint32_t magic = FILEPACKAGE_MAGIC, version = FILEPACKAGE_VERSION;
        uint32_t count = 0, indexOffset = HEADER_SIZE;
        writeBin(out, magic);
        writeBin(out, version);
        writeBin(out, count);
        writeBin(out, indexOffset);
        if (!out.good()) return false;
        m_filePath = packageFile;
        return true;
    }

    uint32_t magic = 0, version = 0;
    if (!readBin(in, magic) || !readBin(in, version)) return false;
    if (magic != FILEPACKAGE_MAGIC || version != FILEPACKAGE_VERSION) return false;

    in.seekg(0, std::ios::end);
    std::streampos endPos = in.tellg();
    if (endPos < (std::streampos)(HEADER_SIZE + FOOTER_SIZE)) return false;

    uint32_t indexOffset = 0;
    in.seekg(-(std::streamoff)FOOTER_SIZE, std::ios::end);
    if (!readBin(in, indexOffset)) return false;
    if (indexOffset < HEADER_SIZE || (std::streampos)(indexOffset + FOOTER_SIZE) > endPos) return false;

    in.seekg(indexOffset);
    uint32_t count = 0;
    if (!readBin(in, count) || count > MAX_COUNT) return false;

    for (uint32_t i = 0; i < count; i++){
        FileEntry e;
        uint32_t nl = 0, tl = 0;

        if (!readBin(in, nl) || nl > MAX_NAME_LEN){
            m_entries.clear(); return false;
        }
        if (nl){
            e.fileName.resize(nl);
            in.read(&e.fileName[0], nl);
        }

        if (!readBin(in, tl) || tl > MAX_TEXT_LEN){
            m_entries.clear(); return false;
        }
        if (tl){
            e.text.resize(tl);
            in.read(&e.text[0], tl);
        }

        uint64_t off64 = 0, sz64 = 0;
        if (!readBin(in, off64) || !readBin(in, sz64)){
            m_entries.clear(); return false;
        }
        e.offset = (size_t)off64;
        e.size = (size_t)sz64;

        m_entries[e.fileName] = e;
    }

    m_filePath = packageFile;
    return true;
}

const unsigned char* FilePackage::GetFile(const std::string& fileName, std::size_t& outSize)
{
    LockGuard lock(m_cs);

    std::map<std::string, FileEntry>::const_iterator it = m_entries.find(fileName);
    if (it == m_entries.end() || it->second.size == 0 || m_filePath.empty()){
        outSize = 0;
        return NULL;
    }

    std::ifstream f(m_filePath.c_str(), std::ios::binary);
    if (!f.is_open()){
        outSize = 0;
        return NULL;
    }

    f.seekg((std::streamoff)(HEADER_SIZE + it->second.offset));
    if (!f){
        outSize = 0;
        return NULL;
    }

    m_readBuf.resize(it->second.size);
    f.read(reinterpret_cast<char*>(&m_readBuf[0]), (std::streamsize)it->second.size);
    if (!f){
        m_readBuf.clear();
        outSize = 0;
        return NULL;
    }

    outSize = it->second.size;
    return &m_readBuf[0];
}

bool FilePackage::appendToDisk(const std::string& fileName, const unsigned char* data, std::size_t dataSize, const std::string& text)
{
    // Caller already holds m_cs.
    if (m_filePath.empty()) return false;
    if (text.empty() && (data == NULL || dataSize == 0)) return false;
    if (m_entries.find(fileName) != m_entries.end()) return false;

    std::fstream f(m_filePath.c_str(), std::ios::in | std::ios::out | std::ios::binary);
    if (!f.is_open()) return false;

    // Read current indexOffset (footer = last 4 bytes).
    f.seekg(0, std::ios::end);
    std::streampos endPos = f.tellg();
    if (endPos < (std::streampos)(HEADER_SIZE + FOOTER_SIZE)) return false;

    f.seekg(-(std::streamoff)FOOTER_SIZE, std::ios::end);
    uint32_t oldIndexOffset = 0;
    if (!readBin(f, oldIndexOffset)) return false;
    if (oldIndexOffset < HEADER_SIZE || (std::streampos)(oldIndexOffset + FOOTER_SIZE) > endPos) return false;

    // New entry's offset within the data blob is the current blob size.
    size_t dataBlobSize = (size_t)(oldIndexOffset - HEADER_SIZE);

    FileEntry entry;
    entry.fileName = fileName;
    entry.text = text;
    entry.offset = dataBlobSize;
    entry.size = dataSize;
    m_entries[fileName] = entry;

    // Overwrite the old index area with [new data][new index][new footer].
    // Append-only -> file always grows -> no truncation needed.
    f.clear();
    f.seekp(oldIndexOffset);
    if (dataSize > 0){
        f.write(reinterpret_cast<const char*>(data), (std::streamsize)dataSize);
    }
    writeIndex(f);
    uint32_t newIndexOffset = oldIndexOffset + (uint32_t)dataSize;
    writeBin(f, newIndexOffset);
    f.flush();

    if (!f.good()){
        // Best-effort rollback of the in-memory state; the on-disk file may be partially corrupted.
        m_entries.erase(fileName);
        return false;
    }
    return true;
}

bool FilePackage::AddFileToDisk(const std::string& fileName, const unsigned char* data, std::size_t dataSize, const std::string& text)
{
    LockGuard lock(m_cs);
    return appendToDisk(fileName, data, dataSize, text);
}

std::string FilePackage::GetFileText(const std::string& fileName)
{
    LockGuard lock(m_cs);
    std::map<std::string, FileEntry>::const_iterator it = m_entries.find(fileName);
    return it != m_entries.end() ? it->second.text : std::string();
}

bool FilePackage::SetFileText(const std::string& fileName, const std::string& text)
{
    LockGuard lock(m_cs);
    std::map<std::string, FileEntry>::iterator it = m_entries.find(fileName);
    if (it == m_entries.end()) return false;
    it->second.text = text;
    // In-memory only; not persisted (the on-disk index keeps the old text).
    return true;
}

void FilePackage::GetFileNames(std::vector<std::string>& names) const
{
    LockGuard lock(m_cs);
    names.clear();
    names.reserve(m_entries.size());
    for (std::map<std::string, FileEntry>::const_iterator it = m_entries.begin(); it != m_entries.end(); ++it){
        names.push_back(it->first);
    }
}

int FilePackage::GetFileCount() const
{
    LockGuard lock(m_cs);
    return (int)m_entries.size();
}
