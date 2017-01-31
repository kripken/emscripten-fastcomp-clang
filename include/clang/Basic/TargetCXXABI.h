//===--- TargetCXXABI.h - C++ ABI Target Configuration ----------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief Defines the TargetCXXABI class, which abstracts details of the
/// C++ ABI that we're targeting.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_BASIC_TARGETCXXABI_H
#define LLVM_CLANG_BASIC_TARGETCXXABI_H

#include "llvm/Support/ErrorHandling.h"

namespace clang {

/// \brief The basic abstraction for the target C++ ABI.
class TargetCXXABI {
public:
  /// \brief The basic C++ ABI kind.
  enum Kind {
    /// The generic Itanium ABI is the standard ABI of most open-source
    /// and Unix-like platforms.  It is the primary ABI targeted by
    /// many compilers, including Clang and GCC.
    ///
    /// It is documented here:
    ///   http://www.codesourcery.com/public/cxx-abi/
    GenericItanium,

    /// The generic ARM ABI is a modified version of the Itanium ABI
    /// proposed by ARM for use on ARM-based platforms.
    ///
    /// These changes include:
    ///   - the representation of member function pointers is adjusted
    ///     to not conflict with the 'thumb' bit of ARM function pointers;
    ///   - constructors and destructors return 'this';
    ///   - guard variables are smaller;
    ///   - inline functions are never key functions;
    ///   - array cookies have a slightly different layout;
    ///   - additional convenience functions are specified;
    ///   - and more!
    ///
    /// It is documented here:
    ///    http://infocenter.arm.com
    ///                    /help/topic/com.arm.doc.ihi0041c/IHI0041C_cppabi.pdf
    GenericARM,

    /// The iOS ABI is a partial implementation of the ARM ABI.
    /// Several of the features of the ARM ABI were not fully implemented
    /// in the compilers that iOS was launched with.
    ///
    /// Essentially, the iOS ABI includes the ARM changes to:
    ///   - member function pointers,
    ///   - guard variables,
    ///   - array cookies, and
    ///   - constructor/destructor signatures.
    iOS,

    /// The iOS 64-bit ABI is follows ARM's published 64-bit ABI more
    /// closely, but we don't guarantee to follow it perfectly.
    ///
    /// It is documented here:
    ///    http://infocenter.arm.com
    ///                  /help/topic/com.arm.doc.ihi0059a/IHI0059A_cppabi64.pdf
    iOS64,

    /// WatchOS is a modernisation of the iOS ABI, which roughly means it's
    /// the iOS64 ABI ported to 32-bits. The primary difference from iOS64 is
    /// that RTTI objects must still be unique at the moment.
    WatchOS,

    /// The generic AArch64 ABI is also a modified version of the Itanium ABI,
    /// but it has fewer divergences than the 32-bit ARM ABI.
    ///
    /// The relevant changes from the generic ABI in this case are:
    ///   - representation of member function pointers adjusted as in ARM.
    ///   - guard variables  are smaller.
    GenericAArch64,

    // @LOCALMOD-START Emscripten
    /// Emscripten uses the Itanium C++, with the exception that it uses
    /// ARM-style pointers to member functions.
    Emscripten,
    // @LOCALMOD-END Emscripten
    /// The generic Mips ABI is a modified version of the Itanium ABI.
    ///
    /// At the moment, only change from the generic ABI in this case is:
    ///   - representation of member function pointers adjusted as in ARM.
    GenericMIPS,

    /// The WebAssembly ABI is a modified version of the Itanium ABI.
    ///
    /// The changes from the Itanium ABI are:
    ///   - representation of member function pointers is adjusted, as in ARM;
    ///   - member functions are not specially aligned;
    ///   - constructors and destructors return 'this', as in ARM;
    ///   - guard variables are 32-bit on wasm32, as in ARM;
    ///   - unused bits of guard variables are reserved, as in ARM;
    ///   - inline functions are never key functions, as in ARM;
    ///   - C++11 POD rules are used for tail padding, as in iOS64.
    ///
    /// TODO: At present the WebAssembly ABI is not considered stable, so none
    /// of these details is necessarily final yet.
    WebAssembly,

