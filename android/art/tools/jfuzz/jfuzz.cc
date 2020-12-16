/*
 * Copyright 2016, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cmath>
#include <random>

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/time.h>

namespace {

/*
 * Operators.
 */

static constexpr const char* kIncDecOps[]   = { "++", "--" };
static constexpr const char* kIntUnaryOps[] = { "+", "-", "~" };
static constexpr const char* kFpUnaryOps[]  = { "+", "-" };

static constexpr const char* kBoolBinOps[] = { "&&", "||", "&", "|", "^" };  // few less common
static constexpr const char* kIntBinOps[]  = { "+", "-", "*", "/", "%",
                                               ">>", ">>>", "<<", "&", "|", "^" };
static constexpr const char* kFpBinOps[]   = { "+", "-", "*", "/" };

static constexpr const char* kBoolAssignOps[] = { "=", "&=" , "|=", "^=" };  // few less common
static constexpr const char* kIntAssignOps[]  = { "=", "+=", "-=", "*=", "/=", "%=",
                                                  ">>=", ">>>=", "<<=", "&=", "|=", "^=" };
static constexpr const char* kFpAssignOps[]   = { "=", "+=", "-=", "*=", "/=" };

static constexpr const char* kBoolRelOps[] = { "==", "!=" };
static constexpr const char* kRelOps[]     = { "==", "!=", ">", ">=", "<", "<=" };

/*
 * Exceptions.
 */
static const char* kExceptionTypes[] = {
  "IllegalStateException",
  "NullPointerException",
  "IllegalArgumentException",
  "ArrayIndexOutOfBoundsException"
};

/*
 * Version of JFuzz. Increase this each time changes are made to the program
 * to preserve the property that a given version of JFuzz yields the same
 * fuzzed program for a deterministic random seed.
 */
const char* VERSION = "1.5";

/*
 * Maximum number of array dimensions, together with corresponding maximum size
 * within each dimension (to keep memory/runtime requirements roughly the same).
 */
static const uint32_t kMaxDim = 10;
static const uint32_t kMaxDimSize[kMaxDim + 1] = { 0, 1000, 32, 10, 6, 4, 3, 3, 2, 2, 2 };

/*
 * Utility function to return the number of elements in an array.
 */
template <typename T, uint32_t N>
constexpr uint32_t countof(T const (&)[N]) {
  return N;
}

/**
 * A class that generates a random program that compiles correctly. The program
 * is generated using rules that generate various programming constructs. Each rule
 * has a fixed probability to "fire". Running a generated program yields deterministic
 * output, making it suited to test various modes of execution (e.g an interpreter vs.
 * an compiler or two different run times) for divergences.
 */
class JFuzz {
 public:
  JFuzz(FILE* out,
        uint32_t seed,
        uint32_t expr_depth,
        uint32_t stmt_length,
        uint32_t if_nest,
        uint32_t loop_nest,
        uint32_t try_nest)
      : out_(out),
        fuzz_random_engine_(seed),
        fuzz_seed_(seed),
        fuzz_expr_depth_(expr_depth),
        fuzz_stmt_length_(stmt_length),
        fuzz_if_nest_(if_nest),
        fuzz_loop_nest_(loop_nest),
        fuzz_try_nest_(try_nest),
        return_type_(randomType()),
        array_type_(randomType()),
        array_dim_(random1(kMaxDim)),
        array_size_(random1(kMaxDimSize[array_dim_])),
        indentation_(0),
        expr_depth_(0),
        stmt_length_(0),
        if_nest_(0),
        loop_nest_(0),
        switch_nest_(0),
        do_nest_(0),
        try_nest_(0),
        boolean_local_(0),
        int_local_(0),
        long_local_(0),
        float_local_(0),
        double_local_(0),
        in_inner_(false) { }

  ~JFuzz() { }

  void emitProgram() {
    emitHeader();
    emitTestClassWithMain();
  }

 private:
  //
  // Types.
  //

  // Current type of each expression during generation.
  enum Type {
    kBoolean,
    kInt,
    kLong,
    kFloat,
    kDouble
  };

  // Test for an integral type.
  static bool isInteger(Type tp) {
    return tp == kInt || tp == kLong;
  }

  // Test for a floating-point type.
  static bool isFP(Type tp) {
    return tp == kFloat || tp == kDouble;
  }

  // Emit type.
  void emitType(Type tp) const {
    switch (tp) {
      case kBoolean: fputs("boolean", out_); break;
      case kInt:     fputs("int",     out_); break;
      case kLong:    fputs("long",    out_); break;
      case kFloat:   fputs("float",   out_); break;
      case kDouble:  fputs("double",  out_); break;
    }
  }

  // Emit type class.
  void emitTypeClass(Type tp) const {
    switch (tp) {
      case kBoolean: fputs("Boolean", out_); break;
      case kInt:     fputs("Integer", out_); break;
      case kLong:    fputs("Long",    out_); break;
      case kFloat:   fputs("Float",   out_); break;
      case kDouble:  fputs("Double",  out_); break;
    }
  }

  // Return a random type.
  Type randomType() {
    switch (random1(5)) {
      case 1:  return kBoolean;
      case 2:  return kInt;
      case 3:  return kLong;
      case 4:  return kFloat;
      default: return kDouble;
    }
  }

  // Emits a random strong selected from an array of operator strings.
  template <std::uint32_t N>
  inline void emitOneOf(const char* const (&ops)[N]) {
    fputs(ops[random0(N)], out_);
  }

