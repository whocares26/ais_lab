#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include "db/KnowledgeBase.hpp"
#include "db/WorkingMemory.hpp"
#include "engine/ForwardChainer.hpp"
#include "engine/BackwardChainer.hpp"
#include "engine/ReadChoice.hpp"
#include "engine/Colors.hpp"

// Выбор объекта и его значения. Возвращает собранный факт.
// allowCreate = false — только существующие объекты и значения (для целевой ситуации).
db::Fact readFact(db::KnowledgeBase& kb, bool allowCreate = true) {
    std::cout << (allowCreate ? "Выберите существующий объект или создайте свой:\n"
                              : "Выберите объект:\n");

    std::vector<std::string> names;
    for (const auto& pair : kb.getObjects())
        names.push_back(pair.first);

    for (int i = 0; i < names.size(); i++)
        std::cout << i + 1 << ". " << names[i] << "\n";
    if (allowCreate)
        std::cout << names.size() + 1 << ". Создать свой\n";

    std::string objName;
    int input = readChoice(1, names.size() + (allowCreate ? 1 : 0));

    if (input == names.size() + 1) {
        std::cout << GREEN << "Введите имя объекта: " << RESET;
        std::getline(std::cin, objName);
        while (objName.empty()) {
            std::cout << RED << "Имя не может быть пустым. " << GREEN << "Введите имя объекта: " << RESET;
            std::getline(std::cin, objName);
        }

        db::Object newObj;

        std::cout << GREEN << "Введите текст вопроса (prompt): " << RESET;
        std::getline(std::cin, newObj.m_prompt);

        std::cout << "Может ли объект иметь несколько значений одновременно?\n"
                  << "1. Да\n"
                  << "2. Нет\n";
        newObj.m_multi = (readChoice(1, 2) == 1);

        std::cout << GREEN << "Введите допустимые значения (пустая строка — конец ввода):" << RESET << "\n";
        while (true) {
            std::string value;
            std::getline(std::cin, value);
            if (value.empty())
                break;
            newObj.m_values.push_back(value);
        }

        if (!kb.addObject(objName, newObj)) {
            std::cout << YELLOW << "Объект с таким именем уже существует, используется существующий" << RESET << "\n";
        }
    } else {
        objName = names[input - 1];
    }

    const auto& obj = kb.getObjects().at(objName);

    std::cout << (allowCreate ? "Выберите существующее значение или создайте своё:\n"
                              : "Выберите значение:\n");
    for (int j = 0; j < obj.m_values.size(); j++)
        std::cout << j + 1 << ". " << obj.m_values[j] << "\n";
    if (allowCreate)
        std::cout << obj.m_values.size() + 1 << ". Ввести новое значение\n";

    int valueChoice = readChoice(1, obj.m_values.size() + (allowCreate ? 1 : 0));

    if (valueChoice == obj.m_values.size() + 1) {
        std::cout << GREEN << "Введите новое значение: " << RESET;
        std::string val;
        std::getline(std::cin, val);
        if (!kb.addValue(objName, val)) {
            std::cout << RED << "Ошибка добавления значения!" << RESET << "\n";
        }
        return {objName, val};
    }

    return {objName, obj.m_values[valueChoice - 1]};
}

