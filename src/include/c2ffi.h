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

#ifndef C2FFI_H
#define C2FFI_H

#include <iostream>
#include <string>

#include "c2ffi/predecl.h"

#define DEFWRITER(x) virtual void write(OutputDriver &od) const { od.write((const x&)*this); }

namespace c2ffi {
    class OutputDriver;

    class Writable {
    public:
        virtual ~Writable() { }
        virtual void write(OutputDriver &od) const = 0;
    };

    class OutputDriver {
        std::ostream *_os;
    public:
        OutputDriver(std::ostream *os)
            : _os(os) { }
        virtual ~OutputDriver() { }

        /**
           write_header()    - Called before other output
           write_namespace() - Called after header
           write_between()   - Called _between_ declarations, but _not_
                               after write_namespace().
           write_footer()    - Called after all other output.
         **/
        virtual void write_header() { }
        virtual void write_namespace(const std::string &ns) { }
        virtual void write_between() { }
        virtual void write_footer() { }

        virtual void write_comment(const char *text) { }

        virtual void write(const SimpleType&) = 0;
        virtual void write(const TypedefType &) = 0;
        virtual void write(const BasicType &) = 0;
        virtual void write(const BitfieldType&) = 0;
        virtual void write(const PointerType&) = 0;
        virtual void write(const ArrayType&) = 0;
        virtual void write(const RecordType&) = 0;
        virtual void write(const EnumType&) = 0;
        virtual void write(const ReferenceType&) { }
        virtual void write(const TemplateType&) { }
        virtual void write(const ComplexType&) = 0;

        virtual void write(const UnhandledDecl &d) = 0;
        virtual void write(const VarDecl &d) = 0;
        virtual void write(const FunctionDecl &d) = 0;
        virtual void write(const TypedefDecl &d) = 0;
        virtual void write(const RecordDecl &d) = 0;
        virtual void write(const EnumDecl &d) = 0;

        virtual void write(const CXXRecordDecl &d) { }
        virtual void write(const CXXFunctionDecl &d) { }
        virtual void write(const CXXNamespaceDecl &d) { }

        virtual void write(const ObjCInterfaceDecl &d) { }
        virtual void write(const ObjCCategoryDecl &d) { }
        virtual void write(const ObjCProtocolDecl &d) { }

        virtual void write(const Writable& w) { w.write(*this); }

        void set_os(std::ostream *os) { _os = os; }
        std::ostream& os() { return *_os; }

        void comment(char *fmt, ...);
    };

    typedef OutputDriver* (*MakeOutputDriver)(std::ostream *os);

    struct OutputDriverField {
        const char* name;
        MakeOutputDriver fn;
    };

    extern OutputDriverField OutputDrivers[];
}

#include "c2ffi/template.h"
#include "c2ffi/type.h"
#include "c2ffi/decl.h"

#endif /* C2FFI_H */
