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

#include <iostream>
#include <sstream>

#include <llvm/Support/raw_os_ostream.h>
#include <llvm/Support/Host.h>
#include <llvm/ADT/IntrusiveRefCntPtr.h>
#include <llvm/Support/CrashRecoveryContext.h>

#include <clang/Basic/Version.h>
#include <clang/Basic/DiagnosticOptions.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/Utils.h>
#include <clang/Basic/TargetOptions.h>
#include <clang/Basic/TargetInfo.h>
#include <clang/Basic/FileManager.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Lex/HeaderSearch.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Basic/Diagnostic.h>
#include <clang/AST/ASTContext.h>
#include <clang/AST/ASTConsumer.h>
#include <clang/Parse/Parser.h>
#include <clang/Parse/ParseAST.h>

#include "c2ffi.hpp"
#include "c2ffi/init.hpp"
#include "c2ffi/opt.hpp"
#include "c2ffi/ast.hpp"
#include "c2ffi/macros.hpp"
#include "c2ffi/iparseast.hpp"

using namespace c2ffi;

/*
 * Deploy a hack to add code from a buffer to the translation unit.
 */
static void amendFromStream(clang::CompilerInstance &ci,
                            std::stringstream &ss,
                            const std::string &name,
                            const c2ffi::config &sys,
                            clang::Sema &S) {
    if (sys.verbose) {
        ss.clear();
        ss.seekg(0);
        std::string line;
        while (std::getline(ss, line)) {
            std::cerr << name << ": " << line << std::endl;
        }
    }

    std::string buf = ss.str();

    clang::FileID mfid = ci.getSourceManager().createFileID(
        std::unique_ptr<llvm::MemoryBuffer>(llvm::MemoryBuffer::getMemBuffer(buf, name)));

    IncrementalParseAST(S, mfid, false, true);
}

int main(int argc, char *argv[]) {
    clang::CompilerInstance ci;
    c2ffi::config sys;

    process_args(sys, argc, argv);
    // this finishes parsing the arguments using clang
    init_ci(sys, ci);

    add_includes(ci, sys.includes, false, true);
    add_includes(ci, sys.sys_includes, true, true);

    C2FFIASTConsumer *astc = NULL;

    const clang::FileEntry *file = ci.getFileManager().getFile(sys.filename).get();
    clang::FileID fid = ci.getSourceManager().createFileID(file,
                                                           clang::SourceLocation(),
                                                           clang::SrcMgr::C_User);
    ci.getSourceManager().setMainFileID(fid);
    ci.getDiagnosticClient().BeginSourceFile(ci.getLangOpts(),
                                             &ci.getPreprocessor());

    bool main_error = false;
    bool extra_error = false;

    if(sys.preprocess_only) {
        llvm::raw_ostream *os = new llvm::raw_os_ostream(*sys.output);
        clang::DoPrintPreprocessedInput(ci.getPreprocessor(), os,
                                        ci.getPreprocessorOutputOpts());
        delete os;
        main_error = ci.getDiagnostics().hasErrorOccurred();
    } else {
        astc = new C2FFIASTConsumer(ci, sys);
        ci.setASTConsumer(std::unique_ptr<clang::ASTConsumer>(astc));
        ci.createASTContext();

        sys.od->write_header();

        if(sys.to_namespace != "")
            sys.od->write_namespace(sys.to_namespace);

        std::unique_ptr<clang::Sema> S(
            new clang::Sema(ci.getPreprocessor(), ci.getASTContext(), *astc,
                            clang::TU_Complete, nullptr));

        // Recover resources if we crash before exiting this method.
        llvm::CrashRecoveryContextCleanupRegistrar<Sema> CleanupSema(S.get());
        clang::ParseAST(*S.get(), false, true);

        main_error = ci.getDiagnostics().hasErrorOccurred();
        ci.getDiagnostics().Reset();

        if (sys.macro_output || sys.macro_inject) {
            std::stringstream macros_ss;
            process_macros(ci, macros_ss, sys);
            if (sys.macro_output) {
                *sys.macro_output << macros_ss.str();
                sys.macro_output->close();
            }

            if (sys.macro_inject) {
                amendFromStream(ci, macros_ss, "<macros>", sys, *S.get());
            }
        }

        if (sys.template_output) {
            std::stringstream templates_ss;
            astc->PostProcess(templates_ss);
            amendFromStream(ci, templates_ss, "<templates>", sys, *S.get());
        }

        sys.od->write_footer();
        extra_error = ci.getDiagnostics().hasErrorOccurred();
    }
    ci.getDiagnosticClient().EndSourceFile();
    sys.output->flush();

    if(extra_error)
        std::cerr << "Warning: Some errors occurred in internally generated code." << std::endl;

    if(sys.fail_on_error && main_error)
        return 1;
    return 0;
}
