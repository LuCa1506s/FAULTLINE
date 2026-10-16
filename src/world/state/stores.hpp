#pragma once
#include "core/diagnostics/diagnostics.hpp"
#include "core/id/id.hpp"
#include <map>
#include <set>
#include <utility>

namespace faultline::world {
template<class IdType> class IdAllocator {
public:
    explicit IdAllocator(std::uint64_t next = 1) : next_(next) {}
    [[nodiscard]] IdType allocate() {
        core::require(next_ != 0, "ID domain exhausted");
        return IdType{next_++};
    }
    [[nodiscard]] std::uint64_t nextValue() const { return next_; }
private:
    std::uint64_t next_{1};
};
class EntityRegistry {
public:
    static EntityRegistry restore(std::uint64_t next, std::set<EntityId> live) {
        core::require(live.empty() || (static_cast<bool>(*live.begin()) &&
            (next == 0 || live.rbegin()->value() < next)), "Invalid entity allocator high-water mark");
        EntityRegistry result; result.ids_ = IdAllocator<EntityId>{next}; result.live_ = std::move(live); return result;
    }
    EntityId create() { const auto id = ids_.allocate(); live_.insert(id); return id; }
    void destroy(EntityId id) { core::require(live_.erase(id) == 1, "Destroying unknown entity"); }
    [[nodiscard]] bool exists(EntityId id) const { return live_.contains(id); }
    [[nodiscard]] const std::set<EntityId>& entries() const { return live_; }
    [[nodiscard]] std::uint64_t nextId() const { return ids_.nextValue(); }
private:
    IdAllocator<EntityId> ids_;
    std::set<EntityId> live_;
};
template<class T> class ComponentStore {
public:
    void set(const EntityRegistry& registry, EntityId id, T component) {
        core::require(registry.exists(id), "Component requires a live entity");
        entries_.insert_or_assign(id, std::move(component));
    }
    T& get(EntityId id) { return entries_.at(id); }
    const T& get(EntityId id) const { return entries_.at(id); }
    [[nodiscard]] bool contains(EntityId id) const { return entries_.contains(id); }
    void remove(EntityId id) { entries_.erase(id); }
    [[nodiscard]] const std::map<EntityId, T>& entries() const { return entries_; }
private:
    std::map<EntityId, T> entries_;
};
// Domain keys are the authoritative IDs. Records cannot accidentally edit their key.
// No erase API: historical semantic references must survive actor death.
template<class IdType, class T> class DomainStore {
public:
    static DomainStore restore(std::uint64_t next, std::map<IdType, T> entries) {
        core::require(entries.empty() || (static_cast<bool>(entries.begin()->first) &&
            (next == 0 || entries.rbegin()->first.value() < next)), "Invalid domain allocator high-water mark");
        DomainStore result; result.ids_ = IdAllocator<IdType>{next}; result.entries_ = std::move(entries); return result;
    }
    IdType create(T value = {}) {
        const auto id = ids_.allocate(); entries_.emplace(id, std::move(value)); return id;
    }
    [[nodiscard]] bool exists(IdType id) const { return entries_.contains(id); }
    T& get(IdType id) { return entries_.at(id); }
    const T& get(IdType id) const { return entries_.at(id); }
    [[nodiscard]] const std::map<IdType, T>& entries() const { return entries_; }
    [[nodiscard]] std::uint64_t nextId() const { return ids_.nextValue(); }
private:
    IdAllocator<IdType> ids_;
    std::map<IdType, T> entries_;
};
}