  //
  // Expressions.
  //

  // Emit an unary operator (same type in-out).
  void emitUnaryOp(Type tp) {
    if (tp == kBoolean) {
      fputc('!', out_);
    } else if (isInteger(tp)) {
      emitOneOf(kIntUnaryOps);
    } else {  // isFP(tp)
      emitOneOf(kFpUnaryOps);
    }
  }

  // Emit a pre/post-increment/decrement operator (same type in-out).
  void emitIncDecOp(Type tp) {
    if (tp == kBoolean) {
      // Not applicable, just leave "as is".
    } else {  // isInteger(tp) || isFP(tp)
      emitOneOf(kIncDecOps);
    }
  }

  // Emit a binary operator (same type in-out).
  void emitBinaryOp(Type tp) {
    if (tp == kBoolean) {
      emitOneOf(kBoolBinOps);
    } else if (isInteger(tp)) {
      emitOneOf(kIntBinOps);
    } else {  // isFP(tp)
      emitOneOf(kFpBinOps);
    }
  }

  // Emit an assignment operator (same type in-out).
  void emitAssignmentOp(Type tp) {
    if (tp == kBoolean) {
      emitOneOf(kBoolAssignOps);
    } else if (isInteger(tp)) {
      emitOneOf(kIntAssignOps);
    } else {  // isFP(tp)
      emitOneOf(kFpAssignOps);
    }
  }

  // Emit a relational operator (one type in, boolean out).
  void emitRelationalOp(Type tp) {
    if (tp == kBoolean) {
      emitOneOf(kBoolRelOps);
    } else {  // isInteger(tp) || isFP(tp)
      emitOneOf(kRelOps);
    }
  }

  // Emit a type conversion operator sequence (out type given, new suitable in type picked).
  Type emitTypeConversionOp(Type tp) {
    if (tp == kInt) {
      switch (random1(5)) {
        case 1: fputs("(int)", out_); return kLong;
        case 2: fputs("(int)", out_); return kFloat;
        case 3: fputs("(int)", out_); return kDouble;
        // Narrowing-widening.
        case 4: fputs("(int)(byte)(int)",  out_); return kInt;
        case 5: fputs("(int)(short)(int)", out_); return kInt;
      }
    } else if (tp == kLong) {
      switch (random1(6)) {
        case 1: /* implicit */         return kInt;
        case 2: fputs("(long)", out_); return kFloat;
        case 3: fputs("(long)", out_); return kDouble;
        // Narrowing-widening.
        case 4: fputs("(long)(byte)(long)",  out_); return kLong;
        case 5: fputs("(long)(short)(long)", out_); return kLong;
        case 6: fputs("(long)(int)(long)",   out_); return kLong;
      }
    } else if (tp == kFloat) {
      switch (random1(4)) {
        case 1: fputs("(float)", out_); return kInt;
        case 2: fputs("(float)", out_); return kLong;
        case 3: fputs("(float)", out_); return kDouble;
        // Narrowing-widening.
        case 4: fputs("(float)(int)(float)", out_); return kFloat;
      }
    } else if (tp == kDouble) {
      switch (random1(5)) {
        case 1: fputs("(double)", out_); return kInt;
        case 2: fputs("(double)", out_); return kLong;
        case 3: fputs("(double)", out_); return kFloat;
        // Narrowing-widening.
        case 4: fputs("(double)(int)(double)",   out_); return kDouble;
        case 5: fputs("(double)(float)(double)", out_); return kDouble;
      }
    }
    return tp;  // nothing suitable, just keep type
  }

  // Emit a type conversion (out type given, new suitable in type picked).
  void emitTypeConversion(Type tp) {
    if (tp == kBoolean) {
      Type tp = randomType();
      emitExpression(tp);
      fputc(' ', out_);
      emitRelationalOp(tp);
      fputc(' ', out_);
      emitExpression(tp);
    } else {
      tp = emitTypeConversionOp(tp);
      fputc(' ', out_);
      emitExpression(tp);
    }
  }

  // Emit an unary intrinsic (out type given, new suitable in type picked).
  Type emitIntrinsic1(Type tp) {
    if (tp == kBoolean) {
      switch (random1(6)) {
        case 1: fputs("Float.isNaN",       out_); return kFloat;
        case 2: fputs("Float.isFinite",    out_); return kFloat;
        case 3: fputs("Float.isInfinite",  out_); return kFloat;
        case 4: fputs("Double.isNaN",      out_); return kDouble;
        case 5: fputs("Double.isFinite",   out_); return kDouble;
        case 6: fputs("Double.isInfinite", out_); return kDouble;
      }
    } else if (isInteger(tp)) {
      const char* prefix = tp == kLong ? "Long" : "Integer";
      switch (random1(13)) {
        case 1: fprintf(out_, "%s.highestOneBit",         prefix); break;
        case 2: fprintf(out_, "%s.lowestOneBit",          prefix); break;
        case 3: fprintf(out_, "%s.numberOfLeadingZeros",  prefix); break;
        case 4: fprintf(out_, "%s.numberOfTrailingZeros", prefix); break;
        case 5: fprintf(out_, "%s.bitCount",              prefix); break;
        case 6: fprintf(out_, "%s.signum",                prefix); break;
        case 7: fprintf(out_, "%s.reverse",               prefix); break;
        case 8: fprintf(out_, "%s.reverseBytes",          prefix); break;
        case 9:  fputs("Math.incrementExact", out_); break;
        case 10: fputs("Math.decrementExact", out_); break;
        case 11: fputs("Math.negateExact",    out_); break;
        case 12: fputs("Math.abs",            out_); break;
        case 13: fputs("Math.round", out_);
                 return tp == kLong ? kDouble : kFloat;
      }
    } else {  // isFP(tp)
      switch (random1(6)) {
        case 1: fputs("Math.abs",      out_); break;
        case 2: fputs("Math.ulp",      out_); break;
        case 3: fputs("Math.signum",   out_); break;
        case 4: fputs("Math.nextUp",   out_); break;
        case 5: fputs("Math.nextDown", out_); break;
        case 6: if (tp == kDouble) {
                  fputs("Double.longBitsToDouble", out_);
                  return kLong;
                } else {
                  fputs("Float.intBitsToFloat", out_);
                  return kInt;
                }
      }
    }
    return tp;  // same type in-out
  }

