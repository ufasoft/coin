/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#pragma once

#if _MSC_VER < 1400   // removed in VS8
	#pragma warning(disable: 4270)
	// nonstandard extension used: 'initializing': a non-const 'type1' must be initialized with an l-value, not a function returning 'type2'

	#pragma warning(disable: 4284)
	// return type for 'identifier::operator ->' is not a UDT or reference to a UDT
#endif

#pragma warning(disable: 4061) // enumerator ' identifier ' in switch of enum ' enumeration ' is not explicitly handled by a case label  
#pragma warning(disable: 4062) // identifier ' in switch of enum ' enumeration ' is not handled 
#pragma warning(disable: 4091) // ignored on left of '' when no variable is declared
#pragma warning(disable: 4100) // unreferenced formal parameter 
#pragma warning(disable: 4103) // alignment changed after including header, may be due to missing #pragma pack(pop)
#pragma warning(disable: 4115) // named type definition in parentheses
#pragma warning(disable: 4127) // conditional expression is constant
#pragma warning(disable: 4131) // 'function' : uses old-style declarator
#pragma warning(disable: 4189) // local variable is initialized but not referenced
#pragma warning(disable: 4191) // unsafe conversion from 'type of expression' to 'type required'
#pragma warning(disable: 4200) // nonstandard extension used : zero-sized array in struct/union
#pragma warning(disable: 4201) // nonstandard extension used : nameless struct/union 
#pragma warning(disable: 4214) // nonstandard extension used : bit field types other than int
#pragma warning(disable: 4238) // nonstandard extension used : class rvalue used as lvalue 
#pragma warning(disable: 4239) // nonstandard extension used : 'token' : conversion from 'type' to 'type'
#pragma warning(disable: 4251) // class needs to have dll-interface to be used by clients of class
#pragma warning(disable: 4255) // no function prototype given: converting '()' to '(void)' 
#pragma warning(disable: 4265) // class has virtual functions, but destructor is not virtual
#pragma warning(disable: 4267) // 'initializing' : conversion from 'size_t' to 'int', possible loss of data
#pragma warning(disable: 4275) // non dll-interface class used as base for dll-interface class
#pragma warning(disable: 4296) // expression is always false 
#pragma warning(disable: 4297) // function assumed not to throw an exception but does
#pragma warning(disable: 4315) // pointer for member may not be aligned 8 as expected by the constructor
#pragma warning(disable: 4324) // structure was padded due to __declspec(align())
#pragma warning(disable: 4342) // 'function' called, but a member operator was called in previous versions
#pragma warning(disable: 4350) // behavior change: 'member1' called instead of 'member2' 
#pragma warning(disable: 4365) // conversion from X to Y, signed/unsigned mismatch
#pragma warning(disable: 4371) // layout of class may have changed from a previous version of the compiler due to better packing of member
#pragma warning(disable: 4389) // signed/unsigned mismatch
#pragma warning(disable: 4481) // nonstandard extension used: override specifier 'keyword'
#pragma warning(disable: 4482) // nonstandard extension used: enum 'enum' used in qualified name
#pragma warning(disable: 4512) // assignment operator could not be generated 
#pragma warning(disable: 4514) // unreferenced inline function has been removed
#pragma warning(disable: 4548) // expression before comma has no effect; expected expression with side-effect
#pragma warning(disable: 4555) // expression has no effect; expected expression with side-effect 

#if _MSC_VER<1700
#	pragma warning(disable: 4567) // behavior change due to parameter : calling convention incompatible with previous compiler versions
#endif

#pragma warning(disable: 4571) // catch(...) semantics changed since Visual C++ 7.1; structured exceptions (SEH) are no longer caught

#if _MSC_VER>=1600
#	pragma warning(disable: 4574) // is defined, did you mean to use #if
#endif

#if _MSC_VER>=1700
#	pragma warning(disable: 4435)	//	'class1' : Object layout under /vd2 will change due to virtual base 'class2'
#endif

