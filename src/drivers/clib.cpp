#include <fstream>

#include "c2ffi.hpp"

using namespace c2ffi;

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
  os() << "template " << d.name() << ";\n";
  _sf << "template " << d.name() << ";\n";
}

void CLibOutputDriver::write(const RecordDecl &d) {
  os() << "class " << d.name() << ";\n";
  _sf << "class " << d.name() << ";\n";
}

void CLibOutputDriver::write(const EnumDecl &d) {}

void CLibOutputDriver::close() {
  OutputDriver::close();
  std::ofstream *sf = dynamic_cast<std::ofstream *>(&_sf);
  if (sf != nullptr) {
    sf->close();
  }
}