    /// The Microsoft ABI is the ABI used by Microsoft Visual Studio (and
    /// compatible compilers).
    ///
    /// FIXME: should this be split into Win32 and Win64 variants?
    ///
    /// Only scattered and incomplete official documentation exists.
    Microsoft
  };

private:
  // Right now, this class is passed around as a cheap value type.
  // If you add more members, especially non-POD members, please
  // audit the users to pass it by reference instead.
  Kind TheKind;

public:
  /// A bogus initialization of the platform ABI.
  TargetCXXABI() : TheKind(GenericItanium) {}

  TargetCXXABI(Kind kind) : TheKind(kind) {}

  void set(Kind kind) {
    TheKind = kind;
  }

  Kind getKind() const { return TheKind; }

  /// \brief Does this ABI generally fall into the Itanium family of ABIs?
  bool isItaniumFamily() const {
    switch (getKind()) {
    case GenericAArch64:
    case GenericItanium:
    case GenericARM:
    case Emscripten: // @LOCALMOD Emscripten
    case iOS:
    case iOS64:
    case WatchOS:
    case GenericMIPS:
    case WebAssembly:
      return true;

    case Microsoft:
      return false;
    }
    llvm_unreachable("bad ABI kind");
  }

  /// \brief Is this ABI an MSVC-compatible ABI?
  bool isMicrosoft() const {
    switch (getKind()) {
    case GenericAArch64:
    case GenericItanium:
    case GenericARM:
    case Emscripten: // @LOCALMOD Emscripten
    case iOS:
    case iOS64:
    case WatchOS:
    case GenericMIPS:
    case WebAssembly:
      return false;

    case Microsoft:
      return true;
    }
    llvm_unreachable("bad ABI kind");
  }

  /// \brief Are member functions differently aligned?
  ///
  /// Many Itanium-style C++ ABIs require member functions to be aligned, so
  /// that a pointer to such a function is guaranteed to have a zero in the
  /// least significant bit, so that pointers to member functions can use that
  /// bit to distinguish between virtual and non-virtual functions. However,
  /// some Itanium-style C++ ABIs differentiate between virtual and non-virtual
  /// functions via other means, and consequently don't require that member
  /// functions be aligned.
  bool areMemberFunctionsAligned() const {
    switch (getKind()) {
    case Emscripten:
      // Emscripten uses table indices for function pointers and therefore
      // doesn't require alignment.
      return false;
    case WebAssembly:
      // WebAssembly doesn't require any special alignment for member functions.
      return false;
    case GenericARM:
    case GenericAArch64:
    case GenericMIPS:
      // TODO: ARM-style pointers to member functions put the discriminator in
      //       the this adjustment, so they don't require functions to have any
      //       special alignment and could therefore also return false.
    case GenericItanium:
    case iOS:
    case iOS64:
    case WatchOS:
    case Microsoft:
      return true;
    }
    llvm_unreachable("bad ABI kind");
  }

  /// \brief Is the default C++ member function calling convention
  /// the same as the default calling convention?
  bool isMemberFunctionCCDefault() const {
    // Right now, this is always false for Microsoft.
    return !isMicrosoft();
  }

  /// Are arguments to a call destroyed left to right in the callee?
  /// This is a fundamental language change, since it implies that objects
  /// passed by value do *not* live to the end of the full expression.
  /// Temporaries passed to a function taking a const reference live to the end
  /// of the full expression as usual.  Both the caller and the callee must
  /// have access to the destructor, while only the caller needs the
  /// destructor if this is false.
  bool areArgsDestroyedLeftToRightInCallee() const {
    return isMicrosoft();
  }

  /// \brief Does this ABI have different entrypoints for complete-object
  /// and base-subobject constructors?
  bool hasConstructorVariants() const {
    return isItaniumFamily();
  }

  /// \brief Does this ABI allow virtual bases to be primary base classes?
  bool hasPrimaryVBases() const {
    return isItaniumFamily();
  }

  /// \brief Does this ABI use key functions?  If so, class data such as the
  /// vtable is emitted with strong linkage by the TU containing the key
  /// function.
  bool hasKeyFunctions() const {
    return isItaniumFamily();
  }

