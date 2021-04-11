#include <stdarg.h>

#include <iomanip>
#include <sstream>

#include "c2ffi.hpp"

namespace c2ffi {
class CLibOutputDriver : public OutputDriver {
  std::ostream &_sf;

 public:
  CLibOutputDriver(std::ostream *hf, std::ostream *sf) : OutputDriver(hf), _sf(*sf) {}
  ~CLibOutputDriver() override {}

  virtual void write_header() {}
  virtual void write_namespace(const std::string &ns) {}
  virtual void write_between() {}
  virtual void write_footer() {}

  virtual void write_comment(const char *text) {}

  void write(const SimpleType &) override;
  void write(const TypedefType &) override;
  void write(const BasicType &) override;
  void write(const BitfieldType &) override;
  void write(const PointerType &) override;
  void write(const ArrayType &) override;
  void write(const RecordType &) override;
  void write(const EnumType &) override;
  void write(const ReferenceType &) override {}
  void write(const TemplateType &) override {}
  void write(const ComplexType &) override;

  void write(const UnhandledDecl &d) override;
  void write(const VarDecl &d) override;
  void write(const FunctionDecl &d) override;
  void write(const TypedefDecl &d) override;
  void write(const RecordDecl &d) override;
  void write(const EnumDecl &d) override;

  void write(const CXXRecordDecl &d) override;
  virtual void write(const CXXFunctionDecl &d) {}
  virtual void write(const CXXNamespaceDecl &d) {}

  virtual void write(const ObjCInterfaceDecl &d) {}
  virtual void write(const ObjCCategoryDecl &d) {}
  virtual void write(const ObjCProtocolDecl &d) {}

  virtual void write(const Writable &w) { w.write(*this); }

  void close() override;
};

}  // namespace c2ffi