  // Emit a binary intrinsic (out type given, new suitable in type picked).
  Type emitIntrinsic2(Type tp) {
    if (tp == kBoolean) {
      switch (random1(3)) {
        case 1: fputs("Boolean.logicalAnd", out_); break;
        case 2: fputs("Boolean.logicalOr",  out_); break;
        case 3: fputs("Boolean.logicalXor", out_); break;
      }
    } else if (isInteger(tp)) {
      const char* prefix = tp == kLong ? "Long" : "Integer";
      switch (random1(11)) {
        case 1: fprintf(out_, "%s.compare", prefix); break;
        case 2: fprintf(out_, "%s.sum",     prefix); break;
        case 3: fprintf(out_, "%s.min",     prefix); break;
        case 4: fprintf(out_, "%s.max",     prefix); break;
        case 5:  fputs("Math.min",           out_); break;
        case 6:  fputs("Math.max",           out_); break;
        case 7:  fputs("Math.floorDiv",      out_); break;
        case 8:  fputs("Math.floorMod",      out_); break;
        case 9:  fputs("Math.addExact",      out_); break;
        case 10: fputs("Math.subtractExact", out_); break;
        case 11: fputs("Math.multiplyExact", out_); break;
      }
    } else {  // isFP(tp)
      const char* prefix = tp == kDouble ? "Double" : "Float";
      switch (random1(5)) {
        case 1: fprintf(out_, "%s.sum", prefix); break;
        case 2: fprintf(out_, "%s.min", prefix); break;
        case 3: fprintf(out_, "%s.max", prefix); break;
        case 4: fputs("Math.min", out_); break;
        case 5: fputs("Math.max", out_); break;
      }
    }
    return tp;  // same type in-out
  }

  // Emit an intrinsic (out type given, new suitable in type picked).
  void emitIntrinsic(Type tp) {
    if (random1(2) == 1) {
      tp = emitIntrinsic1(tp);
      fputc('(', out_);
      emitExpression(tp);
      fputc(')', out_);
    } else {
      tp = emitIntrinsic2(tp);
      fputc('(', out_);
      emitExpression(tp);
      fputs(", ", out_);
      emitExpression(tp);
      fputc(')', out_);
    }
  }

  // Emit a method call (out type given).
  void emitMethodCall(Type tp) {
    if (tp != kBoolean && !in_inner_) {
      // Accept all numerical types (implicit conversion) and when not
      // declaring inner classes (to avoid infinite recursion).
      switch (random1(8)) {
        case 1: fputs("mA.a()",  out_); break;
        case 2: fputs("mB.a()",  out_); break;
        case 3: fputs("mB.x()",  out_); break;
        case 4: fputs("mBX.x()", out_); break;
        case 5: fputs("mC.s()",  out_); break;
        case 6: fputs("mC.c()",  out_); break;
        case 7: fputs("mC.x()",  out_); break;
        case 8: fputs("mCX.x()", out_); break;
      }
    } else {
      // Fall back to intrinsic.
      emitIntrinsic(tp);
    }
  }

  // Emit unboxing boxed object.
  void emitUnbox(Type tp) {
    fputc('(', out_);
    emitType(tp);
    fputs(") new ", out_);
    emitTypeClass(tp);
    fputc('(', out_);
    emitExpression(tp);
    fputc(')', out_);
  }

  // Emit miscellaneous constructs.
  void emitMisc(Type tp) {
    if (tp == kBoolean) {
      fprintf(out_, "this instanceof %s", in_inner_ ? "X" : "Test");
    } else if (isInteger(tp)) {
      const char* prefix = tp == kLong ? "Long" : "Integer";
      switch (random1(2)) {
        case 1: fprintf(out_, "%s.MIN_VALUE", prefix); break;
        case 2: fprintf(out_, "%s.MAX_VALUE", prefix); break;
      }
    } else {  // isFP(tp)
      const char* prefix = tp == kDouble ? "Double" : "Float";
      switch (random1(6)) {
        case 1: fprintf(out_, "%s.MIN_NORMAL", prefix);        break;
        case 2: fprintf(out_, "%s.MIN_VALUE", prefix);         break;
        case 3: fprintf(out_, "%s.MAX_VALUE", prefix);         break;
        case 4: fprintf(out_, "%s.POSITIVE_INFINITY", prefix); break;
        case 5: fprintf(out_, "%s.NEGATIVE_INFINITY", prefix); break;
        case 6: fprintf(out_, "%s.NaN", prefix);               break;
      }
    }
  }

