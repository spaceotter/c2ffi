/*
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

#include <clang/AST/ASTContext.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclObjC.h>
#include <clang/AST/RecordLayout.h>
#include <clang/Lex/Preprocessor.h>
#include <llvm/Support/ConvertUTF.h>

#include "c2ffi.hpp"
#include "c2ffi/ast.hpp"

using namespace c2ffi;

static std::string value_to_string(clang::APValue* v) {
  std::string s;
  llvm::raw_string_ostream ss(s);

  if (v->isInt() && v->getInt().isSigned())
    v->getInt().print(ss, true);
  else if (v->isInt())
    v->getInt().print(ss, false);
  else if (v->isFloat())
    v->getFloat().print(ss);

  ss.flush();
  s.erase(s.find_last_not_of(" \n\r\t") + 1);
  return s;
}

static bool convertUTF32ToUTF8String(const llvm::ArrayRef<char>& Source, std::string& Result) {
  const char* SourceBegins = Source.data();
  const size_t SourceLength = Source.size();
  const char* SourceEnding = SourceBegins + SourceLength;
  const size_t ResultMaxLen = SourceLength * UNI_MAX_UTF8_BYTES_PER_CODE_POINT;
  Result.resize(ResultMaxLen);
  const llvm::UTF32* SourceBeginsUTF32 = reinterpret_cast<const llvm::UTF32*>(SourceBegins);
  const llvm::UTF32* SourceEndingUTF32 = reinterpret_cast<const llvm::UTF32*>(SourceEnding);
  llvm::UTF8* ResultBeginsUTF8 = reinterpret_cast<llvm::UTF8*>(&Result[0]);
  llvm::UTF8* ResultEndingUTF8 = reinterpret_cast<llvm::UTF8*>(&Result[0] + ResultMaxLen);
  const llvm::ConversionResult CR =
      llvm::ConvertUTF32toUTF8(&SourceBeginsUTF32, SourceEndingUTF32, &ResultBeginsUTF8,
                               ResultEndingUTF8, llvm::strictConversion);
  if (CR != llvm::conversionOK) {
    Result.clear();
    return false;
  }
  Result.resize(reinterpret_cast<char*>(ResultEndingUTF8) - &Result[0]);
  return true;
}

Decl::Decl(const clang::NamedDecl* d) { _name = d->getDeclName().getAsString(); }

void Decl::set_location(clang::CompilerInstance& ci, const clang::Decl* d) {
  clang::SourceLocation sloc = d->getLocation();

  if (sloc.isValid()) {
    std::string loc = sloc.printToString(ci.getSourceManager());
    set_location(loc);
  }
}

VarDecl::VarDecl(C2FFIASTConsumer& astc, const clang::VarDecl* d)
    : TypeDecl(astc, d, d->getTypeSourceInfo()->getType().getTypePtr()),
      _is_extern(d->hasExternalStorage()) {
  clang::APValue* v = NULL;
  std::string loc = "";
  _is_string = false;

  if (_name.substr(0, 8) == "__c2ffi_") {
    _name = _name.substr(8, std::string::npos);

    clang::Preprocessor& pp = astc.ci().getPreprocessor();
    clang::IdentifierInfo& ii = pp.getIdentifierTable().get(llvm::StringRef(_name));
    const clang::MacroInfo* mi = pp.getMacroInfo(&ii);

    if (mi) loc = mi->getDefinitionLoc().printToString(astc.ci().getSourceManager());
  }

  if (d->hasInit()) {
    if (!d->getType()->isDependentType()) {
      clang::EvaluatedStmt* stmt = d->ensureEvaluatedStmt();
      clang::Expr* e = clang::cast<clang::Expr>(stmt->Value);
      if (!e->isValueDependent() && ((v = d->evaluateValue()) || (v = d->getEvaluatedValue()))) {
        if (v->isLValue()) {
          clang::APValue::LValueBase base = v->getLValueBase();
          if (!base.isNull() && base.is<const clang::Expr*>()) {
            const clang::Expr* e = base.get<const clang::Expr*>();

            if_const_cast(s, clang::StringLiteral, e) {
              _is_string = true;

              if (s->isAscii() || s->isUTF8()) {
                _value = s->getString();
              } else if (s->getCharByteWidth() == 2) {
                llvm::StringRef bytes = s->getBytes();
                llvm::convertUTF16ToUTF8String(llvm::ArrayRef<char>(bytes.data(), bytes.size()),
                                               _value);
              } else if (s->getCharByteWidth() == 4) {
                llvm::StringRef bytes = s->getBytes();
                convertUTF32ToUTF8String(llvm::ArrayRef<char>(bytes.data(), bytes.size()), _value);
              } else {
              }
            }
          }
        } else {
          _value = value_to_string(v);
        }
      }
    }
  }

  if (loc != "") set_location(loc);
}

FieldsMixin::~FieldsMixin() {
  for (NameTypeVector::iterator i = _v.begin(); i != _v.end(); i++) delete (*i).second;
}

FunctionsMixin::~FunctionsMixin() {
  for (FunctionVector::iterator i = _v.begin(); i != _v.end(); i++) delete (*i);
}

void FieldsMixin::add_field(Name name, Type* t) { _v.push_back(NameTypePair(name, t)); }

void FieldsMixin::add_field(C2FFIASTConsumer* ast, clang::FieldDecl* f) {
  clang::ASTContext& ctx = ast->ci().getASTContext();
  auto type_info = ctx.getTypeInfo(f->getTypeSourceInfo()->getType().getTypePtr());
  Type* t = Type::make_type(ast, f->getTypeSourceInfo()->getType().getTypePtr());
  ;

  if (f->isBitField())
    t = new BitfieldType(ast->ci(), f->getTypeSourceInfo()->getType().getTypePtr(),
                         f->getBitWidthValue(ctx), t);

  t->set_bit_offset(ctx.getFieldOffset(f));
  t->set_bit_size(type_info.Width);
  t->set_bit_alignment(type_info.Align);

  add_field(f->getDeclName().getAsString(), t);
}

void FieldsMixin::add_field(C2FFIASTConsumer* ast, const clang::ParmVarDecl* p) {
  std::string name = p->getDeclName().getAsString();
  Type* t = Type::make_type(ast, p->getOriginalType().getTypePtr());
  add_field(name, t);
}

void FunctionsMixin::add_function(FunctionDecl* f) { _v.push_back(f); }

void FunctionsMixin::add_functions(C2FFIASTConsumer* ast, const clang::ObjCContainerDecl* d) {
  for (clang::ObjCContainerDecl::method_iterator m = d->meth_begin(); m != d->meth_end(); m++) {
    FunctionDecl* f = new FunctionDecl(ast, *m);
    add_function(f);
  }
}

void FunctionsMixin::add_functions(C2FFIASTConsumer* ast, const clang::CXXRecordDecl* d) {
  for (clang::CXXRecordDecl::method_iterator i = d->method_begin(); i != d->method_end(); ++i) {
    const clang::CXXMethodDecl* m = (*i);
    if (m->getAccess() == clang::AccessSpecifier::AS_public ||
        m->getAccess() == clang::AccessSpecifier::AS_none) {
      CXXFunctionDecl* f = new CXXFunctionDecl(ast, m);
      add_function(f);
    }
  }
}

static const char* sc2str[] = {"none", "extern", "static", "private_extern"};

FunctionDecl::FunctionDecl(C2FFIASTConsumer* ast, const clang::FunctionDecl* d)
    : Decl(d),
      TemplateMixin(ast, d->getTemplateSpecializationInfo()
                             ? d->getTemplateSpecializationInfo()->TemplateArguments
                             : nullptr),
      _return(Type::make_type(ast, d->getReturnType().getTypePtr())),
      _is_variadic(d->isVariadic()),
      _is_inline(d->isInlineSpecified()),
      _is_objc_method(false),
      _is_class_method(false),
      _linkage(LINK_C),
      _storage_class("unknown"),
      _d(d) {
  clang::StorageClass storage_class = d->getStorageClass();
  if (storage_class < sizeof(sc2str) / sizeof(*sc2str)) _storage_class = sc2str[storage_class];

  for (clang::FunctionDecl::param_const_iterator i = d->param_begin(); i != d->param_end(); i++) {
    add_field(ast, *i);
  }
}

FunctionDecl::FunctionDecl(C2FFIASTConsumer* ast, const clang::ObjCMethodDecl* d)
    : Decl(d),
      TemplateMixin(ast, nullptr),
      _return(Type::make_type(ast, d->getReturnType().getTypePtr())),
      _is_variadic(d->isVariadic()),
      _is_inline(false),
      _is_objc_method(true),
      _is_class_method(d->isClassMethod()),
      _linkage(LINK_C),
      _storage_class("none"),
      _d(nullptr) {
  set_location(ast->ci(), d);

  for (clang::ObjCMethodDecl::param_const_iterator i = d->param_begin(); i != d->param_end(); i++) {
    add_field(ast, *i);
  }
}

RecordDecl::RecordDecl(C2FFIASTConsumer& ast, const clang::RecordDecl* d, bool is_toplevel)
    : Decl(d), _is_union(d->isUnion()), _bit_size(0), _bit_alignment(0), _d(d) {
  if (is_toplevel && _name.empty()) throw invalid_decl("Invalid RecordDecl");
  clang::ASTContext& ctx = ast.ci().getASTContext();
  const clang::Type* t = d->getTypeForDecl();

  if (!t->isIncompleteType() && !t->isInstantiationDependentType()) {
    set_bit_size(ctx.getTypeSize(t));
    set_bit_alignment(ctx.getTypeAlign(t));
  } else {
    set_bit_size(0);
    set_bit_alignment(0);
  }

  if (_name == "") set_id(ast.add_decl(d));

  for (clang::RecordDecl::field_iterator i = d->field_begin(); i != d->field_end(); i++) {
    if (i->getAccess() == clang::AccessSpecifier::AS_public ||
        i->getAccess() == clang::AccessSpecifier::AS_none) {
      add_field(&ast, *i);
    }
  }
}

void EnumDecl::add_field(Name name, uint64_t v) { _v.push_back(NameNumPair(name, v)); }

CXXRecordDecl::CXXRecordDecl(C2FFIASTConsumer& ast, const clang::CXXRecordDecl* d, bool is_toplevel)
    : RecordDecl(ast, d, is_toplevel),
      TemplateMixin(
          &ast,
          dynamic_cast<const clang::ClassTemplateSpecializationDecl*>(d) == nullptr
              ? nullptr
              : &dynamic_cast<const clang::ClassTemplateSpecializationDecl*>(d)->getTemplateArgs()),
      _is_class(d->isClass()) {
  if (!d->hasDefinition() || d->getDefinition()->isInvalidDecl())
    throw invalid_decl("Invalid CXXRecord");

  // template specializations will contain a record decl with the same name for unknown
  // reasons. it is of no interest to the public API since you can't declare anything with its
  // type, so skip it.
  if_const_cast(p, clang::CXXRecordDecl, d->getParent()) {
    if (p->getDeclName().getAsString() == _name &&
        p->getTemplateSpecializationKind() ==
            clang::TemplateSpecializationKind::TSK_ExplicitInstantiationDefinition) {
      throw invalid_decl("Internal template");
    }
  }

  set_id(ast.add_cxx_decl(d));
  add_functions(&ast, d);

  if (!d->isDependentType()) {
    const clang::ASTRecordLayout& layout = ast.ci().getASTContext().getASTRecordLayout(d);

    for (clang::CXXRecordDecl::base_class_const_iterator i = d->bases_begin(); i != d->bases_end();
         ++i) {
      bool is_virtual = (*i).isVirtual();
      const clang::CXXRecordDecl* decl = (*i).getType().getTypePtr()->getAsCXXRecordDecl();
      int64_t offset = 0;

      if (is_virtual)
        offset = layout.getVBaseClassOffset(decl).getQuantity();
      else
        offset = layout.getBaseClassOffset(decl).getQuantity();

      add_parent(decl->getNameAsString(), (CXXRecordDecl::Access)(*i).getAccessSpecifier(), offset,
                 is_virtual);
    }
  }
}

CXXFunctionDecl::CXXFunctionDecl(C2FFIASTConsumer* ast, const clang::CXXMethodDecl* d)
    : FunctionDecl(ast, d),
      _is_static(d->isStatic()),
      _is_virtual(d->isVirtual()),
      _is_const(d->isConst()),
      _is_pure(d->isPure()) {
  set_location(ast->ci(), d);
}

void ObjCInterfaceDecl::add_protocol(Name name) { _protocols.push_back(name); }
