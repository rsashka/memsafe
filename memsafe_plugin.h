#pragma once
#ifndef INCLUDED_MEMSAFE_PLUGIN_H_
#define INCLUDED_MEMSAFE_PLUGIN_H_

#include <string>
#include <vector>
#include <algorithm>
#include <map>
#include <unordered_set>

#include <yaml-cpp/yaml.h>
#include <stdio.h>
#include <filesystem>
#include <fstream>
#include <chrono>

#include "memsafe.h"

namespace fs = std::filesystem;

namespace memsafe {

    // Copy-Past form https://github.com/google/googletest

    inline void SplitString(const ::std::string& str, char delimiter,
            ::std::vector< ::std::string>* dest) {
        ::std::vector< ::std::string> parsed;
        ::std::string::size_type pos = 0;
        while (true) {
            const ::std::string::size_type colon = str.find(delimiter, pos);
            if (colon == ::std::string::npos) {
                parsed.push_back(str.substr(pos));
                break;
            } else {
                parsed.push_back(str.substr(pos, colon - pos));
                pos = colon + 1;
            }
        }
        dest->swap(parsed);
    }
    // Returns true if and only if the wildcard pattern matches the string. Each
    // pattern consists of regular characters, single-character wildcards (?), and
    // multi-character wildcards (*).
    // 
    // This function implements a linear-time string globbing algorithm based on
    // https://research.swtch.com/glob

    static bool PatternMatchesString(const std::string& name_str,
            const char* pattern, const char* pattern_end) {
        const char* name = name_str.c_str();
        const char* const name_begin = name;
        const char* const name_end = name + name_str.size();

        const char* pattern_next = pattern;
        const char* name_next = name;

        while (pattern < pattern_end || name < name_end) {
            if (pattern < pattern_end) {
                switch (*pattern) {
                    default: // Match an ordinary character.
                        if (name < name_end && *name == *pattern) {
                            ++pattern;
                            ++name;
                            continue;
                        }
                        break;
                    case '?': // Match any single character.
                        if (name < name_end) {
                            ++pattern;
                            ++name;
                            continue;
                        }
                        break;
                    case '*':
                        // Match zero or more characters. Start by skipping over the wildcard
                        // and matching zero characters from name. If that fails, restart and
                        // match one more character than the last attempt.
                        pattern_next = pattern;
                        name_next = name + 1;
                        ++pattern;
                        continue;
                }
            }
            // Failed to match a character. Restart if possible.
            if (name_begin < name_next && name_next <= name_end) {
                pattern = pattern_next;
                name = name_next;
                continue;
            }
            return false;
        }
        return true;
    }

    inline bool IsGlobPattern(const std::string& pattern) {
        return std::any_of(pattern.begin(), pattern.end(),
                [](const char c) {
                    return c == '?' || c == '*'; });
    }

    class StringMatcher {
    public:
        StringMatcher() = default;

        // Constructs a filter from a string of patterns separated by `:`.

        explicit StringMatcher(const std::string& filter, const char separator = ';') {
            Create(filter, separator);
        }

        void Create(const std::string& filter, const char separator = ';') {
            Clear();
            // By design "" filter matches "" string.
            std::vector<std::string> all_patterns;
            SplitString(filter, separator, &all_patterns);
            const auto exact_match_patterns_begin = std::partition(
                    all_patterns.begin(), all_patterns.end(), &IsGlobPattern);

            glob_patterns_.reserve(exact_match_patterns_begin - all_patterns.begin());
            std::move(all_patterns.begin(), exact_match_patterns_begin,
                    std::inserter(glob_patterns_, glob_patterns_.begin()));
            std::move(
                    exact_match_patterns_begin, all_patterns.end(),
                    std::inserter(exact_match_patterns_, exact_match_patterns_.begin()));
        }

        // Returns true if and only if name matches at least one of the patterns in
        // the filter.

        bool MatchesName(const std::string& name) const {
            return exact_match_patterns_.count(name) > 0 ||
                    std::any_of(glob_patterns_.begin(), glob_patterns_.end(),
                    [&name](const std::string & pattern) {
                        return PatternMatchesString(
                                name, pattern.c_str(),
                                pattern.c_str() + pattern.size());
                    });
        }

        //        bool MatchesNameColor(std::string& name, const char *start = "",, const char *stop = "") const {
        //            if (exact_match_patterns_.count(name)) {
        //            } else {
        //            }
        //            return  ||
        //                    std::any_of(glob_patterns_.begin(), glob_patterns_.end(),
        //                    [&name](const std::string & pattern) {
        //                        return PatternMatchesString(
        //                                name, pattern.c_str(),
        //                                pattern.c_str() + pattern.size());
        //                    });
        //        }

        bool isEmpty() {
            return glob_patterns_.empty() && exact_match_patterns_.empty();
        }

        void Clear() {
            glob_patterns_.clear();
            exact_match_patterns_.clear();
        }

        SCOPE(private) :
        std::vector<std::string> glob_patterns_;
        std::unordered_set<std::string> exact_match_patterns_;
    };

    std::string SeparatorRemove(const std::string_view number) {
        std::string result(number);
        auto removed = std::remove(result.begin(), result.end(), '\'');
        removed = std::remove(result.begin(), removed, '_');
        result.erase(removed, result.end());
        return result;
    }

