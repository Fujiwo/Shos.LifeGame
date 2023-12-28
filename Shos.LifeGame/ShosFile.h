#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#if defined(UNICODE) || defined(_UNICODE)
#define tstring std::wstring
#else // UNICODE
#define tstring std::string
#endif // UNICODE

namespace Shos {

class File final
{
public:
    static bool GetFilePaths(tstring folderPath, std::vector<tstring>& filePaths, std::string extension)
    {
        using namespace std::filesystem;

        filePaths.clear();
        if (!is_directory(folderPath))
            return false;

        directory_iterator end;
        std::error_code    err;

        for (directory_iterator iterator(folderPath); iterator != end && !err; iterator.increment(err)) {
            auto path = iterator->path();
            if (path.extension() == extension)
                filePaths.push_back(path
#if defined(UNICODE) || defined(_UNICODE)
                    .wstring()
#else // UNICODE
                    .string ()
#endif // UNICODE
                );
        }
        return !err;
    }

    static tstring GetStem(tstring filePath)
    {
        std::filesystem::path path = filePath;
        return path.stem()
#if defined(UNICODE) || defined(_UNICODE)
            .wstring();
#else // UNICODE
            .string ();
#endif // UNICODE
    }

    static bool Read(tstring fileName, std::vector<std::string>& lines)
    {
        std::ifstream stream(fileName);
        if (!stream.is_open())
            return false;

        std::string line;
        while (getline(stream, line))
            lines.push_back(line);

        return true;
    }
};

class String final
{
public:
    static std::string Trim(std::string source)
    {
        const std::string TRIM_CHARACTERS = " ";

        size_t start    = source.find_first_not_of(TRIM_CHARACTERS);
        size_t end      = source.find_last_not_of (TRIM_CHARACTERS);
        return source.substr(start, end - start + 1);
    }
};

} // namespace Shos
