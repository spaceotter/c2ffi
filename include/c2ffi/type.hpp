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

#ifndef C2FFI_TYPE_H
#define C2FFI_TYPE_H

#include <clang/AST/Decl.h>
#include <clang/AST/Type.h>
#include <clang/Frontend/CompilerInstance.h>
#include <stdint.h>

#include <ostream>
#include <string>
#include <vector>

#include "c2ffi.hpp"

namespace c2ffi {
class C2FFIASTConsumer;

class Type : public Writable {
  unsigned int _id;

 protected:
  const clang::CompilerInstance &_ci;
  const clang::Type *_type;

  uint64_t _bit_offset;
  uint64_t _bit_size;
  unsigned _bit_alignment;

  friend class PointerType;

 public:
  Type(const clang::CompilerInstance &ci, const clang::Type *t);
  virtual ~Type() {}

  static Type *make_type(C2FFIASTConsumer *, const clang::Type *);

  unsigned int id() const { return _id; }
  void set_id(unsigned int id) { _id = id; }

  uint64_t bit_offset() const { return _bit_offset; }
  void set_bit_offset(uint64_t offset) { _bit_offset = offset; }

  uint64_t bit_size() const { return _bit_size; }
  void set_bit_size(uint64_t size) { _bit_size = size; }

  uint64_t bit_alignment() const { return _bit_alignment; }
  void set_bit_alignment(uint64_t alignment) { _bit_alignment = alignment; }

  const clang::Type *orig() const { return _type; }

  std::string metatype() const;
};

typedef std::string Name;
typedef std::vector<Name> NameVector;

typedef std::pair<Name, Type *> NameTypePair;
typedef std::vector<NameTypePair> NameTypeVector;

typedef std::pair<Name, uint64_t> NameNumPair;
typedef std::vector<NameNumPair> NameNumVector;

// :void, etc
class SimpleType : public Type {
  std::string _name;

 public:
  SimpleType(const clang::CompilerInstance &ci, const clang::Type *t, std::string name);

  const std::string &name() const { return _name; }

  DEFWRITER(SimpleType);
};

// typedef'd types, which can exist in a namespace that has to be provided to correctly
// disambiguate the name.
class TypedefType : public SimpleType {
  unsigned _ns;
  const Type * _underType;

 public:
  TypedefType(C2FFIASTConsumer *astc, const clang::TypedefType *t);

  const clang::TypedefType *orig() const { return (const clang::TypedefType *)_type; }

  unsigned ns() const { return _ns; }

  const Type * underType() const { return _underType; }

  DEFWRITER(TypedefType);
};

// :int, :unsigned-char, etc
class BasicType : public SimpleType {
 public:
  BasicType(const clang::CompilerInstance &ci, const clang::BuiltinType *t, std::string name);

  const clang::BuiltinType *orig() const { return (const clang::BuiltinType *)_type; }

  DEFWRITER(BasicType);
};

class BitfieldType : public Type {
  Type *_base;
  unsigned int _width;

 public:
  BitfieldType(const clang::CompilerInstance &ci, const clang::Type *t, unsigned int width,
               Type *base)
      : Type(ci, t), _base(base), _width(width) {}

  virtual ~BitfieldType() { delete _base; }

  const Type *base() const { return _base; }
  unsigned int width() const { return _width; }

  DEFWRITER(BitfieldType);
};

// This could be simple, but we want to be specific about what
// we're pointing _to_
class PointerType : public Type {
  Type *_pointee;

 public:
  PointerType(const clang::CompilerInstance &ci, const clang::Type *t, Type *pointee)
      : Type(ci, t), _pointee(pointee) {}
  virtual ~PointerType() { delete _pointee; }

  const Type &pointee() const { return *_pointee; }
  bool is_string() const;

  DEFWRITER(PointerType);
};

class ReferenceType : public PointerType {
 public:
  ReferenceType(const clang::CompilerInstance &ci, const clang::ReferenceType *t, Type *pointee)
      : PointerType(ci, t, pointee) {}
  const clang::ReferenceType *orig() const { return (const clang::ReferenceType *)_type; }
  DEFWRITER(ReferenceType);
};

class ArrayType : public PointerType {
  uint64_t _size;

 public:
  ArrayType(const clang::CompilerInstance &ci, const clang::ConstantArrayType *t, Type *pointee,
            uint64_t size)
      : PointerType(ci, t, pointee), _size(size) {}
  const clang::ConstantArrayType *orig() const { return (const clang::ConstantArrayType *)_type; }

  uint64_t size() const { return _size; }
  DEFWRITER(ArrayType);
};

class RecordType : public SimpleType, public TemplateMixin {
  bool _is_union;
  bool _is_class;

 public:
  RecordType(C2FFIASTConsumer *ast, const clang::RecordType *t, std::string name,
             bool is_union = false, bool is_class = false,
             const clang::TemplateArgumentList *arglist = NULL);
  const clang::RecordType *orig() const { return (const clang::RecordType *)_type; }

  bool is_union() const { return _is_union; }
  bool is_class() const { return _is_class; }
  DEFWRITER(RecordType);
};

class EnumType : public SimpleType {
 public:
  EnumType(const clang::CompilerInstance &ci, const clang::EnumType *t, std::string name)
      : SimpleType(ci, t, name) {}
  const clang::EnumType *orig() const { return (const clang::EnumType *)_type; }
  DEFWRITER(EnumType);
};

class ComplexType : public Type {
  Type *_element;

 public:
  ComplexType(const clang::CompilerInstance &ci, const clang::ComplexType *t, Type *element)
      : Type(ci, t), _element(element) {}
  virtual ~ComplexType() { delete _element; }
  const clang::ComplexType *orig() const { return (const clang::ComplexType *)_type; }

  const Type &element() const { return *_element; }
  bool is_string() const;

  DEFWRITER(ComplexType);
};

// This is a bit of a hack to contain inline declarations (e.g.,
// anonymous typedef struct)
class DeclType : public Type {
  Decl *_d;

 public:
  DeclType(clang::CompilerInstance &ci, const clang::TagType *t, Decl *d, const clang::Decl *cd);
  const clang::TagType *orig() const { return (const clang::TagType *)_type; }

  // Note, this cheats:
  virtual void write(OutputDriver &od) const;
};
}  // namespace c2ffi

#endif /* C2FFI_TYPE_H */
