#include <stdarg.h>

#include <filesystem>
#include <iomanip>
#include <sstream>

#include "c2ffi.hpp"

namespace c2ffi {
/*
 * The settings for generating C identifiers from C++
 */
struct ManglerConfig {
  // these determine how flattened C names are assembled
  std::string root_prefix = "upp_";
  std::string c_separator = "_";
  std::string _this = "_upp_this";
  std::string _return = "_upp_return";
  std::string _struct = "_s_";
  std::string dtor = "_dtor";
  std::string ctor = "_ctor";
};

extern ManglerConfig mangleConf;
struct Identifier;

class CLibOutputDriver : public OutputDriver {
  std::ostream &_hf;
  std::ostream &_sf;
  const std::filesystem::path _inheader;
  const std::filesystem::path _outheader;
  const clang::LangOptions *_lo = nullptr;

 public:
  CLibOutputDriver(const std::filesystem::path &inheader, const std::filesystem::path &outheader,
                   std::ostream *hf, std::ostream *sf)
      : OutputDriver(hf), _hf(*hf), _sf(*sf), _inheader(inheader), _outheader(outheader) {}
  ~CLibOutputDriver() override {}

  void write_header() override;
  void write_namespace(const std::string &ns) override {}
  void write_between() override {}
  void write_footer() override;

  void write_comment(const char *text) override {}

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
  void write(const CXXFunctionDecl &d) override;
  void write(const CXXNamespaceDecl &d) override {}

  void write(const ObjCInterfaceDecl &d) override {}
  void write(const ObjCCategoryDecl &d) override {}
  void write(const ObjCProtocolDecl &d) override {}

  void write(const Writable &w) override { w.write(*this); }

  void close() override;

 private:
  // helper functions
  void write_params(const FieldsMixin &fields, const Type &_return, bool add_types = true,
                    const std::string &_this = "");
  void write_fn(const FunctionDecl &d, const std::string &type, const Identifier *parent,
                bool ctor);
};

}  // namespace c2ffi
