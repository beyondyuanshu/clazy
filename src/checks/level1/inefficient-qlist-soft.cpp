/*
   This file is part of the clazy static checker.

  Copyright (C) 2016 Sergio Martins <smartins@kde.org>

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

#include "inefficient-qlist-soft.h"
#include "Utils.h"
#include "checkmanager.h"
#include "StringUtils.h"

#include <clang/AST/AST.h>
#include <clang/Lex/Lexer.h>

using namespace clang;
using namespace std;


InefficientQListSoft::InefficientQListSoft(const std::string &name, ClazyContext *context)
    : InefficientQListBase(name, context, IgnoreNonLocalVariable | IgnoreInFunctionWithSameReturnType |
                                          IgnoreIsAssignedToInFunction | IgnoreIsPassedToFunctions|
                                          IgnoreIsInitializedByFunctionCall)
{
}

void InefficientQListSoft::VisitStmt(clang::Stmt *stmt)
{
}


REGISTER_CHECK_WITH_FLAGS("inefficient-qlist-soft", InefficientQListSoft, CheckLevel1)