  // Adjust local of given type and return adjusted value.
  uint32_t adjustLocal(Type tp, int32_t a) {
    switch (tp) {
      case kBoolean: boolean_local_ += a; return boolean_local_;
      case kInt:     int_local_     += a; return int_local_;
      case kLong:    long_local_    += a; return long_local_;
      case kFloat:   float_local_   += a; return float_local_;
      default:       double_local_  += a; return double_local_;
    }
  }

  // Emit an expression that is a strict upper bound for an array index.
  void emitUpperBound() {
    if (random1(8) == 1) {
      fputs("mArray.length", out_);
    } else if (random1(8) == 1) {
      fprintf(out_, "%u", random1(array_size_));  // random in range
    } else {
      fprintf(out_, "%u", array_size_);
    }
  }

  // Emit an array index, usually within proper range.
  void emitArrayIndex() {
    if (loop_nest_ > 0 && random1(2) == 1) {
      fprintf(out_, "i%u", random0(loop_nest_));
    } else if (random1(8) == 1) {
      fputs("mArray.length - 1", out_);
    } else {
      fprintf(out_, "%u", random0(array_size_));  // random in range
    }
    // Introduce potential off by one errors with low probability.
    if (random1(100) == 1) {
      if (random1(2) == 1) {
        fputs(" - 1", out_);
      } else {
        fputs(" + 1", out_);
      }
    }
  }

  // Emit a literal.
  void emitLiteral(Type tp) {
    switch (tp) {
      case kBoolean: fputs(random1(2) == 1 ? "true" : "false", out_); break;
      case kInt:     fprintf(out_, "%d",    random()); break;
      case kLong:    fprintf(out_, "%dL",   random()); break;
      case kFloat:   fprintf(out_, "%d.0f", random()); break;
      case kDouble:  fprintf(out_, "%d.0",  random()); break;
    }
  }

  // Emit array variable, if available.
  bool emitArrayVariable(Type tp) {
    if (tp == array_type_) {
      fputs("mArray", out_);
      for (uint32_t i = 0; i < array_dim_; i++) {
        fputc('[', out_);
        emitArrayIndex();
        fputc(']', out_);
      }
      return true;
    }
    return false;
  }

  // Emit a local variable, if available.
  bool emitLocalVariable(Type tp) {
    uint32_t locals = adjustLocal(tp, 0);
    if (locals > 0) {
      uint32_t local = random0(locals);
      switch (tp) {
        case kBoolean: fprintf(out_, "lZ%u", local); break;
        case kInt:     fprintf(out_, "lI%u", local); break;
        case kLong:    fprintf(out_, "lJ%u", local); break;
        case kFloat:   fprintf(out_, "lF%u", local); break;
        case kDouble:  fprintf(out_, "lD%u", local); break;
      }
      return true;
    }
    return false;
  }

  // Emit a field variable.
  void emitFieldVariable(Type tp) {
    switch (tp) {
      case kBoolean:fputs("mZ", out_); break;
      case kInt:    fputs("mI", out_); break;
      case kLong:   fputs("mJ", out_); break;
      case kFloat:  fputs("mF", out_); break;
      case kDouble: fputs("mD", out_); break;
    }
  }

  // Emit a variable.
  void emitVariable(Type tp) {
    switch (random1(4)) {
      case 1:
        if (emitArrayVariable(tp))
          return;
        // FALL-THROUGH
      case 2:
        if (emitLocalVariable(tp))
          return;
        // FALL-THROUGH
      default:
        emitFieldVariable(tp);
        break;
    }
  }

  // Emit an expression.
  void emitExpression(Type tp) {
    // Continuing expression becomes less likely as the depth grows.
    if (random1(expr_depth_ + 1) > fuzz_expr_depth_) {
      if (random1(2) == 1) {
        emitLiteral(tp);
      } else {
        emitVariable(tp);
      }
      return;
    }

    expr_depth_++;

    fputc('(', out_);
    switch (random1(12)) {  // favor binary operations
      case 1:
        // Unary operator: ~ x
        emitUnaryOp(tp);
        fputc(' ', out_);
        emitExpression(tp);
        break;
      case 2:
        // Pre-increment: ++x
        emitIncDecOp(tp);
        emitVariable(tp);
        break;
      case 3:
        // Post-increment: x++
        emitVariable(tp);
        emitIncDecOp(tp);
        break;
      case 4:
        // Ternary operator: b ? x : y
        emitExpression(kBoolean);
        fputs(" ? ", out_);
        emitExpression(tp);
        fputs(" : ", out_);
        emitExpression(tp);
        break;
      case 5:
        // Type conversion: (float) x
        emitTypeConversion(tp);
        break;
      case 6:
        // Intrinsic: foo(x)
        emitIntrinsic(tp);
        break;
      case 7:
        // Method call: mA.a()
        emitMethodCall(tp);
        break;
      case 8:
        // Emit unboxing boxed value: (int) Integer(x)
        emitUnbox(tp);
        break;
      case 9:
        // Miscellaneous constructs: a.length
        emitMisc(tp);
        break;
      default:
        // Binary operator: x + y
        emitExpression(tp);
        fputc(' ', out_);
        emitBinaryOp(tp);
        fputc(' ', out_);
        emitExpression(tp);
        break;
    }
    fputc(')', out_);

    --expr_depth_;
  }

  //
  // Statements.
  //

  // Emit current indentation.
  void emitIndentation() const {
    for (uint32_t i = 0; i < indentation_; i++) {
      fputc(' ', out_);
    }
  }

