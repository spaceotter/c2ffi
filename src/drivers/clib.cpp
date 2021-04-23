#include <clang/AST/PrettyPrinter.h>

#include <fstream>
#include <unordered_map>

#include "c2ffi.hpp"

using namespace c2ffi;

ManglerConfig c2ffi::mangleConf;

struct mangling_error : public std::runtime_error {
  mangling_error(const std::string &what_arg) : std::runtime_error(what_arg) {}
};

// clang-format off
std::unordered_map<std::string, std::string> operator_map = {
  {"+", "add"},
  {"*", "mul"},
  {"/", "div"},
  {"-", "sub"},
  {"=", "set"},
  {"()", "call"},
  {"[]", "idx"},
  {" ", ""},
  {".", "dot"}
};
// clang-format on

static std::string sanitize_identifier(std::string name) {
  std::string::size_type s;
  for (auto &pair : operator_map) {
    std::string replace = mangleConf.c_separator + pair.second + mangleConf.c_separator;
    while (1) {
      s = name.find(pair.first);
      if (s == std::string::npos) {
        break;
      }
      name.replace(s, pair.first.size(), replace);
    }
  }
  return name;
}

// replaces printTemplateArgumentList(os, TemplateArgs.asArray(), P);
static void printCTemplateArgs(std::ostream &os,
                               const clang::ArrayRef<clang::TemplateArgument> &Args,
                               const clang::PrintingPolicy &PP) {
  using namespace clang;
  bool FirstArg = true;
  for (const auto &Arg : Args) {
    // Print the argument into a string.
    SmallString<128> Buf;
    llvm::raw_svector_ostream ArgOS(Buf);
    const TemplateArgument &Argument = Arg;
    if (Argument.getKind() == TemplateArgument::Pack) {
      if (Argument.pack_size() && !FirstArg) os << mangleConf.c_separator;
      printCTemplateArgs(os, Argument.getPackAsArray(), PP);
    } else {
      if (!FirstArg) os << mangleConf.c_separator;
      // Tries to print the argument with location info if exists.
      Arg.print(PP, ArgOS);
    }

    StringRef ArgString = ArgOS.str();
    os << ArgString.str();

    FirstArg = false;
  }
}

// closely follows the NamedDecl::printQualifiedName method
static std::string getCName(const clang::NamedDecl *d) {
  using namespace clang;
  std::stringstream os;
  const PrintingPolicy PP(d->getLangOpts());
  const DeclContext *Ctx = d->getDeclContext();
  if (Ctx->isFunctionOrMethod()) {
    throw mangling_error("Identifier is in function or method: " + d->getQualifiedNameAsString());
  }
  using ContextsTy = SmallVector<const DeclContext *, 8>;
  ContextsTy Contexts;

  // Collect named contexts.
  while (Ctx) {
    if (isa<NamedDecl>(Ctx)) Contexts.push_back(Ctx);
    Ctx = Ctx->getParent();
  }

  os << mangleConf.root_prefix;

  for (const DeclContext *DC : llvm::reverse(Contexts)) {
    if (const auto *Spec = dyn_cast<ClassTemplateSpecializationDecl>(DC)) {
      os << Spec->getName().str();
      const TemplateArgumentList &TemplateArgs = Spec->getTemplateArgs();
      os << mangleConf.c_separator;
      printCTemplateArgs(os, TemplateArgs.asArray(), PP);
    } else if (const auto *ND = dyn_cast<NamespaceDecl>(DC)) {
      if (ND->isAnonymousNamespace()) {
        throw mangling_error("Anonymous namespace");
      } else
        os << ND->getDeclName().getAsString();
    } else if (const auto *RD = dyn_cast<clang::RecordDecl>(DC)) {
      if (!RD->getIdentifier())
        throw mangling_error("Anonymous struct or class");
      else
        os << RD->getDeclName().getAsString();
    } else if (const auto *FD = dyn_cast<clang::FunctionDecl>(DC)) {
      throw mangling_error("Decl inside a fuction");
    } else if (const auto *ED = dyn_cast<clang::EnumDecl>(DC)) {
      // C++ [dcl.enum]p10: Each enum-name and each unscoped
      // enumerator is declared in the scope that immediately contains
      // the enum-specifier. Each scoped enumerator is declared in the
      // scope of the enumeration.
      // For the case of unscoped enumerator, do not include in the qualified
      // name any information about its enum enclosing scope, as its visibility
      // is global.
      if (ED->isScoped())
        os << ED->getDeclName().getAsString();
      else
        continue;
    } else {
      os << cast<NamedDecl>(DC)->getDeclName().getAsString();
    }
    os << mangleConf.c_separator;
  }

  if (d->getDeclName())
    os << d->getDeclName().getAsString();
  else {
    // Give the printName override a chance to pick a different name before we
    // fall back to "(anonymous)".
    SmallString<64> NameBuffer;
    llvm::raw_svector_ostream NameOS(NameBuffer);
    d->printName(NameOS);
    if (NameBuffer.empty())
      throw mangling_error("Decl is anonymous");
    else
      os << NameBuffer.c_str();
  }

  if (const auto *t = dynamic_cast<const clang::ClassTemplateSpecializationDecl *>(d)) {
    os << mangleConf.c_separator;
    printCTemplateArgs(os, t->getTemplateArgs().asArray(), PP);
  }

  return sanitize_identifier(os.str());
}

