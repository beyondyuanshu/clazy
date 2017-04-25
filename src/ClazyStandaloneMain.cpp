/*
   This file is part of the clazy static checker.

  Copyright (C) 2017 Sergio Martins <smartins@kde.org>

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

#include "Clazy.h"
#include "ClazyAnchorHeader.h"

#include <clang/Frontend/FrontendActions.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/CommandLine.h>

#include <memory>

using namespace clang;
using namespace clang::tooling;
using namespace llvm;

static llvm::cl::OptionCategory s_clazyCategory("clazy options");

static cl::extrahelp s_commonHelp(CommonOptionsParser::HelpMessage);
static cl::extrahelp s_clazyHelp("\nMore help text...");



class ClazyToolAction : public clang::tooling::FrontendActionFactory
{
public:
    ClazyToolAction()
        : FrontendActionFactory() {}

    FrontendAction *create() override
    {
        return new ClazyStandaloneASTAction();
    }
};

int main(int argc, const char **argv)
{
    CommonOptionsParser optionsParser(argc, argv, s_clazyCategory);
    ClangTool tool(optionsParser.getCompilations(), optionsParser.getSourcePathList());

    tool.run(new ClazyToolAction());

    return 0;
}