  // Emit a return statement.
  bool emitReturn(bool mustEmit) {
    // Only emit when we must, or with low probability inside ifs/loops,
    // but outside do-while to avoid confusing the may follow status.
    if (mustEmit || ((if_nest_ + loop_nest_) > 0 && do_nest_ == 0 && random1(10) == 1)) {
      fputs("return ", out_);
      emitExpression(return_type_);
      fputs(";\n", out_);
      return false;
    }
    // Fall back to assignment.
    return emitAssignment();
  }

  // Emit a continue statement.
  bool emitContinue() {
    // Only emit with low probability inside loops.
    if (loop_nest_ > 0 && random1(10) == 1) {
      fputs("continue;\n", out_);
      return false;
    }
    // Fall back to assignment.
    return emitAssignment();
  }

  // Emit a break statement.
  bool emitBreak() {
    // Only emit with low probability inside loops, but outside switches
    // to avoid confusing the may follow status.
    if (loop_nest_ > 0 && switch_nest_ == 0 && random1(10) == 1) {
      fputs("break;\n", out_);
      return false;
    }
    // Fall back to assignment.
    return emitAssignment();
  }

  // Emit a new scope with a local variable declaration statement.
  bool emitScope() {
    Type tp = randomType();
    fputs("{\n", out_);
    indentation_ += 2;
    emitIndentation();
    emitType(tp);
    switch (tp) {
      case kBoolean: fprintf(out_, " lZ%u = ", boolean_local_); break;
      case kInt:     fprintf(out_, " lI%u = ", int_local_);     break;
      case kLong:    fprintf(out_, " lJ%u = ", long_local_);    break;
      case kFloat:   fprintf(out_, " lF%u = ", float_local_);   break;
      case kDouble:  fprintf(out_, " lD%u = ", double_local_);  break;
    }
    emitExpression(tp);
    fputs(";\n", out_);

    adjustLocal(tp, 1);  // local now visible

    bool mayFollow = emitStatementList();

    adjustLocal(tp, -1);  // local no longer visible

    indentation_ -= 2;
    emitIndentation();
    fputs("}\n", out_);
    return mayFollow;
  }

  // Emit one dimension of an array initializer, where parameter dim >= 1
  // denotes the number of remaining dimensions that should be emitted.
  void emitArrayInitDim(int dim) {
    if (dim == 1) {
      // Last dimension: set of values.
      fputs("{ ", out_);
      for (uint32_t i = 0; i < array_size_; i++) {
        emitExpression(array_type_);
        fputs(", ", out_);
      }
      fputs("}", out_);

    } else {
      // Outer dimensions: set of sets.
      fputs("{\n", out_);
      indentation_ += 2;
      emitIndentation();

      for (uint32_t i = 0; i < array_size_; i++) {
        emitArrayInitDim(dim - 1);
        if (i != array_size_ - 1) {
          fputs(",\n", out_);
          emitIndentation();
        }
      }

      fputs(",\n", out_);
      indentation_ -= 2;
      emitIndentation();
      fputs("}", out_);
    }
  }

  // Emit an array initializer of the following form.
  //   {
  //     type[]..[] tmp = { .. };
  //     mArray = tmp;
  //   }
  bool emitArrayInit() {
    // Avoid elaborate array initializers.
    uint64_t p = pow(array_size_, array_dim_);
    if (p > 20) {
      return emitAssignment();  // fall back
    }

    fputs("{\n", out_);

    indentation_ += 2;
    emitIndentation();
    emitType(array_type_);
    for (uint32_t i = 0; i < array_dim_; i++) {
      fputs("[]", out_);
    }
    fputs(" tmp = ", out_);
    emitArrayInitDim(array_dim_);
    fputs(";\n", out_);

    emitIndentation();
    fputs("mArray = tmp;\n", out_);

    indentation_ -= 2;
    emitIndentation();
    fputs("}\n", out_);
    return true;
  }

  // Emit a for loop.
  bool emitForLoop() {
    // Continuing loop nest becomes less likely as the depth grows.
    if (random1(loop_nest_ + 1) > fuzz_loop_nest_) {
      return emitAssignment();  // fall back
    }

    bool goesUp = random1(2) == 1;
    fprintf(out_, "for (int i%u = ", loop_nest_);
    if (goesUp) {
      fprintf(out_, "0; i%u < ", loop_nest_);
      emitUpperBound();
      fprintf(out_, "; i%u++) {\n", loop_nest_);
    } else {
      emitUpperBound();
      fprintf(out_, " - 1; i%d >= 0", loop_nest_);
      fprintf(out_, "; i%d--) {\n", loop_nest_);
    }

    ++loop_nest_;  // now in loop

    indentation_ += 2;
    emitStatementList();

    --loop_nest_;  // no longer in loop

    indentation_ -= 2;
    emitIndentation();
    fprintf(out_, "}\n");
    return true;  // loop-body does not block flow
  }

