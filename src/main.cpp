#include "nlpxfile.h"
#include "package.h"
#include "ziparchive.h"
#include "zipbuilder.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

static void print_usage() {
    std::cout <<
        "usage:\n"
        "  nolimits-nlp extract [--merge] <package>... <directory>\n"
        "  nolimits-nlp pack <directory> <package>\n"
        "\n"
        "  nolimits-nlp extract data2603.nlpz trees\n"
        "  nolimits-nlp extract --merge data2000.nlpx data2603.nlpz merged\n"
        "  nolimits-nlp pack trees trees.nlpz\n";
}

static std::string format_size(uint64_t bytes) {
    const char *units[] = {"B", "KB", "MB", "GB"};
    double scaled = (double)bytes;
    int unit_index = 0;

    while (scaled >= 1024.0 && unit_index < 3) {
        scaled /= 1024.0;
        unit_index++;
    }

    char formatted[64];
    snprintf(formatted, sizeof(formatted), "%.1f %s", scaled, units[unit_index]);
    return std::string(formatted);
}

static bool read_file(const std::filesystem::path &path, std::vector<unsigned char> &bytes) {
    FILE *stream = fopen(path.string().c_str(), "rb");
    if (!stream) return false;

    fseek(stream, 0, SEEK_END);
    long length = ftell(stream);
    fseek(stream, 0, SEEK_SET);

    bytes.resize(length > 0 ? (size_t)length : 0);
    bool complete = bytes.empty() || fread(bytes.data(), 1, bytes.size(), stream) == bytes.size();

    fclose(stream);
    return complete;
}

static bool write_file(const std::filesystem::path &path, const unsigned char *bytes, size_t length) {
    FILE *stream = fopen(path.string().c_str(), "wb");
    if (!stream) return false;

    bool complete = length == 0 || fwrite(bytes, 1, length, stream) == length;

    fclose(stream);
    return complete;
}

static bool is_directory_entry(const std::string &name) {
    return !name.empty() && name.back() == '/';
}

static bool resolve_inside(const std::filesystem::path &root, const std::string &name, std::filesystem::path &target) {
    std::filesystem::path relative(name);
    if (relative.is_absolute()) return false;

    std::filesystem::path resolved = root;
    for (const std::filesystem::path &component : relative) {
        if (component == "..") return false;
        if (component == "." || component.empty()) continue;
        resolved /= component;
    }

    target = resolved;
    return resolved.string().rfind(root.string(), 0) == 0;
}

static std::string sort_name(const std::string &path) {
    std::string stem = std::filesystem::path(path).stem().string();
    std::transform(stem.begin(), stem.end(), stem.begin(),
                   [](unsigned char letter) { return (char)tolower(letter); });
    return stem;
}

static bool write_entry(zip_archive &archive, const zip_entry &archive_entry,
                        const std::filesystem::path &root, uint64_t &written_length) {
    std::filesystem::path target;
    if (!resolve_inside(root, archive_entry.name, target)) {
        std::cerr << "refused path " << archive_entry.name << "\n";
        return false;
    }

    std::vector<unsigned char> plain_bytes;
    if (!archive.unpack(archive_entry, plain_bytes)) {
        std::cerr << "cannot unpack " << archive_entry.name << "\n";
        return false;
    }

    std::error_code creation_failure;
    std::filesystem::create_directories(target.parent_path(), creation_failure);

    if (!write_file(target, plain_bytes.data(), plain_bytes.size())) {
        std::cerr << "cannot write " << target.string() << "\n";
        return false;
    }

    written_length += plain_bytes.size();
    return true;
}