struct c2ffi::Identifier {
  // remember old identifiers to save time, they don't change
  static std::unordered_map<const clang::NamedDecl *, Identifier> ids;

  Identifier(const clang::NamedDecl *d) {
    if (ids.count(d)) {
      c = ids.at(d).c;
      cpp = ids.at(d).cpp;
    } else {
      c = getCName(d);
      cpp = d->getQualifiedNameAsString();
      if (const auto *t = dynamic_cast<const clang::ClassTemplateSpecializationDecl *>(d)) {
        clang::SmallString<128> Buf;
        llvm::raw_svector_ostream ArgOS(Buf);
        clang::printTemplateArgumentList(ArgOS, t->getTemplateArgs().asArray(),
                                         clang::PrintingPolicy(d->getLangOpts()));
        cpp += ArgOS.str().str();
      }
      ids.emplace(std::make_pair(d, *this));
    }
  }

  std::string c;    // a name-mangled identifier for C
  std::string cpp;  // the fully qualified C++ name
};

std::unordered_map<const clang::NamedDecl *, Identifier> Identifier::ids;

void CLibOutputDriver::write_header() {
  std::string macroname = sanitize_identifier(_inheader.stem());
  _hf << "/*\n";
  _hf << " * This header file was generated automatically by c2ffi.\n";
  _hf << " */\n";
  _hf << "#ifndef " << macroname << "_CIFGEN_H\n";
  _hf << "#define " << macroname << "_CIFGEN_H\n";
  _hf << "#ifdef __cplusplus\n";
  _hf << "#include \"" << (std::string)_inheader << "\"\n";
  _hf << "extern \"C\" {\n";
  _hf << "#endif\n\n";

  _sf << "/*\n";
  _sf << " * This source file was generated automatically by c2ffi.\n";
  _sf << " */\n";
  _sf << "#include \"" << (std::string)_outheader << "\"\n\n";
}

void CLibOutputDriver::write_footer() {
  std::string macroname = sanitize_identifier(_inheader.stem());
  _hf << "#ifdef __cplusplus\n";
  _hf << "} // extern \"C\"\n";
  _hf << "#endif // __cplusplus\n";
  _hf << "#endif // " << macroname << "_CIFGEN_H\n";
}

void CLibOutputDriver::write(const SimpleType &t) {
  const std::string &n(t.name());
  if (n.size() > 1 && n[0] == ':') {
    os() << n.substr(1);
  } else {
    os() << t.name();
  }
}

void CLibOutputDriver::write(const TypedefType &t) { os() << Identifier(t.orig()->getDecl()).c; }

void CLibOutputDriver::write(const BasicType &t) {
  os() << ((clang::BuiltinType *)t.orig())->getNameAsCString(clang::PrintingPolicy(*_lo));
}

void CLibOutputDriver::write(const BitfieldType &t) {}

void CLibOutputDriver::write(const PointerType &t) {
  write(t.pointee());
  os() << "*";
}

void CLibOutputDriver::write(const ArrayType &t) {}
void CLibOutputDriver::write(const RecordType &t) {
  Identifier i(t.orig()->getDecl());
  os() << i.c;
}
void CLibOutputDriver::write(const EnumType &t) {}
void CLibOutputDriver::write(const ComplexType &t) {}

void CLibOutputDriver::write(const UnhandledDecl &d) {}
void CLibOutputDriver::write(const VarDecl &d) {}

bool shouldPointerize(const Type *t) {
  if (const TypedefType *tt = dynamic_cast<const TypedefType *>(t)) {
    return shouldPointerize(tt->underType());
  } else if (dynamic_cast<const ReferenceType *>(t)) {
    return true;
  } else if (dynamic_cast<const RecordType *>(t)) {
    return true;
  }
  return false;
}

void CLibOutputDriver::write_params(const FieldsMixin &f, const Type &_return, bool add_types,
                                    const std::string &_this) {
  os() << "(";

  bool first = true;

  if (!_this.empty()) {
    if (add_types) {
      os() << _this << "* ";
    }
    os() << mangleConf._this;
    first = false;
  }

  for (const auto &f : f.fields()) {
    if (!first) {
      os() << ", ";
    }
    if (add_types) {
      write(*f.second);
      os() << " ";
    }
    if (shouldPointerize(f.second)) {
      os() << "*";
    }
    os() << f.first;
    first = false;
  }

  os() << ")";
}