  // Emit while or do-while loop.
  bool emitDoLoop() {
    // Continuing loop nest becomes less likely as the depth grows.
    if (random1(loop_nest_ + 1) > fuzz_loop_nest_) {
      return emitAssignment();  // fall back
    }

    bool isWhile = random1(2) == 1;
    fputs("{\n", out_);
    indentation_ += 2;
    emitIndentation();
    fprintf(out_, "int i%u = %d;\n", loop_nest_, isWhile ? -1 : 0);
    emitIndentation();
    if (isWhile) {
      fprintf(out_, "while (++i%u < ", loop_nest_);
      emitUpperBound();
      fputs(") {\n", out_);
    } else {
      fputs("do {\n", out_);
      do_nest_++;
    }

    ++loop_nest_;  // now in loop

    indentation_ += 2;
    emitStatementList();

    --loop_nest_;  // no longer in loop

    indentation_ -= 2;
    emitIndentation();
    if (isWhile) {
      fputs("}\n", out_);
    } else {
      fprintf(out_, "} while (++i%u < ", loop_nest_);
      emitUpperBound();
      fputs(");\n", out_);
      --do_nest_;
    }
    indentation_ -= 2;
    emitIndentation();
    fputs("}\n", out_);
    return true;  // loop-body does not block flow
  }

  // Emit an if statement.
  bool emitIfStmt() {
    // Continuing if nest becomes less likely as the depth grows.
    if (random1(if_nest_ + 1) > fuzz_if_nest_) {
      return emitAssignment();  // fall back
    }

    fputs("if (", out_);
    emitExpression(kBoolean);
    fputs(") {\n", out_);

    ++if_nest_;  // now in if

    indentation_ += 2;
    bool mayFollowTrue = emitStatementList();
    indentation_ -= 2;
    emitIndentation();
    fprintf(out_, "} else {\n");
    indentation_ += 2;
    bool mayFollowFalse = emitStatementList();

    --if_nest_;  // no longer in if

    indentation_ -= 2;
    emitIndentation();
    fprintf(out_, "}\n");
    return mayFollowTrue || mayFollowFalse;
  }

  bool emitTry() {
    fputs("try {\n", out_);
    indentation_ += 2;
    bool mayFollow = emitStatementList();
    indentation_ -= 2;
    emitIndentation();
    fputc('}', out_);
    return mayFollow;
  }

  bool emitCatch() {
    uint32_t count = random1(countof(kExceptionTypes));
    bool mayFollow = false;
    for (uint32_t i = 0; i < count; ++i) {
      fprintf(out_, " catch (%s ex%u_%u) {\n", kExceptionTypes[i], try_nest_, i);
      indentation_ += 2;
      mayFollow |= emitStatementList();
      indentation_ -= 2;
      emitIndentation();
      fputc('}', out_);
    }
    return mayFollow;
  }

  bool emitFinally() {
    fputs(" finally {\n", out_);
    indentation_ += 2;
    bool mayFollow = emitStatementList();
    indentation_ -= 2;
    emitIndentation();
    fputc('}', out_);
    return mayFollow;
  }

  // Emit a try-catch-finally block.
  bool emitTryCatchFinally() {
    // Apply a hard limit on the number of catch blocks. This is for
    // javac which fails if blocks within try-catch-finally are too
    // large (much less than you'd expect).
    if (try_nest_ > fuzz_try_nest_) {
      return emitAssignment();  // fall back
    }

    ++try_nest_;  // Entering try-catch-finally

    bool mayFollow = emitTry();
    switch (random0(3)) {
      case 0:  // try..catch
        mayFollow |= emitCatch();
        break;
      case 1:  // try..finally
        mayFollow &= emitFinally();
        break;
      case 2:  // try..catch..finally
        // When determining whether code may follow, we observe that a
        // finally block always follows after try and catch
        // block. Code may only follow if the finally block permits
        // and either the try or catch block allows code to follow.
        mayFollow = (mayFollow | emitCatch()) & emitFinally();
        break;
    }
    fputc('\n', out_);

    --try_nest_;  // Leaving try-catch-finally
    return mayFollow;
  }

  // Emit a switch statement.
  bool emitSwitch() {
    // Continuing if nest becomes less likely as the depth grows.
    if (random1(if_nest_ + 1) > fuzz_if_nest_) {
      return emitAssignment();  // fall back
    }

    bool mayFollow = false;
    fputs("switch (", out_);
    emitArrayIndex();  // restrict its range
    fputs(") {\n", out_);

    ++if_nest_;
    ++switch_nest_;  // now in switch

    indentation_ += 2;
    for (uint32_t i = 0; i < 2; i++) {
      emitIndentation();
      if (i == 0) {
        fprintf(out_, "case %u: {\n", random0(array_size_));
      } else {
        fprintf(out_, "default: {\n");
      }
      indentation_ += 2;
      if (emitStatementList()) {
        // Must end with break.
        emitIndentation();
        fputs("break;\n", out_);
        mayFollow = true;
      }
      indentation_ -= 2;
      emitIndentation();
      fputs("}\n", out_);
    }

    --if_nest_;
    --switch_nest_;  // no longer in switch

    indentation_ -= 2;
    emitIndentation();
    fprintf(out_, "}\n");
    return mayFollow;
  }

  bool emitNopCall() {
    fputs("nop();\n", out_);
    return true;
  }

  // Emit an assignment statement.
  bool emitAssignment() {
    Type tp = randomType();
    emitVariable(tp);
    fputc(' ', out_);
    emitAssignmentOp(tp);
    fputc(' ', out_);
    emitExpression(tp);
    fputs(";\n", out_);
    return true;
  }

  // Emit a single statement. Returns true if statements may follow.
  bool emitStatement() {
    switch (random1(16)) {  // favor assignments
      case 1:  return emitReturn(false);     break;
      case 2:  return emitContinue();        break;
      case 3:  return emitBreak();           break;
      case 4:  return emitScope();           break;
      case 5:  return emitArrayInit();       break;
      case 6:  return emitForLoop();         break;
      case 7:  return emitDoLoop();          break;
      case 8:  return emitIfStmt();          break;
      case 9:  return emitSwitch();          break;
      case 10: return emitTryCatchFinally(); break;
      case 11: return emitNopCall();         break;
      default: return emitAssignment();      break;
    }
  }