    std::string SeparatorInsert(size_t number, const char sep = '\'') {
        std::string result = std::to_string(number);
        size_t pos = 3;
        while (pos < result.size()) {
            result = result.insert(result.size() - pos, 1, sep);
            pos += 4;
        }
        return result;
    }

    /*
     * 
     * 
     */
    class MemSafeShared {
    public:

        typedef std::map<std::string, std::string> SharedList;

        std::string m_shared_file;
        std::string m_input_file;

        MemSafeShared(std::string_view shared, std::string_view input) : m_shared_file(shared), m_input_file(input) {
        }

        SharedList ReadSharedFile() {

            SharedList result;
            YAML::Node file = YAML::LoadFile(m_shared_file);
            for (auto it = file.begin(); it != file.end(); it++) {
                for (auto cls = it->second.begin(); cls != it->second.end(); cls++) {
                    if (cls->first.as<std::string>().compare("classes") == 0) {
                        for (auto name = cls->second.begin(); name != cls->second.end(); name++) {
                            result[name->first.as<std::string>()] = name->second.as<std::string>();
                        }
                    }
                }
            }
            return result;
        }

        void WriteSharedFile(const SharedList &list) {

            YAML::Emitter writer;
            write_help(writer);

            writer << YAML::DoubleQuoted;
            writer << YAML::BeginMap;

            std::string input_modified;
            try {
                input_modified = std::format("{}", fs::last_write_time(m_input_file));
            } catch (std::exception &ex) {
                input_modified = ex.what();
            }


            if (fs::exists(fs::path(m_shared_file))) {
                YAML::Node old = YAML::LoadFile(m_shared_file);

                for (auto it = old.begin(); it != old.end(); it++) {

                    // File names at the top level of the map
                    if (it->first.as<std::string>().compare(m_input_file)) {
                        // Copy all except current file name
                        writer << YAML::Key << it->first.as<std::string>();
                        writer << YAML::Value << it->second;
                        writer << YAML::Newline;
                        writer << YAML::Newline;
                    }

                }

                fs::path old_name(m_shared_file);
                old_name += ".bak";
                fs::rename(fs::path(m_shared_file), old_name);
            }

            {
                writer << YAML::Key << m_input_file;
                writer << YAML::Value;

                writer << YAML::BeginMap;
                {
                    writer << YAML::Key << "modified";
                    writer << YAML::Value << input_modified;

                    writer << YAML::Key << "classes";
                    writer << YAML::Value;

                    writer << YAML::BeginMap;
                    for (auto &elem : list) {
                        writer << YAML::Key << elem.first;
                        writer << YAML::Value << elem.second;
                    }
                    writer << YAML::EndMap;
                }
                writer << YAML::EndMap;
            }
            writer << YAML::EndMap;
            writer << YAML::Newline;

            std::ofstream shared(m_shared_file);
            shared << writer.c_str();
            shared.close();
        }

    protected:

        void write_help(YAML::Emitter & emit) {

            emit << YAML::Comment("") << YAML::Newline;
            emit << YAML::Comment("This file is created automatically for circular reference analysis") << YAML::Newline;
            emit << YAML::Comment("by the memsafe plugin https://github.com/rsashka/memsafe ") << YAML::Newline;
            emit << YAML::Comment("when the C++ compiler uses multiple translation units.") << YAML::Newline;

            emit << YAML::Comment("") << YAML::Newline;
            emit << YAML::Comment("-------------------------------------------------------") << YAML::Newline;
            emit << YAML::Comment("") << YAML::Newline;

            emit << YAML::Comment("Since C++ compiles files separately, and the class (data structure)") << YAML::Newline;
            emit << YAML::Comment("definition may be in another translation unit due to a forward declaration,") << YAML::Newline;
            emit << YAML::Comment("two passes are required for the cyclic reference analyzer to work correctly.") << YAML::Newline;
            emit << YAML::Comment("") << YAML::Newline;
            emit << YAML::Comment("During the first pass, the plugin analyzes the file only for strong references") << YAML::Newline;
            emit << YAML::Comment("in all base classes whose definition is present during the AST analysis.") << YAML::Newline;
            emit << YAML::Comment("") << YAML::Newline;
            emit << YAML::Comment("All results of the first analyzer pass are collected in one file") << YAML::Newline;
            emit << YAML::Comment("each in its own section individually for each translation unit.") << YAML::Newline;
            emit << YAML::Comment("This section contains a list of analyzed classes with a list of reference data types.") << YAML::Newline;
            emit << YAML::Comment("(a list of class definitions that were found during the AST analysis).") << YAML::Newline;
            emit << YAML::Comment("") << YAML::Newline;
            emit << YAML::Comment("During the second pass, the analyzer loads one file and forms from it") << YAML::Newline;
            emit << YAML::Comment("a list of classes, each with its own list of reference data types") << YAML::Newline;
            emit << YAML::Comment("(it must be complete after the first pass is completed for all translation units).") << YAML::Newline;

            emit << YAML::Comment("") << YAML::Newline;
            emit << YAML::Comment("-------------------------------------------------------") << YAML::Newline;
            emit << YAML::Comment("") << YAML::Newline;
        }
    };
}
#endif // INCLUDED_MEMSAFE_PLUGIN_H_
