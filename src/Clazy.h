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

#ifndef CLAZY_AST_ACTION_H
#define CLAZY_AST_ACTION_H

#include "checkmanager.h"

#include <clang/AST/ASTConsumer.h>
#include <clang/Frontend/FrontendAction.h>
#include "clang/AST/RecursiveASTVisitor.h"

#include <memory>
#include <vector>
#include <string>

namespace clang {
    class CompilerInstance;
}

class ClazyASTAction : public clang::PluginASTAction
{
public:

    enum HelpMode {
        HelpMode_Normal = 0,
        HelpMode_AnchorHeader = 1
    };

    ClazyASTAction();

protected:
    std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance &ci, llvm::StringRef) override;
    bool ParseArgs(const clang::CompilerInstance &ci, const std::vector<std::string> &args_) override;
    void PrintHelp(llvm::raw_ostream &ros, HelpMode = HelpMode_Normal);
    void PrintAnchorHeader(llvm::raw_ostream &ro, RegisteredCheck::List &checks);
private:
    RegisteredCheck::List m_checks;
    bool m_inplaceFixits = true;
    CheckManager *const m_checkManager;
};

class ClazyASTConsumer : public clang::ASTConsumer,
                         public clang::RecursiveASTVisitor<ClazyASTConsumer>
{
public:
    explicit ClazyASTConsumer(ClazyContext *context);
    ~ClazyASTConsumer();

    bool VisitDecl(clang::Decl *decl);
    bool VisitStmt(clang::Stmt *stm);
    void HandleTranslationUnit(clang::ASTContext &ctx) override;
    void addCheck(CheckBase *check);

    ClazyContext *context() const { return m_context; }

private:
    ClazyASTConsumer(const ClazyASTConsumer &) = delete;
    clang::Stmt *lastStm = nullptr;
    ClazyContext *const m_context;
    CheckBase::List m_createdChecks;
    clang::ast_matchers::MatchFinder m_matchFinder;
};

#endif
