#include <fstream>
#include <iostream>
#include <sstream>
#include <string>


class FileReader
{
   private:
    std::ifstream file;

   public:
    FileReader(std::string filename) :
        file(filename, std::ios::in)
    {
    }

    std::string read() const
    {
        if (!file.is_open())
        {
            return "";
        }

        std::string ret;

        return ret;
    }
};


int main(void)
{
    FileReader file_reader("file.c");
    std::string content = file_reader.read();
    return 0;
}


int main(void)
{
    const char* filename = "file.c";

    FILE* fp = fopen(filename, "r");
    if (fp == NULL)
    {
    }
}