int main() {
    auto knowledgeBase = std::make_unique<db::KnowledgeBase>();
    if (!knowledgeBase->load()) {
        std::cerr << "Ошибка чтения базы знаний\n";
        return 1;
    }
    db::WorkingMemory workingMemory(*knowledgeBase);
    engine::ForwardChainer chainer(*knowledgeBase, workingMemory);
    engine::BackwardChainer backChainer(*knowledgeBase, workingMemory);

    while (true) {
        std::cout << CYAN << BOLD << "\n=== Экспертная система ===" << RESET << "\n"
                  << "1. Показать правила\n"
                  << "2. Добавить правило\n"
                  << "3. Изменить правило\n"
                  << "4. Удалить правило\n"
                  << "5. Показать стартовую ситуацию\n"
                  << "6. Запустить прямой вывод\n"
                  << "7. Проверить цель (обратный вывод)\n"
                  << "0. Выход\n";

        int choice = readChoice(0, 7);
        switch (choice) {
            case 1: {
                for (const auto& rule : knowledgeBase->getRules())
                    std::cout << rule.m_id << ". " << toString(rule) << "\n";
                break;
            }

            case 2: {
                db::Rule rule;

                std::cout << YELLOW << BOLD << "ВВОД УСЛОВИЯ" << RESET << "\n";
                while (true) {
                    rule.m_condition.push_back(readFact(*knowledgeBase));
                    std::cout << "Нужен ли ещё факт в условие?\n"
                                << "1. Да\n"
                                << "2. Нет\n";
                    if (readChoice(1, 2) == 2)
                        break;
                }
                std::cout << YELLOW << BOLD << "ВВОД ЗАКЛЮЧЕНИЯ" << RESET << "\n";
                rule.m_result = readFact(*knowledgeBase);
                if (!knowledgeBase->addRule(rule)) {
                    std::cerr << RED << "Ошибка при добавлении правила!" << RESET;
                }
                break;
            }

            case 3: {
                // "3. Изменить правило\n"
                const auto& list = knowledgeBase->getRules();
                for (const auto& rule : list)
                    std::cout << rule.m_id << ". " << toString(rule) << "\n";
                int id = -1;
                while (knowledgeBase->findRule(id) == nullptr) {
                    std::cout << GREEN << "Введите ID правила: " << RESET;
                    std::string line;
                    std::getline(std::cin, line);
                    try { id = std::stoi(line); } catch (...) { id = -1; }
                    if (knowledgeBase->findRule(id) == nullptr)
                        std::cout << RED << "Нет правила с таким ID" << RESET << "\n";
                }

                db::Rule rule;
                std::cout << YELLOW << BOLD << "ВВОД УСЛОВИЯ" << RESET << "\n";
                while (true) {
                    rule.m_condition.push_back(readFact(*knowledgeBase));
                    std::cout << "Нужен ли ещё факт в условие?\n"
                                << "1. Да\n"
                                << "2. Нет\n";
                    if (readChoice(1, 2) == 2)
                        break;
                }
                std::cout << YELLOW << BOLD << "ВВОД ЗАКЛЮЧЕНИЯ" << RESET << "\n";
                rule.m_result = readFact(*knowledgeBase);
                if (!knowledgeBase->replaceRule(id, rule)) {
                    std::cerr << RED << "Ошибка при изменении правила!" << RESET;
                }
                break;
            }

            case 4: {
                for (const auto& rule : knowledgeBase->getRules())
                    std::cout << rule.m_id << ". " << toString(rule) << "\n";
                int id = -1;
                while (knowledgeBase->findRule(id) == nullptr) {
                    std::cout << GREEN << "Введите ID правила: " << RESET;
                    std::string line;
                    std::getline(std::cin, line);
                    try { id = std::stoi(line); } catch (...) { id = -1; }
                    if (knowledgeBase->findRule(id) == nullptr)
                        std::cout << RED << "Нет правила с таким ID" << RESET << "\n";
                }
                if (!knowledgeBase->removeRule(id)) {
                    std::cout << RED << "Ошибка при удалении правила по ID" << RESET << "\n";
                }
                break;
            }

            case 5: {
                const auto& facts = knowledgeBase->getInitFacts();
                std::cout << YELLOW << BOLD << "СТАРТОВЫЕ ФАКТЫ" << RESET << "\n";
                for (const auto& fact : facts) {
                    std::cout << fact.first << "=" << fact.second << std::endl;
                }
                break;
            }

            case 6: {
                workingMemory.clear();
                chainer.run();

                std::cout << CYAN << BOLD << "\n=== Рабочая база данных ===" << RESET << "\n";
                for (const auto& obj : workingMemory.getMemory())
                    for (const auto& val : obj.second)
                        std::cout << obj.first << " = " << val << "\n";
                std::cout << CYAN << BOLD << "\n=== Заключение ===" << RESET << "\n";
                bool any = false;
                for (const auto& goal : knowledgeBase->getGoalObjects()) {
                    const auto& mem = workingMemory.getMemory();
                    auto it = mem.find(goal);
                    if (it == mem.end()) continue;          // цель не выведена
                    any = true;
                    for (const auto& val : it->second)
                        std::cout << goal << " = " << val << "\n";
                }
                if (!any) {
                    std::cout << YELLOW << "Заключение не получено: недостаточно данных" << RESET << "\n";
                }
                break;
            }
            case 7: {
                workingMemory.clear();
                std::cout << YELLOW << BOLD << "ВЫБОР ЦЕЛЕВОЙ СИТУАЦИИ" << RESET << "\n";
                db::Fact goal = readFact(*knowledgeBase, false);

                std::cout << CYAN << BOLD << "\n=== Обратный вывод ===" << RESET << "\n";
                backChainer.run(goal);

                std::cout << CYAN << BOLD << "\n=== Рабочая база данных ===" << RESET << "\n";
                for (const auto& obj : workingMemory.getMemory())
                    for (const auto& val : obj.second)
                        std::cout << obj.first << " = " << val << "\n";
                break;
            }
            case 0:
                return 0;
        }
    }
}
