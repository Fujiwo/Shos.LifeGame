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
        std::transform(text.begin(), text.end(), result.begin(), tolower);
        return result;
    }
};

} // namespace Shos