void CLibOutputDriver::write_fn(const FunctionDecl &d, const std::string &type,
                                const Identifier *parent, bool ctor) {
  Identifier i(d.orig());
  std::string _this = parent ? parent->c : "";

  _hf << "// location: " << d.location() << "\n";
  _hf << "// " << type << " " << i.cpp << "\n";
  _sf << "// location: " << d.location() << "\n";
  _sf << "// " << type << " " << i.cpp << "\n";

  if (ctor) {
    _hf << _this << "* " << _this << mangleConf.ctor;
    _sf << _this << "* " << _this << mangleConf.ctor;
  } else {
    set_os(&_hf);
    write(d.return_type());
    set_os(&_sf);
    write(d.return_type());
    _hf << " " << i.c;
    _sf << " " << i.c;
  }

  set_os(&_hf);
  write_params(d, d.return_type(), true, ctor ? "" : _this);
  set_os(&_sf);
  write_params(d, d.return_type(), true, ctor ? "" : _this);

  _hf << ";\n\n";
  _sf << " {\n";
  _sf << "  return ";
  if (ctor) {
    _sf << "new " << parent->cpp;
  } else {
    if (!_this.empty()) {
      _sf << mangleConf._this << "->";
    }
    _sf << i.cpp;
  }
  set_os(&_sf);
  write_params(d, d.return_type(), false);
  _sf << ";\n}\n\n";
}

void CLibOutputDriver::write(const FunctionDecl &d) { write_fn(d, "Function", nullptr, false); }

void CLibOutputDriver::write(const TypedefDecl &d) {
  _lo = &d.orig()->getLangOpts();
  Identifier i(d.orig());
  _hf << "// location: " << d.location() << "\n";
  _hf << "#ifdef __cplusplus\n";
  if (i.cpp == i.c) _hf << "// ";
  _hf << "typedef " << i.cpp << " " << i.c << ";\n";
  _hf << "#else\n";
  // need to add a forward declaration if the target type is a struct - it may not have been
  // declared already.
  set_os(&_hf);
  const Type *t = &d.type();
  if (dynamic_cast<const DeclType *>(t) || dynamic_cast<const RecordType *>(t)) {
    _hf << "typedef struct ";
    if (dynamic_cast<const RecordType *>(t)) {
      write(*t);
    } else {
      _hf << i.c;
    }
    _hf << mangleConf._struct << " " << i.c << ";\n";
  } else {
    _hf << "typedef ";
    write(d.type());
    _hf << " " << i.c << ";\n";
  }
  _hf << "#endif // __cplusplus\n\n";
}

void CLibOutputDriver::write(const RecordDecl &d) {
  Identifier i(d.orig());
  _hf << "// location: " << d.location() << "\n";
  _hf << "#ifdef __cplusplus\n";
  _hf << "typedef " << i.cpp << " " << i.c << ";\n";
  _hf << "#else\n";
  _hf << "struct " << i.c << ";\n";
  _hf << "#endif // __cplusplus\n";
}

void CLibOutputDriver::write(const EnumDecl &d) {}

void CLibOutputDriver::write(const CXXRecordDecl &d) {
  Identifier i(d.orig());
  _hf << "// location: " << d.location() << "\n";
  _hf << "#ifdef __cplusplus\n";
  _hf << "typedef " << i.cpp << " " << i.c << ";\n";
  _hf << "#else\n";
  _hf << "typedef struct " << i.c << mangleConf._struct << " " << i.c << ";\n";
  _hf << "#endif // __cplusplus\n\n";
  _sf << "// location: " << d.location() << "\n";
  _sf << "// Stubs for C++ struct: " << i.cpp << "\n\n";

  const auto &funcs = d.functions();
  for (FunctionVector::const_iterator i = funcs.begin(); i != funcs.end(); i++) {
    write((const Writable &)*(*i));
  }

  // the destructor should be written always, even if it's not in the method list
  _hf << "// Destructor of " << i.cpp << "\n";
  _sf << "// Destructor of " << i.cpp << "\n";
  _hf << "void " << i.c << mangleConf.dtor << "(" << i.c << "* " << mangleConf._this << ");\n\n";
  _sf << "void " << i.c << mangleConf.dtor << "(" << i.c << "* " << mangleConf._this
      << ") {\n  delete " << mangleConf._this << ";\n}\n\n";
}

void CLibOutputDriver::write(const CXXFunctionDecl &d) {
  const clang::NamedDecl *pd = llvm::dyn_cast<clang::NamedDecl>(d.orig()->getDeclContext());
  Identifier p(pd);
  // drop the destructor, which is printed in the recorddecl writer
  if (!d.name().empty() && d.name()[0] == '~') {
    return;
  }
  if (!d.name().empty() && d.name() == pd->getDeclName().getAsString()) {
    write_fn(d, "Constructor", &p, true);
  } else {
    write_fn(d, "Method", &p, false);
  }
}

void CLibOutputDriver::close() {
  set_os(&_hf);
  OutputDriver::close();
  std::ofstream *sf = dynamic_cast<std::ofstream *>(&_sf);
  if (sf != nullptr) {
    sf->close();
  }
}
