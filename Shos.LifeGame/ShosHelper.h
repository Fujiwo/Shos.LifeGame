#pragma once

#include <algorithm>
#include <cassert>
#include <cctype>
#include <filesystem>
#include <functional>
#include <iterator>
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

class Helper final
{
public:
    template <typename TCollection, typename TElement>
    static void ForEach(const TCollection& collection, std::function<void(const TElement&)> action)
    {
        for (const auto& element : collection)
            action(element);
    }

    template <typename TSourceCollection, typename TDestinationCollection, typename TSourceElement, typename TDestinationElement>
    static void Map(const TSourceCollection& source, TDestinationCollection& destination, std::function<TDestinationElement(const TSourceElement&)> map)
    {
        destination.clear();
        for (const auto& element : source)
            destination.push_back(map(element));
    }

    template <typename TCollection, typename TElement>
    static void Filter(const TCollection& source, TCollection& destination, std::function<bool(const TElement&)> isMatch)
    {
        destination.clear();
        for (const auto& element : source) {
            if (isMatch(element))
                destination.push_back(element);
        }
    }

    template <typename TCollection, typename TElement, typename TSizeType = size_t>
    static TSizeType Maximum(const TCollection& collection, std::function<TSizeType(const TElement&)> getValue)
    {
        assert(collection.size() > 0);

        std::vector<TSizeType> values;
        Map<TCollection, std::vector<TSizeType>, TElement, TSizeType>(collection, values, getValue);
        auto iterator = std::max_element(values.begin(), values.end());
        return *iterator;
    }

    template <typename TCollection>
    static void Connect(TCollection& target, const TCollection& collection)
    { std::copy(collection.begin(), collection.end(), std::back_inserter(target)); }

    template <typename TCollection>
    static std::string Connect(const TCollection& texts)
    {
        std::ostringstream stream;
        ForEach<TCollection, std::string>(texts, [&](const std::string& text) { stream << text; });
        return stream.str();
    }
};

class String final
{
public:
    static std::string Trim(std::string source)
    {
        const std::string TRIM_CHARACTERS = " \t\r\n";

        auto start    = source.find_first_not_of(TRIM_CHARACTERS);
        auto end      = source.find_last_not_of (TRIM_CHARACTERS);
        return start == std::string::npos || end == std::string::npos ? std::string()
                                                                      : source.substr(start, end - start + 1);
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
        std::transform(text.begin(), text.end(), result.begin(), [](unsigned char character) { return char(std::tolower(character)); });
        return result;
    }
};

class File final
{
public:
    /// <summary>Gets the paths of the files whose extension matches (case-insensitive), sorted by file name.</summary>
    static bool GetFilePaths(tstring folderPath, std::vector<tstring>& filePaths, std::string extension)
    {
        using namespace std::filesystem;

        filePaths.clear();
        if (!is_directory(folderPath))
            return false;

        directory_iterator end;
        std::error_code    err;
        std::vector<path>  paths;

        for (directory_iterator iterator(folderPath); iterator != end && !err; iterator.increment(err)) {
            const auto& path = iterator->path();
            if (String::ToLower(path.extension().string()) == String::ToLower(extension))
                paths.push_back(path);
        }

        std::sort(paths.begin(), paths.end(), [](const path& path1, const path& path2) {
            return String::ToLower(path1.filename().string()) < String::ToLower(path2.filename().string());
        });

        for (const auto& path : paths)
            filePaths.push_back(path
#if defined(UNICODE) || defined(_UNICODE)
                .wstring()
#else // UNICODE
                .string ()
#endif // UNICODE
            );
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

        lines.clear();
        std::string line;
        while (getline(stream, line))
            lines.push_back(line);

        return true;
    }
};

} // namespace Shos
