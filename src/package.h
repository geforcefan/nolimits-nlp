#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

class package {
public:
    virtual ~package() = default;

    virtual uint64_t size() const = 0;
    virtual size_t read(uint64_t offset, void *destination, size_t length) = 0;
};

package *open_package(const std::string &path);

