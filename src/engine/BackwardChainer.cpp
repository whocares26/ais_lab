#include <string>
#include <vector>
#include <unordered_map>
#include "db/Rule.hpp"
#include "db/Object.hpp"
#include "db/KnowledgeBase.hpp"
#include "db/WorkingMemory.hpp"
#include "engine/BackwardChainer.hpp"
#include "engine/ReadChoice.hpp"
#include "engine/Colors.hpp"
#include <iostream>

namespace engine {

        // условия правила без заключения: "ЕСЛИ a=1 И b=2"
        static std::string conditionsToString(const db::Rule& rule) {
            std::string res = "ЕСЛИ ";
            for (size_t i = 0; i < rule.m_condition.size(); i++) {
                if (i > 0)
                    res += " И ";
                res += toString(rule.m_condition[i]);
            }
            return res;
        }

        bool BackwardChainer::askUser(const std::string& objName, const std::string& indent) {
            const auto& obj = m_kb.getObjects().at(objName);

            std::cout << indent << obj.m_prompt << "\n" << indent;
            for (size_t i = 0; i < obj.m_values.size(); i++)
                std::cout << i + 1 << ". " << obj.m_values[i] << "  ";
            std::cout << obj.m_values.size() + 1 << ". Не знаю\n" << indent;

            int choice = readChoice(1, obj.m_values.size() + 1);

            if (choice == obj.m_values.size() + 1)
                return false;         // не знаю

            m_wm.addFact({objName, obj.m_values[choice - 1]});
            return true;
        }

        bool BackwardChainer::run(const db::Fact& goal) {
            m_in_progress.clear();
            m_unknown.clear();

            bool proved = prove(goal);

            std::cout << "\n";
            if (proved)
                std::cout << GREEN << BOLD << "ЦЕЛЬ ДОСТИЖИМА: " << toString(goal) << RESET << "\n";
            else
                std::cout << RED << BOLD << "ЦЕЛЬ НЕ ДОСТИЖИМА: " << toString(goal) << RESET << "\n";

            return proved;
        }

        bool BackwardChainer::prove(const db::Fact& goal, int depth) {
            std::string indent(depth * 4, ' ');
            std::string key = toString(goal);
            const std::string ok   = indent + GREEN + "✓ ";
            const std::string fail = indent + RED + "✗ ";

            std::cout << indent << BOLD << "Цель: " << RESET << key << "\n";

            // цель уже есть в рабочей бд
            if (m_wm.hasFact(goal.m_object, goal.m_value)) {
                std::cout << ok << "уже известно" << RESET << "\n";
                return true;
            }

            // однозначный объект уже имеет другое значение — цель противоречит рабочей бд
            const auto& objects = m_kb.getObjects();
            auto it = objects.find(goal.m_object);
            if (it != objects.end() && !it->second.m_multi && m_wm.isKnown(goal.m_object)) {
                std::cout << fail << "противоречит известному факту "
                          << goal.m_object << "=" << *m_wm.getMemory().at(goal.m_object).begin() << RESET << "\n";
                return false;
            }

            // цель уже доказывается
            if (m_in_progress.count(key) > 0) {
                std::cout << fail << "цель уже доказывается выше, ветвь оборвана" << RESET << "\n";
                return false;
            }

            m_in_progress.insert(key);

            // правила под эту цель
            std::vector<const db::Rule*> rules;
            for (const auto& rule : m_kb.getRules())
                if (rule.m_result.m_object == goal.m_object && rule.m_result.m_value == goal.m_value)
                    rules.push_back(&rule);

            int lastFailed = -1;   // последнее не сработавшее правило: его итог печатается вместе с итогом цели
            for (size_t i = 0; i < rules.size(); i++) {
                const auto& rule = *rules[i];
                std::cout << indent << "-> смотрим правило " << rule.m_id << ": " << conditionsToString(rule) << "\n";

                bool allProved = true;
                for (const auto& cond : rule.m_condition) {
                    if (!prove(cond, depth + 1)) {
                        allProved = false;
                        break;
                    }
                }

                // пока доказывались условия, объект мог получить другое значение
                if (allProved && m_wm.addFact(goal) == db::FactAddResult::Conflict) {
                    std::cout << fail << "правило " << rule.m_id << " противоречит рабочей бд" << RESET << "\n";
                    continue;
                }

                if (allProved) {
                    m_in_progress.erase(key);
                    std::cout << ok << "правило " << rule.m_id << " сработало: " << key << RESET << "\n";
                    return true;
                }

                if (i + 1 < rules.size())
                    std::cout << fail << "правило " << rule.m_id << " не сработало" << RESET << "\n";
                else
                    lastFailed = rule.m_id;
            }

            // правил нет или ни одно не сработало — спрашиваем пользователя
            bool askable = it != objects.end() && !it->second.m_prompt.empty()
                && m_unknown.count(goal.m_object) == 0
                && !m_wm.isKnown(goal.m_object);

            if (askable) {
                if (lastFailed != -1)
                    std::cout << fail << "правило " << lastFailed << " не сработало" << RESET << "\n";
                std::cout << indent << (rules.empty() ? "-> правил нет, спрашиваем пользователя\n"
                                                      : "-> правила не сработали, спрашиваем пользователя\n");
                if (!askUser(goal.m_object, indent + "   "))
                    m_unknown.insert(goal.m_object);

                m_in_progress.erase(key);
                if (m_wm.hasFact(goal.m_object, goal.m_value)) {
                    std::cout << ok << "подтверждено пользователем" << RESET << "\n";
                    return true;
                }
                if (m_unknown.count(goal.m_object) > 0)
                    std::cout << fail << "пользователь не знает" << RESET << "\n";
                else
                    std::cout << fail << "пользователь ответил " << goal.m_object << "="
                              << *m_wm.getMemory().at(goal.m_object).begin() << RESET << "\n";
                return false;
            }

            m_in_progress.erase(key);
            if (m_unknown.count(goal.m_object) > 0)
                std::cout << fail << "не доказано: на вопрос уже ответили «Не знаю»" << RESET << "\n";
            else if (rules.empty())
                std::cout << fail << "не доказано: нет ни правил, ни вопроса" << RESET << "\n";
            else if (lastFailed != -1)
                std::cout << fail << "правило " << lastFailed << " не сработало: " << key << " не доказано" << RESET << "\n";
            else
                std::cout << fail << "не доказано: " << key << RESET << "\n";
            return false;
        }
    }
