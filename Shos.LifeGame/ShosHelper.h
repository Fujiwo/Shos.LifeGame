#pragma once

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace Shos {

#if defined(UNICODE) || defined(_UNICODE)
using tstring = std::wstring;
#else // UNICODE
using tstring = std::string;
#endif // UNICODE

class String final
{
public:
    static std::string Trim(std::string_view source)
    {
        constexpr std::string_view trimCharacters = " \t\r\n";

        const auto start = source.find_first_not_of(trimCharacters);
        const auto end   = source.find_last_not_of (trimCharacters);
        return start == std::string_view::npos ? std::string()
                                               : std::string(source.substr(start, end - start + 1));
    }

    static std::string Adjust(std::string text, char character, size_t width)
    {
        if (text.length() < width)
            text.append(width - text.length(), character);
        return text;
    }

    static std::vector<std::string> Split(const std::string& text, char delimiter)
    {
        std::vector<std::string> tokens;
        std::istringstream       stream(text);
        std::string              token;
        while (std::getline(stream, token, delimiter))
            tokens.push_back(token);
        return tokens;
    }

    static std::string ToLower(std::string text)
    {
        std::ranges::transform(text, text.begin(), [](unsigned char character) { return char(std::tolower(character)); });
        return text;
    }
};

class File final
{
public:
    /// <summary>Gets the paths of the files whose extension matches (case-insensitive), sorted by file name.</summary>
    static std::vector<tstring> GetFilePaths(const tstring& folderPath, std::string_view extension)
    {
        namespace fs = std::filesystem;

        if (!fs::is_directory(folderPath))
            return {};

        const auto        lowerExtension = String::ToLower(std::string(extension));
        std::error_code   error;
        std::vector<fs::path> paths;
        for (fs::directory_iterator iterator(folderPath, error), end; iterator != end && !error; iterator.increment(error)) {
            const auto& path = iterator->path();
            if (String::ToLower(path.extension().string()) == lowerExtension)
                paths.push_back(path);
        }

        std::ranges::sort(paths, {}, [](const fs::path& path) { return String::ToLower(path.filename().string()); });

        std::vector<tstring> filePaths;
        for (const auto& path : paths)
            filePaths.push_back(ToTString(path));
        return filePaths;
    }

    static tstring GetStem(const tstring& filePath)
    { return ToTString(std::filesystem::path(filePath).stem()); }

    /// <returns>The lines of the file, or no lines when the file cannot be read.</returns>
    static std::vector<std::string> Read(const tstring& fileName)
    {
        std::vector<std::string> lines;
        std::ifstream            stream{ std::filesystem::path(fileName) };
        for (std::string line; std::getline(stream, line); )
            lines.push_back(line);
        return lines;
    }

private:
    static tstring ToTString(const std::filesystem::path& path)
    {
#if defined(UNICODE) || defined(_UNICODE)
        return path.wstring();
#else // UNICODE
        return path.string();
#endif // UNICODE
    }
};

} // namespace Shos
