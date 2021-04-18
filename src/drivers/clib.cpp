#include <fstream>
#include <unordered_map>

#include "c2ffi.hpp"

using namespace c2ffi;

ManglerConfig c2ffi::mangleConf;

struct Identifier {
  Identifier(const std::string &name);
  Identifier(const Identifier &parent, const std::string &name);
  Identifier(const std::string &c, const std::string &cpp) : c(c), cpp(cpp) {}
  Identifier(const Identifier &parent, const std::string &c, const std::string &cpp);

  std::string c;    // a name-mangled identifier for C
  std::string cpp;  // the fully qualified C++ name
};

Identifier::Identifier(const std::string &name) : c(mangleConf.root_prefix + name), cpp(name) {}

Identifier::Identifier(const Identifier &parent, const std::string &name)
    : Identifier(parent, name, name) {}

Identifier::Identifier(const Identifier &parent, const std::string &c, const std::string &cpp)
    : c(parent.c + mangleConf.c_separator + c), cpp(parent.cpp + mangleConf.cpp_separator + cpp) {}

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

void CLibOutputDriver::write_header() {
  std::string macroname = sanitize_identifier(_inheader.stem());
  os() << "/*\n";
  os() << " * This file was generated automatically by c2ffi.\n";
  os() << " */\n";
  os() << "#ifndef " << macroname << "_CIFGEN_H\n";
  os() << "#define " << macroname << "_CIFGEN_H\n";
  os() << "#ifdef __cplusplus\n";
  os() << "#include \"" << (std::string)_inheader << "\"\n";
  os() << "extern \"C\" {\n";
  os() << "#endif\n\n";

  _sf << "/*\n";
  _sf << " * This file was generated automatically by c2ffi.\n";
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

void CLibOutputDriver::write(const BasicType &t) {}
void CLibOutputDriver::write(const BitfieldType &t) {}
void CLibOutputDriver::write(const PointerType &t) {}
void CLibOutputDriver::write(const ArrayType &t) {}
void CLibOutputDriver::write(const RecordType &t) {}
void CLibOutputDriver::write(const EnumType &t) {}
void CLibOutputDriver::write(const ComplexType &t) {}

void CLibOutputDriver::write(const UnhandledDecl &d) {}
void CLibOutputDriver::write(const VarDecl &d) {}
void CLibOutputDriver::write(const FunctionDecl &d) {}
void CLibOutputDriver::write(const TypedefDecl &d) {
  os() << "typedef " << d.name() << ";\n";
  _sf << "typedef " << d.orig()->getQualifiedNameAsString() << ";\n";
}

void CLibOutputDriver::write(const RecordDecl &d) {
  os() << "struct " << d.name() << ";\n";
  _sf << "struct " << d.name() << ";\n";
}

void CLibOutputDriver::write(const EnumDecl &d) {}

void CLibOutputDriver::write(const CXXRecordDecl &d) {
  os() << "class " << d.name() << ";\n";
  _sf << "class " << d.orig()->getQualifiedNameAsString() << ";\n";
}

void CLibOutputDriver::close() {
  OutputDriver::close();
  std::ofstream *sf = dynamic_cast<std::ofstream *>(&_sf);
  if (sf != nullptr) {
    sf->close();
  }
}