//!!! #pragma warning(disable: 4609) // warning number N unavailable; /analyze must be specified for this warning
#pragma warning(disable: 4619) // warning number N unavailable; /analyze must be specified for this warning //!!!
#pragma warning(disable: 4625) // copy constructor could not be generated because a base class copy constructor is inaccessible 
#pragma warning(disable: 4626) // assignment operator could not be generated because a base class assignment operator is inaccessible 
#pragma warning(disable: 4640) // construction of local static object is not thread-safe
//!!!R #pragma warning(disable: 4668) // SYMBOL is not defined as a preprocessor macro, replacing with '0' for '#if/#elif'
#pragma warning(disable: 4702)  // unreachable code caused by optimizations
#pragma warning(disable: 4706) // assignment within conditional expression 
#pragma warning(disable: 4710) // function not inlined
#pragma warning(disable: 4711) // function selected for inline expansion
#pragma warning(disable: 4738) // storing 32-bit float result in memory, possible loss of performance
#pragma warning(disable: 4793) // causes native code generation for function
#pragma warning(disable: 4800) // forcing value to bool 'true' or 'false' of class (performance warning)
#pragma warning(disable: 4815) // ' var ' : zero-sized array in stack object will have no elements (unless the object is an aggregate that has been aggregate initialized) 
#pragma warning(disable: 4820) // bytes padding added after data member
#pragma warning(disable: 4826) // Conversion is sign-extended. This may cause unexpected runtime behavior.
#pragma warning(disable: 4836) // local types or unnamed types cannot be used as template arguments
#pragma warning(disable: 4917) // a GUID can only be associated with a class, interface or namespace
#pragma warning(disable: 4928) // illegal copy-initialization; more than one user-defined conversion has been implicitly applied 
#pragma warning(disable: 4986) // exception specification does not match previous declaration
#pragma warning(disable: 4987) // nonstandard extension used: 'throw (...)'
#pragma warning(disable: 4996) // was declared deprecated 

#pragma warning(disable: 4018) // signed/unsigned mismatch
#pragma warning(disable: 4733) // Inline asm assigning to 'FS:0' 
#pragma warning(disable: 4946) // reinterpret_cast used between related classes: 'class1' and 'class2'


//#pragma warning(disable: 4786)  // name truncated

//!!!R 4114
#pragma warning(disable:  4231 4250 4355 4927)


// these definitions equal to -Wall compiler option

#pragma warning(default: 4242) // 'identifier' : conversion from 'type1' to 'type2', possible loss of data
#pragma warning(default: 4254) // 'operator' : conversion from 'type1' to 'type2', possible loss of data
#pragma warning(default: 4263) // member function does not override any base class virtual member function
#pragma warning(default: 4264) // no override available for virtual member function from base 'class'; function is hidden
#pragma warning(default: 4266) // no override available for virtual member function from base 'type'; function is hidden
#pragma warning(default: 4287) // 'operator' : unsigned/negative constant mismatch
#pragma warning(default: 4289) // nonstandard extension used : 'var' : loop control variable declared in the for-loop is used outside the for-loop scope
#pragma warning(default: 4302) // 'conversion' : truncation from 'type 1' to 'type 2'
#pragma warning(default: 4339) // 'type' : use of undefined type detected in CLR meta-data - use of this type may lead to a runtime exception
//#pragma warning(default: 4342) // behavior change: 'function' called, but a member operator was called in previous versions
#pragma warning(default: 4347) // behavior change: 'function template' is called instead of 'function'

#if UCFG_FULL && !UCFG_STDSTL
#	pragma warning(default: 4350) // behavior change: 'member1' called instead of 'member2'
#endif

#pragma warning(default: 4390) // ';' : empty controlled statement found; is this the intent?
#pragma warning(default: 4412) // 'function' : function signature contains type 'type'; C++ objects are unsafe to pass between pure code and mixed or native.
#pragma warning(default: 4431) // missing type specifier - int assumed. Note: C no longer supports default-int
#pragma warning(default: 4536) // 'type name' : type-name exceeds meta-data limit of 'limit' characters
#pragma warning(default: 4545) // expression before comma evaluates to a function which is missing an argument list
#pragma warning(default: 4546) // function call before comma missing argument list
#pragma warning(default: 4547) // 'operator' : operator before comma has no effect; expected operator with side-effect
#pragma warning(default: 4549) // 'operator' : operator before comma has no effect; did you intend 'operator'?
#pragma warning(default: 4557) // '__assume' contains effect 'effect'
#pragma warning(default: 4623) // 'derived class' : default constructor could not be generated because a base class default constructor is inaccessible 
#pragma warning(default: 4628) // digraphs not supported with -Ze. Character sequence 'digraph' not interpreted as alternate token for 'char'
#pragma warning(default: 4641) // XML document comment has an ambiguous cross reference

#ifdef __cplusplus
#	pragma warning(default: 4668) // 'symbol' is not defined as a preprocessor macro, replacing with '0' for 'directives'
#endif

#pragma warning(default: 4682) // 'parameter' : no directional parameter attribute specified, defaulting to [in]
#pragma warning(default: 4686) // 'user-defined type' : possible change in behavior, change in UDT return calling convention
#pragma warning(default: 4692) // 'function': signature of non-private member contains assembly private native type 'native_type'
#pragma warning(default: 4837) // trigraph detected: '??%c' replaced by '%c'
#pragma warning(default: 4905) // wide string literal cast to 'LPSTR'
#pragma warning(default: 4906) // string literal cast to 'LPWSTR'
#pragma warning(default: 4931) // we are assuming the type library was built for number-bit pointers
#pragma warning(default: 4962) // 'function' : Profile-guided optimizations disabled because optimizations caused profile data to become inconsistent"


