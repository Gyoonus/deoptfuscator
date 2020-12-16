HiddenApi
=========

This tool iterates over all class members inside given DEX files and modifies
their access flags if their signatures appear on one of two lists - greylist and
blacklist - provided as text file inputs. These access flags denote to the
runtime that the marked methods/fields should be treated as internal APIs with
access restricted only to platform code. Methods/fields not mentioned on the two
lists are assumed to be on a whitelist and left accessible by all code.

API signatures
==============

The methods/fields to be marked are specified in two text files (greylist,
blacklist) provided an input. Only one signature per line is allowed.

Types are expected in their DEX format - class descriptors are to be provided in
"slash" form, e.g. "Ljava/lang/Object;", primitive types in their shorty form,
e.g. "I" for "int", and a "[" prefix denotes an array type. Lists of types do
not use any separators, e.g. "ILxyz;F" for "int, xyz, float".

Methods are encoded as:
    `class_descriptor->method_name(parameter_types)return_type`

Fields are encoded as:
    `class_descriptor->field_name:field_type`

Bit encoding
============

Two bits of information are encoded in the DEX access flags. These are encoded
as unsigned LEB128 values in DEX and so as to not increase the size of the DEX,
different modifiers were chosen for different kinds of methods/fields.

First bit is encoded as the inversion of visibility access flags (bits 2:0).
At most one of these flags can be set at any given time. Inverting these bits
therefore produces a value where at least two bits are set and there is never
any loss of information.

Second bit is encoded differently for each given type of class member as there
is no single unused bit such that setting it would not increase the size of the
LEB128 encoding. The following bits are used:

 * bit 5 for fields as it carries no other meaning
 * bit 5 for non-native methods, as `synchronized` can only be set on native
   methods (the Java `synchronized` modifier is bit 17)
 * bit 9 for native methods, as it carries no meaning and bit 8 (`native`) will
   make the LEB128 encoding at least two bytes long

Two following bit encoding is used to denote the membership of a method/field:

 * whitelist: `false`, `false`
 * greylist: `true`, `false`
 * blacklist: `true`, `true`
