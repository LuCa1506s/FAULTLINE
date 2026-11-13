#pragma once
#include "persistence/save.hpp"
#include <bit>
#include <limits>
#include <type_traits>

namespace faultline::persistence::detail {
template<class T> struct IsVector : std::false_type {};
template<class T, class A> struct IsVector<std::vector<T,A>> : std::true_type {};
template<class T> struct IsOptional : std::false_type {};
template<class T> struct IsOptional<std::optional<T>> : std::true_type {};
template<class T> struct IsArray : std::false_type {};
template<class T, std::size_t N> struct IsArray<std::array<T,N>> : std::true_type {};
template<class T> struct IsId : std::false_type {};
template<class Tag> struct IsId<Id<Tag>> : std::true_type {};
inline constexpr std::uint32_t maxCollectionCount = 100000;
inline constexpr std::uint32_t maxStringBytes = 65536;
// Strict UTF-8: reject overlong forms, surrogates, invalid continuations and > U+10FFFF.
inline bool validUtf8(std::string_view text) {
    std::uint32_t value = 0, minimum = 0; unsigned remaining = 0;
    for (char character : text) {
        const auto byte=static_cast<unsigned char>(character);
        if (remaining == 0) {
            if (byte < 0x80) continue;
            if (byte >= 0xC2 && byte <= 0xDF) { remaining=1; value=byte & 31U; minimum=0x80; }
            else if (byte >= 0xE0 && byte <= 0xEF) { remaining=2; value=byte & 15U; minimum=0x800; }
            else if (byte >= 0xF0 && byte <= 0xF4) { remaining=3; value=byte & 7U; minimum=0x10000; }
            else return false;
        } else {
            if ((byte & 0xC0U) != 0x80) return false;
            value=(value << 6) | (byte & 63U);
            if (--remaining == 0 && (value < minimum || value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF))) return false;
        }
    }
    return remaining == 0;
}
class Writer {
public:
    SaveBytes bytes;
    std::size_t objects{};
    void collection(std::size_t count) {
        if (count>maxCollectionCount || count>300000-objects) fail("Collection resource limit");
        objects+=count; write(static_cast<std::uint32_t>(count));
    }
    template<class... T> void operator()(const T&... values) { (write(values), ...); }
    template<class T> void write(const T& value) {
        if constexpr (std::is_same_v<T,bool>) write(static_cast<std::uint8_t>(value));
        else if constexpr (std::is_integral_v<T>) {
            using U = std::make_unsigned_t<T>;
            auto bits = std::bit_cast<U>(value);
            for (std::size_t i=0; i<sizeof(T); ++i) { bytes.push_back(static_cast<std::uint8_t>(bits & 255U)); bits >>= 8; }
        } else if constexpr (std::is_enum_v<T>) write(static_cast<std::underlying_type_t<T>>(value));
        else if constexpr (std::is_same_v<T,float>) {
            static_assert(sizeof(float)==4 && std::numeric_limits<float>::is_iec559);
            if (!std::isfinite(value)) fail("Non-finite float");
            write(std::bit_cast<std::uint32_t>(value == 0 ? 0.0F : value));
        } else if constexpr (IsId<T>::value) write(value.value());
        else if constexpr (std::is_same_v<T,std::string>) {
            if (value.size()>maxStringBytes || !validUtf8(value)) fail("Invalid/oversized UTF-8 string");
            write(static_cast<std::uint32_t>(value.size()));
            for (char c : value) bytes.push_back(static_cast<std::uint8_t>(c));
        } else if constexpr (IsVector<T>::value) {
            collection(value.size()); for (const auto& item : value) write(item);
        } else if constexpr (IsArray<T>::value) { for (const auto& item : value) write(item); }
        else if constexpr (IsOptional<T>::value) { write(value.has_value()); if (value) write(*value); }
        else fields(*this,value);
        if (bytes.size()>maxSaveBytes) fail("Save resource limit");
    }
    [[noreturn]] void fail(std::string message) const { throw SaveError{SaveErrorCode::InvalidData,"encode",bytes.size(),std::move(message)}; }
};
class Reader {
public:
    explicit Reader(std::span<const std::uint8_t> data) : bytes(data) {}
    std::span<const std::uint8_t> bytes;
    std::size_t offset{};
    std::string section{"header"};
    SaveHeader header;
    std::size_t objectsLeft{300000}; // Global allocation budget, including nested collections.
    template<class... T> void operator()(T&... values) { (read(values), ...); }
    [[nodiscard]] std::size_t remaining() const { return bytes.size()-offset; }
    void need(std::size_t count) const { if (count>remaining()) fail(SaveErrorCode::TruncatedData,"Unexpected end of data"); }
    [[noreturn]] void fail(SaveErrorCode code, std::string message) const {
        throw SaveError{code,section,offset,std::move(message),header};
    }
    std::uint32_t count(std::uint32_t maximum = maxCollectionCount) {
        std::uint32_t result{}; read(result);
        if (result>maximum || result>objectsLeft) fail(SaveErrorCode::InvalidData,"Collection length exceeds resource limit");
        need(result); // Every encoded element occupies at least one byte; no speculative large resize.
        objectsLeft-=result; return result;
    }
    template<class T> void read(T& value) {
        if constexpr (std::is_same_v<T,bool>) {
            std::uint8_t v{}; read(v); if (v>1) fail(SaveErrorCode::InvalidData,"Invalid bool"); value=(v==1);
        } else if constexpr (std::is_integral_v<T>) {
            need(sizeof(T)); std::uint64_t bits{};
            for (std::size_t i=0; i<sizeof(T); ++i) bits |= static_cast<std::uint64_t>(bytes[offset++]) << (i*8);
            value=std::bit_cast<T>(static_cast<std::make_unsigned_t<T>>(bits));
        } else if constexpr (std::is_enum_v<T>) {
            std::underlying_type_t<T> v{}; read(v); value=static_cast<T>(v);
        } else if constexpr (std::is_same_v<T,float>) {
            std::uint32_t v{}; read(v); value=std::bit_cast<float>(v);
            if (!std::isfinite(value)) fail(SaveErrorCode::InvalidData,"Non-finite float");
            if (value==0) value=0.0F;
        } else if constexpr (IsId<T>::value) { std::uint64_t v{}; read(v); value=T{v}; }
        else if constexpr (std::is_same_v<T,std::string>) {
            std::uint32_t length{}; read(length);
            if (length>maxStringBytes) fail(SaveErrorCode::InvalidData,"String length exceeds resource limit");
            need(length); value.assign(bytes.begin()+static_cast<std::ptrdiff_t>(offset),bytes.begin()+static_cast<std::ptrdiff_t>(offset+length)); offset+=length;
            if (!validUtf8(value)) fail(SaveErrorCode::InvalidData,"Invalid UTF-8");
        } else if constexpr (IsVector<T>::value) {
            const auto length=count(); value.clear();
            // Decode one element at a time: a bogus count cannot allocate count*sizeof(T).
            for (std::uint32_t i=0; i<length; ++i) { typename T::value_type item{}; read(item); value.push_back(std::move(item)); }
        } else if constexpr (IsArray<T>::value) { for (auto& item : value) read(item); }
        else if constexpr (IsOptional<T>::value) {
            bool present{}; read(present); if (present) { value.emplace(); read(*value); } else value.reset();
        } else fields(*this,value);
    }
};
}
