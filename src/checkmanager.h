/*
   This file is part of the clazy static checker.

  Copyright (C) 2015 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Sérgio Martins <sergio.martins@kdab.com>

  Copyright (C) 2015-2016 Sergio Martins <smartins@kde.org>

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.
*/

#ifndef CLANG_LAZY_CHECK_MANAGER_H
#define CLANG_LAZY_CHECK_MANAGER_H

#include "clazy_export.h"
#include "checkbase.h"

#include <clang/Lex/PreprocessorOptions.h>

#include <functional>
#include <unordered_map>

struct CLAZYLIB_EXPORT RegisteredFixIt {
    typedef std::vector<RegisteredFixIt> List;
    RegisteredFixIt() : id(-1) {}
    RegisteredFixIt(int id, const std::string &name) : id(id), name(name) {}
    int id = -1;
    std::string name;
    bool operator==(const RegisteredFixIt &other) const { return id == other.id; }
};

using FactoryFunction = std::function<CheckBase*(ClazyContext *context)>;

struct CLAZYLIB_EXPORT RegisteredCheck {
    typedef std::vector<RegisteredCheck> List;
    std::string name;
    std::string className;
    CheckLevel level;
    FactoryFunction factory;
    bool operator==(const RegisteredCheck &other) const { return name == other.name; }
};

class CLAZYLIB_EXPORT CheckManager
{
public:
    static CheckManager *instance()
    {
        static CheckManager s_instance;
        return &s_instance;
    }

    int registerCheck(const std::string &name, const std::string &className, CheckLevel level, const FactoryFunction &);
    int registerFixIt(int id, const std::string &fititName, const std::string &checkName);

    RegisteredCheck::List availableChecks(CheckLevel maxLevel) const;
    RegisteredCheck::List requestedChecksThroughEnv() const;
    RegisteredCheck::List requestedChecksThroughEnv(std::vector<std::string> &userDisabledChecks) const;

    RegisteredCheck::List::const_iterator checkForName(const RegisteredCheck::List &checks, const std::string &name) const;
    RegisteredCheck::List checksForCommaSeparatedString(const std::string &str,
                                                        std::vector<std::string> &userDisabledChecks) const;
    RegisteredFixIt::List availableFixIts(const std::string &checkName) const;

    /**
     * Returns all checks with level <= requested level.
     */
    RegisteredCheck::List checksFromRequestedLevel() const;

    CheckBase::List createChecks(const RegisteredCheck::List &requestedChecks, ClazyContext *context);

    bool fixitsEnabled() const;
    void enableAllFixIts();

    bool allFixitsEnabled() const { return m_enableAllFixits; }

    /**
     * Enables all checks with level <= @p level.
     * A high level will enable checks known to have false positives, while a low level is more
     * conservative and emits less warnings.
     */
    void setRequestedLevel(CheckLevel level);
    CheckLevel requestedLevel() const { return m_requestedLevel; }

    static void removeChecksFromList(RegisteredCheck::List &list, std::vector<std::string> &checkNames);

private:
    CheckManager();

    bool checkExists(const std::string &name) const;
    RegisteredCheck::List checksForLevel(int level) const;
    bool isReservedCheckName(const std::string &name) const;
    CheckBase* createCheck(const std::string &name, ClazyContext *context);
    std::string checkNameForFixIt(const std::string &) const;
    RegisteredCheck::List m_registeredChecks;
    std::unordered_map<std::string, std::vector<RegisteredFixIt> > m_fixitsByCheckName;
    std::unordered_map<std::string, RegisteredFixIt > m_fixitByName;
    std::string m_requestedFixitName;
    bool m_enableAllFixits;
    CheckLevel m_requestedLevel;
};

#define CLAZY_STRINGIFY2(X) #X
#define CLAZY_STRINGIFY(X) CLAZY_STRINGIFY2(X)

#define REGISTER_CHECK_WITH_FLAGS(CHECK_NAME, CLASS_NAME, LEVEL) \
    volatile int ClazyAnchor_##CLASS_NAME = CheckManager::instance()->registerCheck(CHECK_NAME, CLAZY_STRINGIFY(CLASS_NAME), LEVEL, [](ClazyContext *context){ return new CLASS_NAME(CHECK_NAME, context); });

#define REGISTER_FIXIT(FIXIT_ID, FIXIT_NAME, CHECK_NAME) \
    static int dummy_##FIXIT_ID = CheckManager::instance()->registerFixIt(FIXIT_ID, FIXIT_NAME, CHECK_NAME); \
    inline void silence_warning_dummy_##FIXIT_ID() { (void)dummy_##FIXIT_ID; }

#endif