  // Emit a statement list. Returns true if statements may follow.
  bool emitStatementList() {
    while (stmt_length_ < 1000) {  // avoid run-away
      stmt_length_++;
      emitIndentation();
      if (!emitStatement()) {
        return false;  // rest would be dead code
      }
      // Continuing this list becomes less likely as the total statement list grows.
      if (random1(stmt_length_) > fuzz_stmt_length_) {
        break;
      }
    }
    return true;
  }

  // Emit interface and class declarations.
  void emitClassDecls() {
    in_inner_ = true;
    fputs("  private interface X {\n", out_);
    fputs("    int x();\n", out_);
    fputs("  }\n\n", out_);
    fputs("  private class A {\n", out_);
    fputs("    public int a() {\n", out_);
    fputs("      return ", out_);
    emitExpression(kInt);
    fputs(";\n    }\n", out_);
    fputs("  }\n\n", out_);
    fputs("  private class B extends A implements X {\n", out_);
    fputs("    public int a() {\n", out_);
    fputs("      return super.a() + ", out_);
    emitExpression(kInt);
    fputs(";\n    }\n", out_);
    fputs("    public int x() {\n", out_);
    fputs("      return ", out_);
    emitExpression(kInt);
    fputs(";\n    }\n", out_);
    fputs("  }\n\n", out_);
    fputs("  private static class C implements X {\n", out_);
    fputs("    public static int s() {\n", out_);
    fputs("      return ", out_);
    emitLiteral(kInt);
    fputs(";\n    }\n", out_);
    fputs("    public int c() {\n", out_);
    fputs("      return ", out_);
    emitLiteral(kInt);
    fputs(";\n    }\n", out_);
    fputs("    public int x() {\n", out_);
    fputs("      return ", out_);
    emitLiteral(kInt);
    fputs(";\n    }\n", out_);
    fputs("  }\n\n", out_);
    in_inner_ = false;
  }

  // Emit field declarations.
  void emitFieldDecls() {
    fputs("  private A mA  = new B();\n", out_);
    fputs("  private B mB  = new B();\n", out_);
    fputs("  private X mBX = new B();\n", out_);
    fputs("  private C mC  = new C();\n", out_);
    fputs("  private X mCX = new C();\n\n", out_);
    fputs("  private boolean mZ = false;\n", out_);
    fputs("  private int     mI = 0;\n", out_);
    fputs("  private long    mJ = 0;\n", out_);
    fputs("  private float   mF = 0;\n", out_);
    fputs("  private double  mD = 0;\n\n", out_);
  }

  // Emit array declaration.
  void emitArrayDecl() {
    fputs("  private ", out_);
    emitType(array_type_);
    for (uint32_t i = 0; i < array_dim_; i++) {
      fputs("[]", out_);
    }
    fputs(" mArray = new ", out_);
    emitType(array_type_);
    for (uint32_t i = 0; i < array_dim_; i++) {
      fprintf(out_, "[%d]", array_size_);
    }
    fputs(";\n\n", out_);
  }

  // Emit test constructor.
  void emitTestConstructor() {
    fputs("  private Test() {\n", out_);
    indentation_ += 2;
    emitIndentation();
    emitType(array_type_);
    fputs(" a = ", out_);
    emitLiteral(array_type_);
    fputs(";\n", out_);
    for (uint32_t i = 0; i < array_dim_; i++) {
      emitIndentation();
      fprintf(out_, "for (int i%u = 0; i%u < %u; i%u++) {\n", i, i, array_size_, i);
      indentation_ += 2;
    }
    emitIndentation();
    fputs("mArray", out_);
    for (uint32_t i = 0; i < array_dim_; i++) {
      fprintf(out_, "[i%u]", i);
    }
    fputs(" = a;\n", out_);
    emitIndentation();
    if (array_type_ == kBoolean) {
      fputs("a = !a;\n", out_);
    } else {
      fputs("a++;\n", out_);
    }
    for (uint32_t i = 0; i < array_dim_; i++) {
      indentation_ -= 2;
      emitIndentation();
      fputs("}\n", out_);
    }
    indentation_ -= 2;
    fputs("  }\n\n", out_);
  }

  // Emit test method.
  void emitTestMethod() {
    fputs("  private ", out_);
    emitType(return_type_);
    fputs(" testMethod() {\n", out_);
    indentation_ += 2;
    if (emitStatementList()) {
      // Must end with return.
      emitIndentation();
      emitReturn(true);
    }
    indentation_ -= 2;
    fputs("  }\n\n", out_);
  }

