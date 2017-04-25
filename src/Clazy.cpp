/*
   This file is part of the clazy static checker.

  Copyright (C) 2015 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Sérgio Martins <sergio.martins@kdab.com>

  Copyright (C) 2015-2017 Sergio Martins <smartins@kde.org>

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

#include "Utils.h"
#include "Clazy.h"
#include "StringUtils.h"
#include "clazy_stl.h"
#include "checkbase.h"
#include "AccessSpecifierManager.h"

#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/AST/AST.h"

#include "clang/Frontend/CompilerInstance.h"
#include "llvm/Support/raw_ostream.h"
#include "clang/AST/ParentMap.h"
#include <llvm/Config/llvm-config.h>

#include <stdio.h>
#include <sstream>
#include <iostream>

using namespace clang;
using namespace std;
using namespace clang::ast_matchers;


static void manuallyPopulateParentMap(ParentMap *map, Stmt *s)
{
    if (!s)
        return;

    for (Stmt *child : s->children()) {
        llvm::errs() << "Patching " << child->getStmtClassName() << "\n";
        map->setParent(child, s);
        manuallyPopulateParentMap(map, child);
    }
}

ClazyASTConsumer::ClazyASTConsumer(ClazyContext *context)
    : m_context(context)
{
}

void ClazyASTConsumer::addCheck(CheckBase *check)
{
    check->registerASTMatchers(m_matchFinder);
    m_createdChecks.push_back(check);
}

ClazyASTConsumer::~ClazyASTConsumer()
{
    delete m_context;
}

bool ClazyASTConsumer::VisitDecl(Decl *decl)
{
    const bool isInSystemHeader = m_context->sm.isInSystemHeader(decl->getLocStart());

    if (AccessSpecifierManager *a = m_context->accessSpecifierManager)
        a->VisitDeclaration(decl);

    for (CheckBase *check : m_createdChecks) {
        if (!(isInSystemHeader && check->ignoresAstNodesInSystemHeaders()))
            check->VisitDeclaration(decl);
    }

    return true;
}

bool ClazyASTConsumer::VisitStmt(Stmt *stm)
{
    if (!m_context->parentMap) {
        if (m_context->ci.getDiagnostics().hasUnrecoverableErrorOccurred())
            return false; // ParentMap sometimes crashes when there were errors. Doesn't like a botched AST.

        m_context->parentMap = new ParentMap(stm);
    }

    ParentMap *parentMap = m_context->parentMap;

    // Workaround llvm bug: Crashes creating a parent map when encountering Catch Statements.
    if (lastStm && isa<CXXCatchStmt>(lastStm) && !parentMap->hasParent(stm)) {
        parentMap->setParent(stm, lastStm);
        manuallyPopulateParentMap(parentMap, stm);
    }

    lastStm = stm;

    // clang::ParentMap takes a root statement, but there's no root statement in the AST, the root is a declaration
    // So add to parent map each time we go into a different hierarchy
    if (!parentMap->hasParent(stm))
        parentMap->addStmt(stm);

    const bool isInSystemHeader = m_context->sm.isInSystemHeader(stm->getLocStart());
    for (CheckBase *check : m_createdChecks) {
        if (!(isInSystemHeader && check->ignoresAstNodesInSystemHeaders()))
            check->VisitStatement(stm);
    }

    return true;
}

void ClazyASTConsumer::HandleTranslationUnit(ASTContext &ctx)
{
    // Run our RecursiveAstVisitor based checks:
    TraverseDecl(ctx.getTranslationUnitDecl());

    // Run our AstMatcher base checks:
    m_matchFinder.matchAST(ctx);
}

static bool parseArgument(const string &arg, vector<string> &args)
{
    auto it = clazy_std::find(args, arg);
    if (it != args.end()) {
        args.erase(it, it + 1);
        return true;
    }

    return false;
}

static CheckLevel parseLevel(vector<std::string> &args)
{
    static const vector<string> levels = { "level0", "level1", "level2", "level3", "level4" };
    const int numLevels = levels.size();
    for (int i = 0; i < numLevels; ++i) {
        if (parseArgument(levels.at(i), args)) {
            return static_cast<CheckLevel>(i);
        }
    }

    return CheckLevelUndefined;
}

static bool checkLessThan(const RegisteredCheck &c1, const RegisteredCheck &c2)
{
    return c1.name < c2.name;
}

static bool checkLessThanByLevel(const RegisteredCheck &c1, const RegisteredCheck &c2)
{
    if (c1.level == c2.level)
        return checkLessThan(c1, c2);

    return c1.level < c2.level;
}


ClazyASTAction::ClazyASTAction()
    : PluginASTAction()
    , m_checkManager(CheckManager::instance())
{
}

std::unique_ptr<clang::ASTConsumer> ClazyASTAction::CreateASTConsumer(CompilerInstance &ci, llvm::StringRef)
{
    ClazyContext::ClazyOptions options = ClazyContext::ClazyOption_None;

    if (m_inplaceFixits)
        options |= ClazyContext::ClazyOption_FixitsAreInplace;

    if (m_checkManager->fixitsEnabled())
        options |= ClazyContext::ClazyOption_FixitsEnabled;

    if (m_checkManager->allFixitsEnabled())
        options |= ClazyContext::ClazyOption_AllFixitsEnabled;

    auto context = new ClazyContext(ci, options);

    auto astConsumer = new ClazyASTConsumer(context);
    CheckBase::List createdChecks = m_checkManager->createChecks(m_checks, context);
    for (CheckBase *check : createdChecks) {
        astConsumer->addCheck(check);
    }

   return std::unique_ptr<ASTConsumer>(astConsumer);
}

bool ClazyASTAction::ParseArgs(const CompilerInstance &, const std::vector<std::string> &args_)
{
    std::vector<std::string> args = args_;

    if (parseArgument("help", args)) {
        PrintHelp(llvm::errs(), HelpMode_Normal);
        return true;
    }

    if (parseArgument("generateAnchorHeader", args)) {
        PrintHelp(llvm::errs(), HelpMode_AnchorHeader);
        return true;
    }

    if (parseArgument("no-inplace-fixits", args)) {
        // Unit-tests don't use inplace fixits
        m_inplaceFixits = false;
    }

    // This argument is for debugging purposes
    const bool printRequestedChecks = parseArgument("print-requested-checks", args);

    const CheckLevel requestedLevel = parseLevel(/*by-ref*/args);
    if (requestedLevel != CheckLevelUndefined) {
        m_checkManager->setRequestedLevel(requestedLevel);
    }

    if (parseArgument("enable-all-fixits", args)) {
        // This is useful for unit-tests, where we also want to run fixits. Don't use it otherwise.
        m_checkManager->enableAllFixIts();
    }

    if (args.size() > 1) {
        // Too many arguments.
        llvm::errs() << "Too many arguments: ";
        for (const std::string &a : args)
            llvm::errs() << a << ' ';
        llvm::errs() << "\n";

        PrintHelp(llvm::errs());
        return false;
    } else if (args.size() == 1) {
        vector<string> userDisabledChecks;
        m_checks = m_checkManager->checksForCommaSeparatedString(args[0], /*by-ref=*/userDisabledChecks);
        if (m_checks.empty()) {
            llvm::errs() << "Could not find checks in comma separated string " + args[0] + "\n";
            PrintHelp(llvm::errs());
            return false;
        }
    }

    vector<string> userDisabledChecks;
    // Append checks specified from env variable
    RegisteredCheck::List checksFromEnv = m_checkManager->requestedChecksThroughEnv(/*by-ref*/userDisabledChecks);
    copy(checksFromEnv.cbegin(), checksFromEnv.cend(), back_inserter(m_checks));

    if (m_checks.empty() && requestedLevel == CheckLevelUndefined) {
        // No check or level specified, lets use the default level
        m_checkManager->setRequestedLevel(DefaultCheckLevel);
    }

    // Add checks from requested level
    auto checksFromRequestedLevel = m_checkManager->checksFromRequestedLevel();
    clazy_std::append(checksFromRequestedLevel, m_checks);
    clazy_std::sort_and_remove_dups(m_checks, checkLessThan);
    CheckManager::removeChecksFromList(m_checks, userDisabledChecks);

    if (printRequestedChecks) {
        llvm::errs() << "Requested checks: ";
        const unsigned int numChecks = m_checks.size();
        for (unsigned int i = 0; i < numChecks; ++i) {
            llvm::errs() << m_checks.at(i).name;
            const bool isLast = i == numChecks - 1;
            if (!isLast) {
                llvm::errs() << ", ";
            }
        }

        llvm::errs() << "\n";
    }

    return true;
}

