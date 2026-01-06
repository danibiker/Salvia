#pragma once

#include <string>
#include <const/Constant.h>

using namespace std;

class FileProps{
    public:
        string filename;
        int filetype;
        string dir;
        int ico;
        std::size_t fileSize;
        string extension;
        string creationTime;
        string modificationTime;
        time_t iCreationTime;
        time_t iModificationTime;
        int compareMethod;
        
        FileProps(){
            init();
        }

        ~FileProps(){
            //cout << "deleting FileProps" << endl;
        }
        
        FileProps(string dir, string filename, int ico, int filetype){
            this->dir = dir;
            this->filename = filename;
            this->ico = ico;
            this->filetype = filetype;
            init();
        }

        void init(){
            compareMethod = COMPAREWHOLEWORD;
            filetype = TIPOFICHERO;
            ico = page_white;
            fileSize = 0;
            iCreationTime = 0;
            iModificationTime = 0;
        }

        FileProps(const FileProps &c){
            filename = c.filename;
            filetype = c.filetype;
            dir = c.dir;
            ico = c.ico;
            fileSize = c.fileSize;
            extension = c.extension;
            creationTime = c.creationTime;
            modificationTime = c.modificationTime;
            iCreationTime = c.iCreationTime;
            iModificationTime = c.iModificationTime;
        }
        
        int comparar(const FileProps &c) const {
            string str1 = filename;
            string str2 = c.filename;
            Constant::lowerCase(&str1);
            Constant::lowerCase(&str2);
            
            if (c.compareMethod == COMPAREWHOLEWORD)
                return str1.compare(str2);
            else if (c.compareMethod == COMPAREBEGINNING)
                return str1.find(str2);
            else return str1.compare(str2);
        }
        
        bool operator==(const FileProps &c) const {
            return comparar(c) == 0;
        }
        bool operator!=(const FileProps &c) const {
            return comparar(c) != 0;
        }
        bool operator<(const FileProps &c) const {
            return comparar(c) < 0;
        }
        bool operator>(const FileProps &c) const {
            return comparar(c) > 0;
        }
        bool operator<=(const FileProps &c) const {
            return comparar(c) <= 0;
        }
        bool operator>=(const FileProps &c) const {
            return comparar(c) >= 0;
        }
        
        static bool sortByText (FileProps i,FileProps j) { 
            return (i.filename < j.filename); 
        }

        static bool sortByTextUnique (const std::unique_ptr<FileProps>& i, const std::unique_ptr<FileProps>& j) { 
            return (i->filename < j->filename); 
        }
        
        static bool sortByTextReverse (FileProps i,FileProps j) { 
            return (i.filename > j.filename); 
        }
        
        static bool sortByTextCase (FileProps i,FileProps j) {
            string file1 = i.filename;
            string file2 = j.filename;
            Constant::lowerCase(&file1);
            Constant::lowerCase(&file2);
            return (file1 < file2); 
        }
        
        static bool sortByTextReverseCase (FileProps i, FileProps j) { 
            string file1 = i.filename;
            string file2 = j.filename;
            Constant::lowerCase(&file1);
            Constant::lowerCase(&file2);
            return (file1 > file2); 
        }
        
};

