#pragma once

#include <optional>
#include <string>
#include <vector>

#include "models/book.h"
#include "source/domain/source_types.h"

namespace fanqie {

class IBookSource {
public:
    virtual ~IBookSource() = default;

    virtual const SourceInfo& info() const = 0;
    virtual const SourceCapabilities& capabilities() const = 0;

    virtual void configure() = 0;

    virtual std::vector<Book> search(const std::string& keywords, int page) = 0;
    virtual std::optional<Book> get_book_info(const std::string& book_id) = 0;
    virtual std::vector<TocItem> get_toc(const std::string& book_id) = 0;
    virtual std::optional<Chapter> get_chapter(
        const std::string& book_id,
        const std::string& item_id) = 0;
};

} // namespace fanqie
