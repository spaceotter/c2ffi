/* -*- c++ -*-

   c2ffi
   Copyright (C) 2013  Ryan Pavlik

   This file is part of c2ffi.

   c2ffi is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 2 of the License, or
   (at your option) any later version.

   c2ffi is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with c2ffi.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef C2FFI_PREDECL_H
#define C2FFI_PREDECL_H

namespace c2ffi {
    class Type;
    class SimpleType;
    class TypedefType;
    class BasicType;
    class BitfieldType;
    class PointerType;
    class ArrayType;
    class RecordType;
    class EnumType;
    class ComplexType;
    class DeclType;
    class ReferenceType;
    class TemplateType;

    class Decl;
    class UnhandledDecl;
    class TypeDecl;
    class VarDecl;
    class FunctionDecl;
    class TypedefDecl;
    class RecordDecl;
    class EnumDecl;
    class CXXRecordDecl;
    class CXXFunctionDecl;
    class CXXNamespaceDecl;
    class ObjCInterfaceDecl;
    class ObjCCategoryDecl;
    class ObjCProtocolDecl;
}
#endif /* C2FFI_PREDECL_H */