  /// \brief Can an out-of-line inline function serve as a key function?
  ///
  /// This flag is only useful in ABIs where type data (for example,
  /// vtables and type_info objects) are emitted only after processing
  /// the definition of a special "key" virtual function.  (This is safe
  /// because the ODR requires that every virtual function be defined
  /// somewhere in a program.)  This usually permits such data to be
  /// emitted in only a single object file, as opposed to redundantly
  /// in every object file that requires it.
  ///
  /// One simple and common definition of "key function" is the first
  /// virtual function in the class definition which is not defined there.
  /// This rule works very well when that function has a non-inline
  /// definition in some non-header file.  Unfortunately, when that
  /// function is defined inline, this rule requires the type data
  /// to be emitted weakly, as if there were no key function.
  ///
  /// The ARM ABI observes that the ODR provides an additional guarantee:
  /// a virtual function is always ODR-used, so if it is defined inline,
  /// that definition must appear in every translation unit that defines
  /// the class.  Therefore, there is no reason to allow such functions
  /// to serve as key functions.
  ///
  /// Because this changes the rules for emitting type data,
  /// it can cause type data to be emitted with both weak and strong
  /// linkage, which is not allowed on all platforms.  Therefore,
  /// exploiting this observation requires an ABI break and cannot be
  /// done on a generic Itanium platform.
  bool canKeyFunctionBeInline() const {
    switch (getKind()) {
    case GenericARM:
    case iOS64:
    case WebAssembly:
    case WatchOS:
      return false;

    case GenericAArch64:
    case GenericItanium:
    case Emscripten: // @LOCALMOD Emscripten
    case iOS:   // old iOS compilers did not follow this rule
    case Microsoft:
    case GenericMIPS:
      return true;
    }
    llvm_unreachable("bad ABI kind");
  }

  /// When is record layout allowed to allocate objects in the tail
  /// padding of a base class?
  ///
  /// This decision cannot be changed without breaking platform ABI
  /// compatibility, and yet it is tied to language guarantees which
  /// the committee has so far seen fit to strengthen no less than
  /// three separate times:
  ///   - originally, there were no restrictions at all;
  ///   - C++98 declared that objects could not be allocated in the
  ///     tail padding of a POD type;
  ///   - C++03 extended the definition of POD to include classes
  ///     containing member pointers; and
  ///   - C++11 greatly broadened the definition of POD to include
  ///     all trivial standard-layout classes.
  /// Each of these changes technically took several existing
  /// platforms and made them permanently non-conformant.
  enum TailPaddingUseRules {
    /// The tail-padding of a base class is always theoretically
    /// available, even if it's POD.  This is not strictly conforming
    /// in any language mode.
    AlwaysUseTailPadding,

    /// Only allocate objects in the tail padding of a base class if
    /// the base class is not POD according to the rules of C++ TR1.
    /// This is non-strictly conforming in C++11 mode.
    UseTailPaddingUnlessPOD03,

    /// Only allocate objects in the tail padding of a base class if
    /// the base class is not POD according to the rules of C++11.
    UseTailPaddingUnlessPOD11
  };
  TailPaddingUseRules getTailPaddingUseRules() const {
    switch (getKind()) {
    // To preserve binary compatibility, the generic Itanium ABI has
    // permanently locked the definition of POD to the rules of C++ TR1,
    // and that trickles down to derived ABIs.
    case GenericItanium:
    case GenericAArch64:
    case GenericARM:
    case Emscripten: // @LOCALMOD Emscripten
    case iOS:
    case GenericMIPS:
      return UseTailPaddingUnlessPOD03;

    // iOS on ARM64 and WebAssembly use the C++11 POD rules.  They do not honor
    // the Itanium exception about classes with over-large bitfields.
    case iOS64:
    case WebAssembly:
    case WatchOS:
      return UseTailPaddingUnlessPOD11;

    // MSVC always allocates fields in the tail-padding of a base class
    // subobject, even if they're POD.
    case Microsoft:
      return AlwaysUseTailPadding;
    }
    llvm_unreachable("bad ABI kind");
  }

  friend bool operator==(const TargetCXXABI &left, const TargetCXXABI &right) {
    return left.getKind() == right.getKind();
  }

  friend bool operator!=(const TargetCXXABI &left, const TargetCXXABI &right) {
    return !(left == right);
  }
};

}  // end namespace clang

#endif
