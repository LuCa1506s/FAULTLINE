#pragma once
#include "persistence/schema_v11.hpp"
#include <map>

namespace faultline::test {
// Compare ALL historical payload bytes after projecting only the explicitly
// introduced v11 CharacterBody collection out of Entities. No semantic fallback.
inline std::map<std::uint32_t,persistence::SaveBytes> historicalPayloads(const persistence::SaveBytes& bytes) {
    using namespace persistence;
    detail::Reader r{bytes}; r.offset=48;
    std::map<std::uint32_t,SaveBytes> sections;
    while (r.remaining()) {
        std::uint32_t id{},length{}; r(id,length); r.need(length);
        SaveBytes body{bytes.begin()+static_cast<std::ptrdiff_t>(r.offset),bytes.begin()+static_cast<std::ptrdiff_t>(r.offset+length)};
        if (id==3 && readSaveHeader(bytes).saveFormatVersion.value>=11) {
            detail::Reader entities{body}; std::uint64_t next{}; entities(next);
            auto count=entities.count(); entities.need(static_cast<std::size_t>(count)*8); entities.offset+=static_cast<std::size_t>(count)*8;
            count=entities.count(); entities.need(static_cast<std::size_t>(count)*28); entities.offset+=static_cast<std::size_t>(count)*28;
            count=entities.count(); entities.need(static_cast<std::size_t>(count)*16); entities.offset+=static_cast<std::size_t>(count)*16;
            const auto start=entities.offset; count=entities.count();
            core::require(count==0,"Historical fixture unexpectedly migrated CharacterBodies");
            body.erase(body.begin()+static_cast<std::ptrdiff_t>(start),body.begin()+static_cast<std::ptrdiff_t>(entities.offset));
        }
        sections.emplace(id,std::move(body)); r.offset+=length;
    }
    return sections;
}
inline bool preservesHistoricalPayloads(const persistence::SaveBytes& old,const persistence::SaveBytes& current) {
    const auto before=historicalPayloads(old),after=historicalPayloads(current);
    for (const auto& [id,bytes] : before) if (!after.contains(id) || after.at(id)!=bytes) return false;
    return true;
}
}