void ClazyASTAction::PrintAnchorHeader(llvm::raw_ostream &ros, RegisteredCheck::List &checks)
{
    // Generates ClazyAnchorHeader.h.
    // Needed so we can support a static build of clazy without the linker discarding our checks.
    // You can generate with:
    // $ echo | clang -Xclang -load -Xclang ClangLazy.so -Xclang -add-plugin -Xclang clang-lazy -Xclang -plugin-arg-clang-lazy -Xclang generateAnchorHeader -c -xc -


    ros << "// This file was autogenerated.\n\n";
    ros << "#ifndef CLAZY_ANCHOR_HEADER_H\n#define CLAZY_ANCHOR_HEADER_H\n\n";

    for (auto &check : checks) {
        ros << string("extern volatile int ClazyAnchor_") + check.className + ";\n";
    }

    ros << "\n";
    ros << "int clazy_dummy()\n{\n";
    ros << "    return\n";

    for (auto &check : checks) {
        ros << string("        ClazyAnchor_") + check.className + " +\n";
    }

    ros << "    0;\n";
    ros << "}\n\n";
    ros << "#endif\n";
}

void ClazyASTAction::PrintHelp(llvm::raw_ostream &ros, HelpMode helpMode)
{
    RegisteredCheck::List checks = m_checkManager->availableChecks(MaxCheckLevel);
    clazy_std::sort(checks, checkLessThanByLevel);

    if (helpMode == HelpMode_AnchorHeader) {
        PrintAnchorHeader(ros, checks);
        return;
    }

    ros << "Available checks and FixIts:\n\n";
    const bool useMarkdown = getenv("CLAZY_HELP_USE_MARKDOWN");

    int lastPrintedLevel = -1;
    const auto numChecks = checks.size();
    for (unsigned int i = 0; i < numChecks; ++i) {
        const RegisteredCheck &check = checks[i];
        const string levelStr = "level" + to_string(check.level);
        if (lastPrintedLevel < check.level) {
            lastPrintedLevel = check.level;

            if (check.level > 0)
                ros << "\n";

            ros << "- Checks from " << levelStr << ":\n";
        }

        const string relativeReadmePath = "src/checks/" + levelStr + "/README-" + check.name + ".md";

        auto padded = check.name;
        padded.insert(padded.end(), 39 - padded.size(), ' ');
        ros << "    - " << (useMarkdown ? "[" : "") << check.name << (useMarkdown ? "](" + relativeReadmePath + ")" : "");
        auto fixits = m_checkManager->availableFixIts(check.name);
        if (!fixits.empty()) {
            ros << "    (";
            bool isFirst = true;
            for (const auto& fixit : fixits) {
                if (isFirst) {
                    isFirst = false;
                } else {
                    ros << ',';
                }

                ros << fixit.name;
            }
            ros << ')';
        }
        ros << "\n";
    }
    ros << "\nIf nothing is specified, all checks from level0 and level1 will be run.\n\n";
    ros << "To specify which checks to enable set the CLAZY_CHECKS env variable, for example:\n";
    ros << "    export CLAZY_CHECKS=\"level0\"\n";
    ros << "    export CLAZY_CHECKS=\"level0,reserve-candidates,qstring-allocations\"\n";
    ros << "    export CLAZY_CHECKS=\"reserve-candidates\"\n\n";
    ros << "or pass as compiler arguments, for example:\n";
    ros << "    -Xclang -plugin-arg-clang-lazy -Xclang reserve-candidates,qstring-allocations\n";
    ros << "\n";
    ros << "To enable FixIts for a check, also set the env variable CLAZY_FIXIT, for example:\n";
    ros << "    export CLAZY_FIXIT=\"fix-qlatin1string-allocations\"\n\n";
    ros << "FixIts are experimental and rewrite your code therefore only one FixIt is allowed per build.\nSpecifying a list of different FixIts is not supported.\nBackup your code before running them.\n";
}

unique_ptr<ASTConsumer> ClazyStandaloneASTAction::CreateASTConsumer(CompilerInstance &ci, llvm::StringRef)
{
    auto context = new ClazyContext(ci);
    auto astConsumer = new ClazyASTConsumer(context);

    auto cm = CheckManager::instance();
    RegisteredCheck::List requestedChecks = cm->requestedChecksThroughEnv();

    CheckBase::List createdChecks = cm->createChecks(requestedChecks, context);
    for (CheckBase *check : createdChecks) {
        astConsumer->addCheck(check);
    }

   return unique_ptr<ASTConsumer>(astConsumer);
}

static FrontendPluginRegistry::Add<ClazyASTAction>
X("clang-lazy", "clang lazy plugin");
