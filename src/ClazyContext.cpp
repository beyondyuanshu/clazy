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

#include "AccessSpecifierManager.h"
#include "PreProcessorVisitor.h"
#include "ClazyContext.h"

#include <clang/AST/ParentMap.h>
#include <clang/Basic/SourceManager.h>
#include "clang/Rewrite/Frontend/FixItRewriter.h"

using namespace clang;

class ClazyFixItOptions : public FixItOptions
{
public:
    ClazyFixItOptions(const ClazyFixItOptions &other) = delete;
    ClazyFixItOptions(bool inplace)
    {
        InPlace = inplace;
        FixWhatYouCan = true;
        FixOnlyWarnings = true;
        Silent = false;
    }

    std::string RewriteFilename(const std::string &filename, int &fd) override
    {
        fd = -1;
        return InPlace ? filename : filename + "_fixed.cpp";
    }
};

ClazyContext::ClazyContext(const clang::CompilerInstance &compiler, ClazyOptions opts)
    : ci(compiler)
    , astContext(ci.getASTContext())
    , sm(ci.getSourceManager())
    , m_noWerror(getenv("CLAZY_NO_WERROR") != nullptr) // Allows user to make clazy ignore -Werror
    , options(opts)
    , extraOptions(clazy_std::splitString(getenv("CLAZY_EXTRA_OPTIONS"), ','))
    , rewriter(fixitsEnabled() ? new FixItRewriter(ci.getDiagnostics(), sm,
                                                   ci.getLangOpts(), new ClazyFixItOptions(fixitsAreInplace()))
                               : nullptr)
{
}

ClazyContext::~ClazyContext()
{
    //delete preprocessorVisitor; // we don't own it
    delete accessSpecifierManager;
    delete parentMap;

    if (rewriter) {
        rewriter->WriteFixedFiles();
        delete rewriter;
    }

    preprocessorVisitor = nullptr;
    accessSpecifierManager = nullptr;
    parentMap = nullptr;
}

void ClazyContext::enableAccessSpecifierManager()
{
    if (!accessSpecifierManager && !usingPreCompiledHeaders())
        accessSpecifierManager = new AccessSpecifierManager(ci);
}

void ClazyContext::enablePreprocessorVisitor()
{
    if (!preprocessorVisitor && !usingPreCompiledHeaders())
        preprocessorVisitor = new PreProcessorVisitor(ci);
}