static int extract_packages(std::vector<std::string> package_paths, const std::string &directory, bool overlay) {
    std::stable_sort(package_paths.begin(), package_paths.end(),
                     [](const std::string &left, const std::string &right) {
                         return sort_name(left) < sort_name(right);
                     });

    std::vector<std::unique_ptr<package>> packages;
    std::vector<zip_archive> archives(package_paths.size());

    for (size_t package_index = 0; package_index < package_paths.size(); package_index++) {
        packages.emplace_back(open_package(package_paths[package_index]));
        if (!packages.back() || !archives[package_index].open(packages.back().get())) {
            std::cerr << "cannot open " << package_paths[package_index] << "\n";
            return 1;
        }
    }

    std::filesystem::path root = std::filesystem::absolute(directory);
    std::error_code creation_failure;
    std::filesystem::create_directories(root, creation_failure);

    size_t written_count = 0;
    size_t failed_count = 0;
    uint64_t written_length = 0;

    if (overlay) {
        std::map<std::string, size_t> winning_package;
        size_t shadowed_count = 0;

        for (size_t package_index = 0; package_index < archives.size(); package_index++) {
            for (const zip_entry &archive_entry : archives[package_index].entries()) {
                if (is_directory_entry(archive_entry.name)) continue;

                if (winning_package.count(archive_entry.name)) shadowed_count++;
                winning_package[archive_entry.name] = package_index;
            }
        }

        for (const std::pair<const std::string, size_t> &winner : winning_package) {
            zip_archive &archive = archives[winner.second];

            const zip_entry *archive_entry = archive.find(winner.first);
            if (!archive_entry) continue;

            if (write_entry(archive, *archive_entry, root, written_length)) written_count++;
            else failed_count++;
        }

        std::cout << shadowed_count << " entries shadowed by a higher named package\n";
    } else {
        for (size_t package_index = 0; package_index < archives.size(); package_index++) {
            std::filesystem::path below = root;
            if (package_paths.size() > 1)
                below /= std::filesystem::path(package_paths[package_index]).stem();

            for (const zip_entry &archive_entry : archives[package_index].entries()) {
                if (is_directory_entry(archive_entry.name)) continue;

                if (write_entry(archives[package_index], archive_entry, below, written_length)) written_count++;
                else failed_count++;
            }
        }
    }

    std::cout << "extracted " << written_count << " files (" << format_size(written_length)
              << ") to " << root.string() << "\n";
    if (failed_count) std::cout << failed_count << " failed\n";

    return failed_count ? 2 : 0;
}

static int pack_directory(const std::string &directory, const std::string &package_path) {
    if (std::filesystem::path(package_path).extension() == ".nlpz") {
        std::cerr << "nlpz cannot be packed, its trailer needs an rsa signature that only the\n"
                     "vendor can issue and the game refuses a package without it, pack nlpx instead\n";
        return 1;
    }

    std::filesystem::path root = std::filesystem::absolute(directory);
    if (!std::filesystem::is_directory(root)) {
        std::cerr << directory << " is not a directory\n";
        return 1;
    }

    std::vector<std::filesystem::path> source_files;
    for (const std::filesystem::directory_entry &walked : std::filesystem::recursive_directory_iterator(root)) {
        if (walked.is_regular_file()) source_files.push_back(walked.path());
    }

    std::sort(source_files.begin(), source_files.end());

    zip_builder builder;
    uint64_t total_length = 0;

    for (const std::filesystem::path &source_file : source_files) {
        std::vector<unsigned char> plain_bytes;
        if (!read_file(source_file, plain_bytes)) {
            std::cerr << "cannot read " << source_file.string() << "\n";
            return 1;
        }

        std::string archive_name = std::filesystem::relative(source_file, root).generic_string();
        if (!builder.add(archive_name, plain_bytes.data(), plain_bytes.size())) {
            std::cerr << "cannot compress " << archive_name << "\n";
            return 1;
        }

        total_length += plain_bytes.size();
    }

    builder.finish();

    if (!nlpx_file::write(package_path, builder.bytes().data(), builder.bytes().size())) {
        std::cerr << "cannot write " << package_path << "\n";
        return 1;
    }

    std::cout << "packed " << source_files.size() << " files (" << format_size(total_length)
              << ") to " << package_path << "\n";
    return 0;
}

int main(int argument_count, char **arguments) {
    std::vector<std::string> given(arguments + 1, arguments + argument_count);

    if (given.size() >= 3 && given[0] == "pack") {
        if (given.size() != 3) {
            print_usage();
            return 1;
        }
        return pack_directory(given[1], given[2]);
    }

    if (given.size() >= 3 && given[0] == "extract") {
        bool overlay = given[1] == "--merge";
        std::vector<std::string> package_paths(given.begin() + (overlay ? 2 : 1), given.end() - 1);

        if (package_paths.empty()) {
            print_usage();
            return 1;
        }

        return extract_packages(package_paths, given.back(), overlay);
    }

    print_usage();
    return 1;
}
