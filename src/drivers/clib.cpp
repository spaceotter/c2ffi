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

struct Identifier {
  Identifier(const std::string &name);
  Identifier(const Identifier &parent, const std::string &name);
  Identifier(const std::string &c, const std::string &cpp) : c(c), cpp(cpp) {}
  Identifier(const Identifier &parent, const std::string &c, const std::string &cpp);
  Identifier(const clang::NamedDecl *d) : c(getCName(d)), cpp(d->getQualifiedNameAsString()) {
    if (const auto *t = dynamic_cast<const clang::ClassTemplateSpecializationDecl *>(d)) {
      clang::SmallString<128> Buf;
      llvm::raw_svector_ostream ArgOS(Buf);
      clang::printTemplateArgumentList(ArgOS, t->getTemplateArgs().asArray(),
                                       clang::PrintingPolicy(d->getLangOpts()));
      cpp += ArgOS.str().str();
    }
  }

  std::string c;    // a name-mangled identifier for C
  std::string cpp;  // the fully qualified C++ name
};

Identifier::Identifier(const std::string &name) : c(mangleConf.root_prefix + name), cpp(name) {}

Identifier::Identifier(const Identifier &parent, const std::string &name)
    : Identifier(parent, name, name) {}

Identifier::Identifier(const Identifier &parent, const std::string &c, const std::string &cpp)
    : c(parent.c + mangleConf.c_separator + c), cpp(parent.cpp + mangleConf.cpp_separator + cpp) {}

void CLibOutputDriver::write_header() {
  std::string macroname = sanitize_identifier(_inheader.stem());
  os() << "/*\n";
  os() << " * This header file was generated automatically by c2ffi.\n";
  os() << " */\n";
  os() << "#ifndef " << macroname << "_CIFGEN_H\n";
  os() << "#define " << macroname << "_CIFGEN_H\n";
  os() << "#ifdef __cplusplus\n";
  os() << "#include \"" << (std::string)_inheader << "\"\n";
  os() << "extern \"C\" {\n";
  os() << "#endif\n\n";

  _sf << "/*\n";
  _sf << " * This source file was generated automatically by c2ffi.\n";
  _sf << " */\n";
  _sf << "#include \"" << (std::string)_outheader << "\"\n\n";
}

void CLibOutputDriver::write_footer() {
  std::string macroname = sanitize_identifier(_inheader.stem());
  os() << "#ifdef __cplusplus\n";
  os() << "} // extern \"C\"\n";
  os() << "#endif // __cplusplus\n";
  os() << "#endif // " << macroname << "_CIFGEN_H\n";
}

void CLibOutputDriver::write(const SimpleType &t) {}

void CLibOutputDriver::write(const TypedefType &) {}

void CLibOutputDriver::write(const BasicType &t) {
  os() << ((clang::BuiltinType *)t.orig())->getNameAsCString(clang::PrintingPolicy(*_lo));
}

void CLibOutputDriver::write(const BitfieldType &t) {}
void CLibOutputDriver::write(const PointerType &t) {}
void CLibOutputDriver::write(const ArrayType &t) {}
void CLibOutputDriver::write(const RecordType &t) {
  Identifier i(t.orig()->getDecl());
  os() << "struct " << i.c;
}
void CLibOutputDriver::write(const EnumType &t) {}
void CLibOutputDriver::write(const ComplexType &t) {}

void CLibOutputDriver::write(const UnhandledDecl &d) {}
void CLibOutputDriver::write(const VarDecl &d) {}
void CLibOutputDriver::write(const FunctionDecl &d) {}

void CLibOutputDriver::write(const TypedefDecl &d) {
  _lo = &d.orig()->getLangOpts();
  Identifier i(d.orig());
  os() << "// location: " << d.location() << "\n";
  os() << "#ifdef __cplusplus\n";
  if (i.cpp == i.c) os() << "// ";
  os() << "typedef " << i.cpp << " " << i.c << ";\n";
  os() << "#else\n";
  // need to add a forward declaration if the target type is a struct - it may not have been
  // declared already.
  const Type *t = &d.type();
  if (const auto *rt = dynamic_cast<const RecordType *>(t)) {
    write(d.type());
    os() << ";\n";
  }
  if (const auto *dt = dynamic_cast<const DeclType *>(t)) {
    os() << "struct " << i.c << ";\n";
  } else {
    os() << "typedef ";
    write(d.type());
    os() << " " << i.c << ";\n";
  }
  os() << "#endif // __cplusplus\n\n";
}

void CLibOutputDriver::write(const RecordDecl &d) {
  os() << "struct " << d.name() << ";\n";
  _sf << "struct " << d.name() << ";\n";
}

void CLibOutputDriver::write(const EnumDecl &d) {}

void CLibOutputDriver::write(const CXXRecordDecl &d) {
  Identifier i(d.orig());
  os() << "// location: " << d.location() << "\n";
  os() << "#ifdef __cplusplus\n";
  os() << "typedef " << i.cpp << " " << i.c << ";\n";
  os() << "#else\n";
  os() << "struct " << i.c << ";\n";
  os() << "#endif // __cplusplus\n";
  _sf << "// location: " << d.location() << "\n";
  _sf << "// Stubs for C++ struct: " << i.cpp << "\n";
  os() << "\n";
  _sf << "\n";
}

void CLibOutputDriver::write(const CXXFunctionDecl &d) {}

void CLibOutputDriver::close() {
  OutputDriver::close();
  std::ofstream *sf = dynamic_cast<std::ofstream *>(&_sf);
  if (sf != nullptr) {
    sf->close();
  }
}
