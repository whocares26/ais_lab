#pragma once
#include <string>
#include <unordered_set>
#include <set>
#include "db/Fact.hpp"
#include "db/KnowledgeBase.hpp"
#include "db/WorkingMemory.hpp"

namespace engine {
    class BackwardChainer {
    public:
        BackwardChainer(const db::KnowledgeBase& kb, db::WorkingMemory& wm) : m_kb(kb), m_wm(wm) {}
        bool run(const db::Fact& goal);
        bool askUser(const std::string& objName, const std::string& indent = "");
    private:
        const db::KnowledgeBase& m_kb;
        std::unordered_set<std::string> m_in_progress;
        db::WorkingMemory& m_wm;
        bool prove(const db::Fact& goal, int depth = 0);
        std::set<std::string> m_unknown;   // объекты, про которые пользователь сказал "не знаю"
    };
}
