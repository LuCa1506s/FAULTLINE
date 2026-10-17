#pragma once
#include "persistence/schema_v7.hpp"
namespace faultline::persistence::detail {
template<class A, class T> requires std::is_same_v<std::remove_cv_t<T>, simulation::StoryPrerequisite>
void fields(A& a,T& v) { a(v.node,v.outcome); }
template<class A, class T> requires std::is_same_v<std::remove_cv_t<T>, simulation::StoryNode>
void fields(A& a,T& v) {
    a(v.kind,v.faction,v.other,v.originalContact,v.contact,v.territory,v.district,v.outcome,v.prerequisites,v.historyRecord);
}
}
