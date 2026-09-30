#include <string>
#include <vector>
#include <unordered_map>
#include "db/Rule.hpp"
#include "db/Object.hpp"
#include "db/KnowledgeBase.hpp"
#include "db/WorkingMemory.hpp"
#include "engine/BackwardChainer.hpp"
#include "engine/ReadChoice.hpp"
#include <iostream>

namespace engine {

        bool BackwardChainer::askUser(const std::string& objName) {
            const auto& obj = m_kb.getObjects().at(objName);

            std::cout << obj.m_prompt << "\n";
            for (size_t i = 0; i < obj.m_values.size(); i++)
                std::cout << i + 1 << ". " << obj.m_values[i] << "\n";
            std::cout << obj.m_values.size() + 1 << ". Не знаю\n";

            int choice = readChoice(1, obj.m_values.size() + 1);

            if (choice == obj.m_values.size() + 1)
                return false;         // не знаю

            m_wm.addFact({objName, obj.m_values[choice - 1]});
            return true;
        }

        bool BackwardChainer::run(const db::Fact& goal) {
            m_in_progress.clear();
            m_unknown.clear();

            std::cout << "Проверка цели: " << toString(goal) << "\n\n";

            bool proved = prove(goal);

            std::cout << "\n";
            if (proved)
                std::cout << "ЦЕЛЬ ДОСТИЖИМА: " << toString(goal) << "\n";
            else
                std::cout << "ЦЕЛЬ НЕ ДОСТИЖИМА: " << toString(goal) << "\n";

            return proved;
        }
        bool BackwardChainer::prove(const db::Fact& goal, int depth) {
            std::string indent(depth * 2, ' ');
            std::string key = toString(goal);

            std::cout << indent << "? " << key << "\n";

            // цель уже есть в рабочей бд
            if (m_wm.hasFact(goal.m_object, goal.m_value)) {
                std::cout << indent << "  известно\n";
                return true;
            }

            // цель уже доказывается
            if (m_in_progress.count(key) > 0) {
                std::cout << indent << "  уже доказывается, ветвь оборвана\n";
                return false;
            }

            m_in_progress.insert(key);

            // перебираем правила под эту цель
            for (const auto& rule : m_kb.getRules()) {
                if (rule.m_result.m_object != goal.m_object ||
                    rule.m_result.m_value  != goal.m_value)
                    continue;

                std::cout << indent << "  правило " << rule.m_id << ": " << toString(rule) << "\n";

                bool allProved = true;
                for (const auto& cond : rule.m_condition) {
                    if (!prove(cond, depth + 1)) {
                        allProved = false;
                        break;
                    }
                }

                if (allProved) {
                    m_wm.addFact(goal);
                    m_in_progress.erase(key);
                    std::cout << indent << "  доказано правилом " << rule.m_id << "\n";
                    return true;
                }
            }

            // запрос пользователю тк правил нет
            const auto& objects = m_kb.getObjects();
            auto it = objects.find(goal.m_object);
            if (it != objects.end() && !it->second.m_prompt.empty()
                && m_unknown.count(goal.m_object) == 0
                && !m_wm.isKnown(goal.m_object)) {

                if (!askUser(goal.m_object))
                    m_unknown.insert(goal.m_object);

                if (m_wm.hasFact(goal.m_object, goal.m_value)) {
                    m_in_progress.erase(key);
                    std::cout << indent << "  подтверждено пользователем\n";
                    return true;
                }
            }

            m_in_progress.erase(key);
            std::cout << indent << "  не доказано\n";
            return false;
        }
    }