  // Emit main method driver.
  void emitMainMethod() {
    fputs("  public static void main(String[] args) {\n", out_);
    indentation_ += 2;
    fputs("    Test t = new Test();\n    ", out_);
    emitType(return_type_);
    fputs(" r = ", out_);
    emitLiteral(return_type_);
    fputs(";\n", out_);
    fputs("    try {\n", out_);
    fputs("      r = t.testMethod();\n", out_);
    fputs("    } catch (Exception e) {\n", out_);
    fputs("      // Arithmetic, null pointer, index out of bounds, etc.\n", out_);
    fputs("      System.out.println(\"An exception was caught.\");\n", out_);
    fputs("    }\n", out_);
    fputs("    System.out.println(\"r  = \" + r);\n",    out_);
    fputs("    System.out.println(\"mZ = \" + t.mZ);\n", out_);
    fputs("    System.out.println(\"mI = \" + t.mI);\n", out_);
    fputs("    System.out.println(\"mJ = \" + t.mJ);\n", out_);
    fputs("    System.out.println(\"mF = \" + t.mF);\n", out_);
    fputs("    System.out.println(\"mD = \" + t.mD);\n", out_);
    fputs("    System.out.println(\"mArray = \" + ", out_);
    if (array_dim_ == 1) {
      fputs("Arrays.toString(t.mArray)", out_);
    } else {
      fputs("Arrays.deepToString(t.mArray)", out_);
    }
    fputs(");\n", out_);
    indentation_ -= 2;
    fputs("  }\n", out_);
  }

  // Emit a static void method.
  void emitStaticNopMethod() {
    fputs("  public static void nop() {}\n\n", out_);
  }

  // Emit program header. Emit command line options in the comments.
  void emitHeader() {
    fputs("\n/**\n * AOSP JFuzz Tester.\n", out_);
    fputs(" * Automatically generated program.\n", out_);
    fprintf(out_,
            " * jfuzz -s %u -d %u -l %u -i %u -n %u (version %s)\n */\n\n",
            fuzz_seed_,
            fuzz_expr_depth_,
            fuzz_stmt_length_,
            fuzz_if_nest_,
            fuzz_loop_nest_,
            VERSION);
    fputs("import java.util.Arrays;\n\n", out_);
  }

  // Emit single test class with main driver.
  void emitTestClassWithMain() {
    fputs("public class Test {\n\n", out_);
    indentation_ += 2;
    emitClassDecls();
    emitFieldDecls();
    emitArrayDecl();
    emitTestConstructor();
    emitTestMethod();
    emitStaticNopMethod();
    emitMainMethod();
    indentation_ -= 2;
    fputs("}\n\n", out_);
  }

  //
  // Random integers.
  //

  // Return random integer.
  int32_t random() {
    return fuzz_random_engine_();
  }

  // Return random integer in range [0,max).
  uint32_t random0(uint32_t max) {
    std::uniform_int_distribution<uint32_t> gen(0, max - 1);
    return gen(fuzz_random_engine_);
  }

  // Return random integer in range [1,max].
  uint32_t random1(uint32_t max) {
    std::uniform_int_distribution<uint32_t> gen(1, max);
    return gen(fuzz_random_engine_);
  }

  // Fuzzing parameters.
  FILE* out_;
  std::mt19937 fuzz_random_engine_;
  const uint32_t fuzz_seed_;
  const uint32_t fuzz_expr_depth_;
  const uint32_t fuzz_stmt_length_;
  const uint32_t fuzz_if_nest_;
  const uint32_t fuzz_loop_nest_;
  const uint32_t fuzz_try_nest_;

  // Return and array setup.
  const Type return_type_;
  const Type array_type_;
  const uint32_t array_dim_;
  const uint32_t array_size_;

  // Current context.
  uint32_t indentation_;
  uint32_t expr_depth_;
  uint32_t stmt_length_;
  uint32_t if_nest_;
  uint32_t loop_nest_;
  uint32_t switch_nest_;
  uint32_t do_nest_;
  uint32_t try_nest_;
  uint32_t boolean_local_;
  uint32_t int_local_;
  uint32_t long_local_;
  uint32_t float_local_;
  uint32_t double_local_;
  bool in_inner_;
};

}  // anonymous namespace

int32_t main(int32_t argc, char** argv) {
  // Time-based seed.
  struct timeval tp;
  gettimeofday(&tp, NULL);

  // Defaults.
  uint32_t seed = (tp.tv_sec * 1000000 + tp.tv_usec);
  uint32_t expr_depth = 1;
  uint32_t stmt_length = 8;
  uint32_t if_nest = 2;
  uint32_t loop_nest = 3;
  uint32_t try_nest = 2;

  // Parse options.
  while (1) {
    int32_t option = getopt(argc, argv, "s:d:l:i:n:vh");
    if (option < 0) {
      break;  // done
    }
    switch (option) {
      case 's':
        seed = strtoul(optarg, nullptr, 0);  // deterministic seed
        break;
      case 'd':
        expr_depth = strtoul(optarg, nullptr, 0);
        break;
      case 'l':
        stmt_length = strtoul(optarg, nullptr, 0);
        break;
      case 'i':
        if_nest = strtoul(optarg, nullptr, 0);
        break;
      case 'n':
        loop_nest = strtoul(optarg, nullptr, 0);
        break;
      case 't':
        try_nest = strtoul(optarg, nullptr, 0);
        break;
      case 'v':
        fprintf(stderr, "jfuzz version %s\n", VERSION);
        return 0;
      case 'h':
      default:
        fprintf(stderr,
                "usage: %s [-s seed] "
                "[-d expr-depth] [-l stmt-length] "
                "[-i if-nest] [-n loop-nest] [-t try-nest] [-v] [-h]\n",
                argv[0]);
        return 1;
    }
  }

  // Seed global random generator.
  srand(seed);

  // Generate fuzzed program.
  JFuzz fuzz(stdout, seed, expr_depth, stmt_length, if_nest, loop_nest, try_nest);
  fuzz.emitProgram();
  return 0;
}
