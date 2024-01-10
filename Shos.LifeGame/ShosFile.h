#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <sstream>
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

        auto start    = source.find_first_not_of(TRIM_CHARACTERS);
        auto end      = source.find_last_not_of (TRIM_CHARACTERS);
        return start == tstring::npos || end == tstring::npos ? source
                                                              : source.substr(start, end - start + 1);
    }

    static std::vector<std::string> Split(const std::string& text, char delimiter)
    {
        std::vector<std::string> tokens;
        std::stringstream        stream(text);
        std::string              token;
        while (std::getline(stream, token, delimiter))
           tokens.push_back(token);
        return tokens;
    }

    static std::string ToLower(const std::string& text)
    {
        std::string result;

        result.resize(text.size());
        std::transform(text.begin(), text.end(), result.begin(), tolower);
        return result;
    }
};

} // namespace Shos
