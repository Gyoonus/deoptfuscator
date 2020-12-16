/*
 * Copyright 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <strstream>

#include <jni.h>

#include "base/utils.h"
#include "exec_utils.h"
#include "jvmti.h"

#pragma clang diagnostic push

// Slicer's headers have code that triggers these warnings. b/65298177
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wsign-compare"

#include "slicer/code_ir.h"
#include "slicer/control_flow_graph.h"
#include "slicer/dex_ir.h"
#include "slicer/dex_ir_builder.h"
#include "slicer/instrumentation.h"
#include "slicer/reader.h"
#include "slicer/writer.h"

#pragma clang diagnostic pop

namespace art {

// Should we do a 'full_rewrite' with this test?
static constexpr bool kDoFullRewrite = true;

struct StressData {
  bool vm_class_loader_initialized;
  bool trace_stress;
  bool redefine_stress;
  bool field_stress;
  bool step_stress;
};

static bool DoExtractClassFromData(jvmtiEnv* env,
                                   const std::string& descriptor,
                                   jint in_len,
                                   const unsigned char* in_data,
                                   /*out*/jint* out_len,
                                   /*out*/unsigned char** out_data) {
  dex::Reader reader(in_data, in_len);
  dex::u4 class_idx = reader.FindClassIndex(descriptor.c_str());
  if (class_idx != dex::kNoIndex) {
    reader.CreateClassIr(class_idx);
  } else {
    LOG(ERROR) << "ERROR: Can't find class " << descriptor;
    return false;
  }
  auto dex_ir = reader.GetIr();

  if (kDoFullRewrite) {
    for (auto& ir_method : dex_ir->encoded_methods) {
      if (ir_method->code != nullptr) {
        lir::CodeIr code_ir(ir_method.get(), dex_ir);
        lir::ControlFlowGraph cfg_compact(&code_ir, false);
        lir::ControlFlowGraph cfg_verbose(&code_ir, true);
        code_ir.Assemble();
      }
    }
  }
  dex::Writer writer(dex_ir);

  struct Allocator : public dex::Writer::Allocator {
    explicit Allocator(jvmtiEnv* jvmti_env) : jvmti_env_(jvmti_env) {}
    virtual void* Allocate(size_t size) {
      unsigned char* out = nullptr;
      if (JVMTI_ERROR_NONE != jvmti_env_->Allocate(size, &out)) {
        return nullptr;
      } else {
        return out;
      }
    }
    virtual void Free(void* ptr) {
      jvmti_env_->Deallocate(reinterpret_cast<unsigned char*>(ptr));
    }
   private:
    jvmtiEnv* jvmti_env_;
  };
  Allocator alloc(env);
  size_t res_len;
  unsigned char* res = writer.CreateImage(&alloc, &res_len);
  if (res != nullptr) {
    *out_data = res;
    *out_len = res_len;
    return true;
  } else {
    return false;
  }
}

class ScopedThreadInfo {
 public:
  ScopedThreadInfo(jvmtiEnv* jvmtienv, JNIEnv* env, jthread thread)
      : jvmtienv_(jvmtienv), env_(env), free_name_(false) {
    memset(&info_, 0, sizeof(info_));
    if (thread == nullptr) {
      info_.name = const_cast<char*>("<NULLPTR>");
    } else if (jvmtienv->GetThreadInfo(thread, &info_) != JVMTI_ERROR_NONE) {
      info_.name = const_cast<char*>("<UNKNOWN THREAD>");
    } else {
      free_name_ = true;
    }
  }

  ~ScopedThreadInfo() {
    if (free_name_) {
      jvmtienv_->Deallocate(reinterpret_cast<unsigned char*>(info_.name));
    }
    env_->DeleteLocalRef(info_.thread_group);
    env_->DeleteLocalRef(info_.context_class_loader);
  }

  const char* GetName() const {
    return info_.name;
  }

 private:
  jvmtiEnv* jvmtienv_;
  JNIEnv* env_;
  bool free_name_;
  jvmtiThreadInfo info_;
};

class ScopedClassInfo {
 public:
  ScopedClassInfo(jvmtiEnv* jvmtienv, jclass c)
      : jvmtienv_(jvmtienv),
        class_(c),
        name_(nullptr),
        generic_(nullptr),
        file_(nullptr),
        debug_ext_(nullptr) {}

  ~ScopedClassInfo() {
    if (class_ != nullptr) {
      jvmtienv_->Deallocate(reinterpret_cast<unsigned char*>(name_));
      jvmtienv_->Deallocate(reinterpret_cast<unsigned char*>(generic_));
      jvmtienv_->Deallocate(reinterpret_cast<unsigned char*>(file_));
      jvmtienv_->Deallocate(reinterpret_cast<unsigned char*>(debug_ext_));
    }
  }

  bool Init() {
    if (class_ == nullptr) {
      name_ = const_cast<char*>("<NONE>");
      generic_ = const_cast<char*>("<NONE>");
      return true;
    } else {
      jvmtiError ret1 = jvmtienv_->GetSourceFileName(class_, &file_);
      jvmtiError ret2 = jvmtienv_->GetSourceDebugExtension(class_, &debug_ext_);
      return jvmtienv_->GetClassSignature(class_, &name_, &generic_) == JVMTI_ERROR_NONE &&
          ret1 != JVMTI_ERROR_MUST_POSSESS_CAPABILITY &&
          ret1 != JVMTI_ERROR_INVALID_CLASS &&
          ret2 != JVMTI_ERROR_MUST_POSSESS_CAPABILITY &&
          ret2 != JVMTI_ERROR_INVALID_CLASS;
    }
  }

  jclass GetClass() const {
    return class_;
  }
  const char* GetName() const {
    return name_;
  }
  const char* GetGeneric() const {
    return generic_;
  }
  const char* GetSourceDebugExtension() const {
    if (debug_ext_ == nullptr) {
      return "<UNKNOWN_SOURCE_DEBUG_EXTENSION>";
    } else {
      return debug_ext_;
    }
  }
  const char* GetSourceFileName() const {
    if (file_ == nullptr) {
      return "<UNKNOWN_FILE>";
    } else {
      return file_;
    }
  }

 private:
  jvmtiEnv* jvmtienv_;
  jclass class_;
  char* name_;
  char* generic_;
  char* file_;
  char* debug_ext_;
};

class ScopedMethodInfo {
 public:
  ScopedMethodInfo(jvmtiEnv* jvmtienv, JNIEnv* env, jmethodID m)
      : jvmtienv_(jvmtienv),
        env_(env),
        method_(m),
        declaring_class_(nullptr),
        class_info_(nullptr),
        name_(nullptr),
        signature_(nullptr),
        generic_(nullptr),
        first_line_(-1) {}

  ~ScopedMethodInfo() {
    env_->DeleteLocalRef(declaring_class_);
    jvmtienv_->Deallocate(reinterpret_cast<unsigned char*>(name_));
    jvmtienv_->Deallocate(reinterpret_cast<unsigned char*>(signature_));
    jvmtienv_->Deallocate(reinterpret_cast<unsigned char*>(generic_));
  }

  bool Init() {
    if (jvmtienv_->GetMethodDeclaringClass(method_, &declaring_class_) != JVMTI_ERROR_NONE) {
      return false;
    }
    class_info_.reset(new ScopedClassInfo(jvmtienv_, declaring_class_));
    jint nlines;
    jvmtiLineNumberEntry* lines;
    jvmtiError err = jvmtienv_->GetLineNumberTable(method_, &nlines, &lines);
    if (err == JVMTI_ERROR_NONE) {
      if (nlines > 0) {
        first_line_ = lines[0].line_number;
      }
      jvmtienv_->Deallocate(reinterpret_cast<unsigned char*>(lines));
    } else if (err != JVMTI_ERROR_ABSENT_INFORMATION &&
               err != JVMTI_ERROR_NATIVE_METHOD) {
      return false;
    }
    return class_info_->Init() &&
        (jvmtienv_->GetMethodName(method_, &name_, &signature_, &generic_) == JVMTI_ERROR_NONE);
  }

  const ScopedClassInfo& GetDeclaringClassInfo() const {
    return *class_info_;
  }

  jclass GetDeclaringClass() const {
    return declaring_class_;
  }

  const char* GetName() const {
    return name_;
  }

  const char* GetSignature() const {
    return signature_;
  }

  const char* GetGeneric() const {
    return generic_;
  }

  jint GetFirstLine() const {
    return first_line_;
  }

 private:
  jvmtiEnv* jvmtienv_;
  JNIEnv* env_;
  jmethodID method_;
  jclass declaring_class_;
  std::unique_ptr<ScopedClassInfo> class_info_;
  char* name_;
  char* signature_;
  char* generic_;
  jint first_line_;

  friend std::ostream& operator<<(std::ostream &os, ScopedMethodInfo const& m);
};

class ScopedFieldInfo {
 public:
  ScopedFieldInfo(jvmtiEnv* jvmtienv, jclass field_klass, jfieldID field)
      : jvmtienv_(jvmtienv),
        declaring_class_(field_klass),
        field_(field),
        class_info_(nullptr),
        name_(nullptr),
        type_(nullptr),
        generic_(nullptr) {}

  ~ScopedFieldInfo() {
    jvmtienv_->Deallocate(reinterpret_cast<unsigned char*>(name_));
    jvmtienv_->Deallocate(reinterpret_cast<unsigned char*>(type_));
    jvmtienv_->Deallocate(reinterpret_cast<unsigned char*>(generic_));
  }

  bool Init() {
    class_info_.reset(new ScopedClassInfo(jvmtienv_, declaring_class_));
    return class_info_->Init() &&
        (jvmtienv_->GetFieldName(
            declaring_class_, field_, &name_, &type_, &generic_) == JVMTI_ERROR_NONE);
  }

  const ScopedClassInfo& GetDeclaringClassInfo() const {
    return *class_info_;
  }

  jclass GetDeclaringClass() const {
    return declaring_class_;
  }

  const char* GetName() const {
    return name_;
  }

  const char* GetType() const {
    return type_;
  }

  const char* GetGeneric() const {
    return generic_;
  }

 private:
  jvmtiEnv* jvmtienv_;
  jclass declaring_class_;
  jfieldID field_;
  std::unique_ptr<ScopedClassInfo> class_info_;
  char* name_;
  char* type_;
  char* generic_;

  friend std::ostream& operator<<(std::ostream &os, ScopedFieldInfo const& m);
};

std::ostream& operator<<(std::ostream &os, const ScopedFieldInfo* m) {
  return os << *m;
}

std::ostream& operator<<(std::ostream &os, ScopedFieldInfo const& m) {
  return os << m.GetDeclaringClassInfo().GetName() << "->" << m.GetName()
            << ":" << m.GetType();
}

std::ostream& operator<<(std::ostream &os, const ScopedMethodInfo* m) {
  return os << *m;
}

std::ostream& operator<<(std::ostream &os, ScopedMethodInfo const& m) {
  return os << m.GetDeclaringClassInfo().GetName() << "->" << m.GetName() << m.GetSignature()
            << " (source: " << m.GetDeclaringClassInfo().GetSourceFileName() << ":"
            << m.GetFirstLine() << ")";
}

static void doJvmtiMethodBind(jvmtiEnv* jvmtienv,
                              JNIEnv* env,
                              jthread thread,
                              jmethodID m,
                              void* address,
                              /*out*/void** out_address) {
  *out_address = address;
  ScopedThreadInfo thread_info(jvmtienv, env, thread);
  ScopedMethodInfo method_info(jvmtienv, env, m);
  if (!method_info.Init()) {
    LOG(ERROR) << "Unable to get method info!";
    return;
  }
  LOG(INFO) << "Loading native method \"" << method_info << "\". Thread is "
            << thread_info.GetName();
}

static std::string GetName(jvmtiEnv* jvmtienv, JNIEnv* jnienv, jobject obj) {
  jclass klass = jnienv->GetObjectClass(obj);
  char *cname, *cgen;
  if (jvmtienv->GetClassSignature(klass, &cname, &cgen) != JVMTI_ERROR_NONE) {
    LOG(ERROR) << "Unable to get class name!";
    jnienv->DeleteLocalRef(klass);
    return "<UNKNOWN>";
  }
  std::string name(cname);
  if (name == "Ljava/lang/String;") {
    jstring str = reinterpret_cast<jstring>(obj);
    const char* val = jnienv->GetStringUTFChars(str, nullptr);
    if (val == nullptr) {
      name += " (unable to get value)";
    } else {
      std::ostringstream oss;
      oss << name << " (value: \"" << val << "\")";
      name = oss.str();
      jnienv->ReleaseStringUTFChars(str, val);
    }
  }
  jvmtienv->Deallocate(reinterpret_cast<unsigned char*>(cname));
  jvmtienv->Deallocate(reinterpret_cast<unsigned char*>(cgen));
  jnienv->DeleteLocalRef(klass);
  return name;
}

static std::string GetValOf(jvmtiEnv* env, JNIEnv* jnienv, std::string type, jvalue val) {
  std::ostringstream oss;
  switch (type[0]) {
    case '[':
    case 'L':
      return val.l != nullptr ? GetName(env, jnienv, val.l) : "null";
    case 'Z':
      return val.z == JNI_TRUE ? "true" : "false";
    case 'B':
      oss << val.b;
      return oss.str();
    case 'C':
      oss << val.c;
      return oss.str();
    case 'S':
      oss << val.s;
      return oss.str();
    case 'I':
      oss << val.i;
      return oss.str();
    case 'J':
      oss << val.j;
      return oss.str();
    case 'F':
      oss << val.f;
      return oss.str();
    case 'D':
      oss << val.d;
      return oss.str();
    case 'V':
      return "<void>";
    default:
      return "<ERROR Found type " + type + ">";
  }
}

void JNICALL FieldAccessHook(jvmtiEnv* jvmtienv,
                             JNIEnv* env,
                             jthread thread,
                             jmethodID m,
                             jlocation location,
                             jclass field_klass,
                             jobject object,
                             jfieldID field) {
  ScopedThreadInfo info(jvmtienv, env, thread);
  ScopedMethodInfo method_info(jvmtienv, env, m);
  ScopedFieldInfo field_info(jvmtienv, field_klass, field);
  jclass oklass = (object != nullptr) ? env->GetObjectClass(object) : nullptr;
  ScopedClassInfo obj_class_info(jvmtienv, oklass);
  if (!method_info.Init() || !field_info.Init() || !obj_class_info.Init()) {
    LOG(ERROR) << "Unable to get callback info!";
    return;
  }
  LOG(INFO) << "ACCESS field \"" << field_info << "\" on object of "
            << "type \"" << obj_class_info.GetName() << "\" in method \"" << method_info
            << "\" at location 0x" << std::hex << location << ". Thread is \""
            << info.GetName() << "\".";
  env->DeleteLocalRef(oklass);
}

static std::string PrintJValue(jvmtiEnv* jvmtienv, JNIEnv* env, char type, jvalue new_value) {
  std::ostringstream oss;
  switch (type) {
    case 'L': {
      jobject nv = new_value.l;
      if (nv == nullptr) {
        oss << "\"null\"";
      } else {
        jclass nv_klass = env->GetObjectClass(nv);
        ScopedClassInfo nv_class_info(jvmtienv, nv_klass);
        if (!nv_class_info.Init()) {
          oss << "with unknown type";
        } else {
          oss << "of type \"" << nv_class_info.GetName() << "\"";
        }
        env->DeleteLocalRef(nv_klass);
      }
      break;
    }
    case 'Z': {
      if (new_value.z) {
        oss << "true";
      } else {
        oss << "false";
      }
      break;
    }
#define SEND_VALUE(chr, sym, type) \
    case chr: { \
      oss << static_cast<type>(new_value.sym); \
      break; \
    }
    SEND_VALUE('B', b, int8_t);
    SEND_VALUE('C', c, uint16_t);
    SEND_VALUE('S', s, int16_t);
    SEND_VALUE('I', i, int32_t);
    SEND_VALUE('J', j, int64_t);
    SEND_VALUE('F', f, float);
    SEND_VALUE('D', d, double);
#undef SEND_VALUE
  }
  return oss.str();
}

void JNICALL FieldModificationHook(jvmtiEnv* jvmtienv,
                                   JNIEnv* env,
                                   jthread thread,
                                   jmethodID m,
                                   jlocation location,
                                   jclass field_klass,
                                   jobject object,
                                   jfieldID field,
                                   char type,
                                   jvalue new_value) {
  ScopedThreadInfo info(jvmtienv, env, thread);
  ScopedMethodInfo method_info(jvmtienv, env, m);
  ScopedFieldInfo field_info(jvmtienv, field_klass, field);
  jclass oklass = (object != nullptr) ? env->GetObjectClass(object) : nullptr;
  ScopedClassInfo obj_class_info(jvmtienv, oklass);
  if (!method_info.Init() || !field_info.Init() || !obj_class_info.Init()) {
    LOG(ERROR) << "Unable to get callback info!";
    return;
  }
  LOG(INFO) << "MODIFY field \"" << field_info << "\" on object of "
            << "type \"" << obj_class_info.GetName() << "\" in method \"" << method_info
            << "\" at location 0x" << std::hex << location << std::dec << ". New value is "
            << PrintJValue(jvmtienv, env, type, new_value) << ". Thread is \""
            << info.GetName() << "\".";
  env->DeleteLocalRef(oklass);
}
void JNICALL MethodExitHook(jvmtiEnv* jvmtienv,
                            JNIEnv* env,
                            jthread thread,
                            jmethodID m,
                            jboolean was_popped_by_exception,
                            jvalue val) {
  ScopedThreadInfo info(jvmtienv, env, thread);
  ScopedMethodInfo method_info(jvmtienv, env, m);
  if (!method_info.Init()) {
    LOG(ERROR) << "Unable to get method info!";
    return;
  }
  std::string type(method_info.GetSignature());
  type = type.substr(type.find(')') + 1);
  std::string out_val(was_popped_by_exception ? "" : GetValOf(jvmtienv, env, type, val));
  LOG(INFO) << "Leaving method \"" << method_info << "\". Thread is \"" << info.GetName() << "\"."
            << std::endl
            << "    Cause: " << (was_popped_by_exception ? "exception" : "return ")
            << out_val << ".";
}

void JNICALL MethodEntryHook(jvmtiEnv* jvmtienv,
                             JNIEnv* env,
                             jthread thread,
                             jmethodID m) {
  ScopedThreadInfo info(jvmtienv, env, thread);
  ScopedMethodInfo method_info(jvmtienv, env, m);
  if (!method_info.Init()) {
    LOG(ERROR) << "Unable to get method info!";
    return;
  }
  LOG(INFO) << "Entering method \"" << method_info << "\". Thread is \"" << info.GetName() << "\"";
}

void JNICALL ClassPrepareHook(jvmtiEnv* jvmtienv,
                              JNIEnv* env,
                              jthread thread,
                              jclass klass) {
  StressData* data = nullptr;
  CHECK_EQ(jvmtienv->GetEnvironmentLocalStorage(reinterpret_cast<void**>(&data)),
           JVMTI_ERROR_NONE);
  if (data->field_stress) {
    jint nfields;
    jfieldID* fields;
    if (jvmtienv->GetClassFields(klass, &nfields, &fields) != JVMTI_ERROR_NONE) {
      LOG(ERROR) << "Unable to get a classes fields!";
      return;
    }
    for (jint i = 0; i < nfields; i++) {
      jfieldID f = fields[i];
      // Ignore errors
      jvmtienv->SetFieldAccessWatch(klass, f);
      jvmtienv->SetFieldModificationWatch(klass, f);
    }
    jvmtienv->Deallocate(reinterpret_cast<unsigned char*>(fields));
  }
  if (data->trace_stress) {
    ScopedThreadInfo info(jvmtienv, env, thread);
    ScopedClassInfo class_info(jvmtienv, klass);
    if (!class_info.Init()) {
      LOG(ERROR) << "Unable to get class info!";
      return;
    }
    LOG(INFO) << "Prepared class \"" << class_info.GetName() << "\". Thread is \""
              << info.GetName() << "\"";
  }
}

void JNICALL SingleStepHook(jvmtiEnv* jvmtienv,
                            JNIEnv* env,
                            jthread thread,
                            jmethodID method,
                            jlocation location) {
  ScopedThreadInfo info(jvmtienv, env, thread);
  ScopedMethodInfo method_info(jvmtienv, env, method);
  if (!method_info.Init()) {
    LOG(ERROR) << "Unable to get method info!";
    return;
  }
  LOG(INFO) << "Single step at location: 0x" << std::setw(8) << std::setfill('0') << std::hex
            << location << " in method " << method_info << " thread: " << info.GetName();
}

// The hook we are using.
void JNICALL ClassFileLoadHookSecretNoOp(jvmtiEnv* jvmti,
                                         JNIEnv* jni_env ATTRIBUTE_UNUSED,
                                         jclass class_being_redefined ATTRIBUTE_UNUSED,
                                         jobject loader ATTRIBUTE_UNUSED,
                                         const char* name,
                                         jobject protection_domain ATTRIBUTE_UNUSED,
                                         jint class_data_len,
                                         const unsigned char* class_data,
                                         jint* new_class_data_len,
                                         unsigned char** new_class_data) {
  std::vector<unsigned char> out;
  // Make the jvmti semi-descriptor into the full descriptor.
  std::string name_str("L");
  name_str += name;
  name_str += ";";
  StressData* data = nullptr;
  CHECK_EQ(jvmti->GetEnvironmentLocalStorage(reinterpret_cast<void**>(&data)),
           JVMTI_ERROR_NONE);
  if (!data->vm_class_loader_initialized) {
    LOG(WARNING) << "Ignoring load of class " << name << " because VMClassLoader is not yet "
                 << "initialized. Transforming this class could cause spurious test failures.";
    return;
  } else if (DoExtractClassFromData(jvmti, name_str, class_data_len, class_data,
                                    /*out*/ new_class_data_len, /*out*/ new_class_data)) {
    LOG(INFO) << "Extracted class: " << name;
  } else {
    std::cerr << "Unable to extract class " << name << std::endl;
    *new_class_data_len = 0;
    *new_class_data = nullptr;
  }
}

static std::string AdvanceOption(const std::string& ops) {
  return ops.substr(ops.find(',') + 1);
}

static bool HasNextOption(const std::string& ops) {
  return ops.find(',') != std::string::npos;
}

static std::string GetOption(const std::string& in) {
  return in.substr(0, in.find(','));
}

// Options are
// jvmti-stress,[redefine,][trace,][field]
static void ReadOptions(StressData* data, char* options) {
  std::string ops(options);
  CHECK_EQ(GetOption(ops), "jvmti-stress") << "Options should start with jvmti-stress";
  do {
    ops = AdvanceOption(ops);
    std::string cur = GetOption(ops);
    if (cur == "trace") {
      data->trace_stress = true;
    } else if (cur == "step") {
      data->step_stress = true;
    } else if (cur == "field") {
      data->field_stress = true;
    } else if (cur == "redefine") {
      data->redefine_stress = true;
    } else {
      LOG(FATAL) << "Unknown option: " << GetOption(ops);
    }
  } while (HasNextOption(ops));
}

// Do final setup during the VMInit callback. By this time most things are all setup.
static void JNICALL PerformFinalSetupVMInit(jvmtiEnv *jvmti_env,
                                            JNIEnv* jni_env,
                                            jthread thread ATTRIBUTE_UNUSED) {
  // Load the VMClassLoader class. We will get a ClassNotFound exception because we don't have
  // visibility but the class will be loaded behind the scenes.
  LOG(INFO) << "manual load & initialization of class java/lang/VMClassLoader!";
  jclass klass = jni_env->FindClass("java/lang/VMClassLoader");
  StressData* data = nullptr;
  CHECK_EQ(jvmti_env->GetEnvironmentLocalStorage(reinterpret_cast<void**>(&data)),
           JVMTI_ERROR_NONE);
  // We need to make sure that VMClassLoader is initialized before we start redefining anything
  // since it can give (non-fatal) error messages if it's initialized after we've redefined BCP
  // classes. These error messages are expected and no problem but they will mess up our testing
  // infrastructure.
  if (klass == nullptr) {
    // Probably on RI. Clear the exception so we can continue but don't mark vmclassloader as
    // initialized.
    LOG(WARNING) << "Unable to find VMClassLoader class!";
    jni_env->ExceptionClear();
  } else {
    // GetMethodID is spec'd to cause the class to be initialized.
    jni_env->GetMethodID(klass, "hashCode", "()I");
    jni_env->DeleteLocalRef(klass);
    data->vm_class_loader_initialized = true;
  }
}

static bool WatchAllFields(JavaVM* vm, jvmtiEnv* jvmti) {
  if (jvmti->SetEventNotificationMode(JVMTI_ENABLE,
                                      JVMTI_EVENT_CLASS_PREPARE,
                                      nullptr) != JVMTI_ERROR_NONE) {
    LOG(ERROR) << "Couldn't set prepare event!";
    return false;
  }
  // TODO We really shouldn't need to do this step here.
  jint nklass;
  jclass* klasses;
  if (jvmti->GetLoadedClasses(&nklass, &klasses) != JVMTI_ERROR_NONE) {
    LOG(WARNING) << "Couldn't get loaded classes! Ignoring.";
    return true;
  }
  JNIEnv* jni = nullptr;
  if (vm->GetEnv(reinterpret_cast<void**>(&jni), JNI_VERSION_1_6)) {
    LOG(ERROR) << "Unable to get jni env. Ignoring and potentially leaking jobjects.";
    return false;
  }
  for (jint i = 0; i < nklass; i++) {
    jclass k = klasses[i];
    ScopedClassInfo sci(jvmti, k);
    if (sci.Init()) {
      LOG(INFO) << "NOTE: class " << sci.GetName() << " already loaded.";
    }
    jint nfields;
    jfieldID* fields;
    jvmtiError err = jvmti->GetClassFields(k, &nfields, &fields);
    if (err == JVMTI_ERROR_NONE) {
      for (jint j = 0; j < nfields; j++) {
        jfieldID f = fields[j];
        if (jvmti->SetFieldModificationWatch(k, f) != JVMTI_ERROR_NONE ||
            jvmti->SetFieldAccessWatch(k, f) != JVMTI_ERROR_NONE) {
          LOG(ERROR) << "Unable to set watches on a field.";
          return false;
        }
      }
    } else if (err != JVMTI_ERROR_CLASS_NOT_PREPARED) {
      LOG(ERROR) << "Unexpected error getting class fields!";
      return false;
    }
    jvmti->Deallocate(reinterpret_cast<unsigned char*>(fields));
    jni->DeleteLocalRef(k);
  }
  jvmti->Deallocate(reinterpret_cast<unsigned char*>(klasses));
  return true;
}

extern "C" JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM* vm,
                                               char* options,
                                               void* reserved ATTRIBUTE_UNUSED) {
  jvmtiEnv* jvmti = nullptr;
  if (vm->GetEnv(reinterpret_cast<void**>(&jvmti), JVMTI_VERSION_1_0)) {
    LOG(ERROR) << "Unable to get jvmti env.";
    return 1;
  }
  StressData* data = nullptr;
  if (JVMTI_ERROR_NONE != jvmti->Allocate(sizeof(StressData),
                                          reinterpret_cast<unsigned char**>(&data))) {
    LOG(ERROR) << "Unable to allocate data for stress test.";
    return 1;
  }
  memset(data, 0, sizeof(StressData));
  // Read the options into the static variables that hold them.
  ReadOptions(data, options);
  // Save the data
  if (JVMTI_ERROR_NONE != jvmti->SetEnvironmentLocalStorage(data)) {
    LOG(ERROR) << "Unable to save stress test data.";
    return 1;
  }

  // Just get all capabilities.
  jvmtiCapabilities caps = {
    .can_tag_objects                                 = 0,
    .can_generate_field_modification_events          = 1,
    .can_generate_field_access_events                = 1,
    .can_get_bytecodes                               = 0,
    .can_get_synthetic_attribute                     = 0,
    .can_get_owned_monitor_info                      = 0,
    .can_get_current_contended_monitor               = 0,
    .can_get_monitor_info                            = 0,
    .can_pop_frame                                   = 0,
    .can_redefine_classes                            = 1,
    .can_signal_thread                               = 0,
    .can_get_source_file_name                        = 1,
    .can_get_line_numbers                            = 1,
    .can_get_source_debug_extension                  = 1,
    .can_access_local_variables                      = 0,
    .can_maintain_original_method_order              = 0,
    .can_generate_single_step_events                 = 1,
    .can_generate_exception_events                   = 0,
    .can_generate_frame_pop_events                   = 0,
    .can_generate_breakpoint_events                  = 0,
    .can_suspend                                     = 0,
    .can_redefine_any_class                          = 0,
    .can_get_current_thread_cpu_time                 = 0,
    .can_get_thread_cpu_time                         = 0,
    .can_generate_method_entry_events                = 1,
    .can_generate_method_exit_events                 = 1,
    .can_generate_all_class_hook_events              = 0,
    .can_generate_compiled_method_load_events        = 0,
    .can_generate_monitor_events                     = 0,
    .can_generate_vm_object_alloc_events             = 0,
    .can_generate_native_method_bind_events          = 1,
    .can_generate_garbage_collection_events          = 0,
    .can_generate_object_free_events                 = 0,
    .can_force_early_return                          = 0,
    .can_get_owned_monitor_stack_depth_info          = 0,
    .can_get_constant_pool                           = 0,
    .can_set_native_method_prefix                    = 0,
    .can_retransform_classes                         = 1,
    .can_retransform_any_class                       = 0,
    .can_generate_resource_exhaustion_heap_events    = 0,
    .can_generate_resource_exhaustion_threads_events = 0,
  };
  jvmti->AddCapabilities(&caps);

  // Set callbacks.
  jvmtiEventCallbacks cb;
  memset(&cb, 0, sizeof(cb));
  cb.ClassFileLoadHook = ClassFileLoadHookSecretNoOp;
  cb.NativeMethodBind = doJvmtiMethodBind;
  cb.VMInit = PerformFinalSetupVMInit;
  cb.MethodEntry = MethodEntryHook;
  cb.MethodExit = MethodExitHook;
  cb.FieldAccess = FieldAccessHook;
  cb.FieldModification = FieldModificationHook;
  cb.ClassPrepare = ClassPrepareHook;
  cb.SingleStep = SingleStepHook;
  if (jvmti->SetEventCallbacks(&cb, sizeof(cb)) != JVMTI_ERROR_NONE) {
    LOG(ERROR) << "Unable to set class file load hook cb!";
    return 1;
  }
  if (jvmti->SetEventNotificationMode(JVMTI_ENABLE,
                                      JVMTI_EVENT_VM_INIT,
                                      nullptr) != JVMTI_ERROR_NONE) {
    LOG(ERROR) << "Unable to enable JVMTI_EVENT_VM_INIT event!";
    return 1;
  }
  if (data->redefine_stress) {
    if (jvmti->SetEventNotificationMode(JVMTI_ENABLE,
                                        JVMTI_EVENT_CLASS_FILE_LOAD_HOOK,
                                        nullptr) != JVMTI_ERROR_NONE) {
      LOG(ERROR) << "Unable to enable CLASS_FILE_LOAD_HOOK event!";
      return 1;
    }
  }
  if (data->trace_stress) {
    if (jvmti->SetEventNotificationMode(JVMTI_ENABLE,
                                        JVMTI_EVENT_CLASS_PREPARE,
                                        nullptr) != JVMTI_ERROR_NONE) {
      LOG(ERROR) << "Unable to enable CLASS_PREPARE event!";
      return 1;
    }
    if (jvmti->SetEventNotificationMode(JVMTI_ENABLE,
                                        JVMTI_EVENT_NATIVE_METHOD_BIND,
                                        nullptr) != JVMTI_ERROR_NONE) {
      LOG(ERROR) << "Unable to enable JVMTI_EVENT_NATIVE_METHOD_BIND event!";
      return 1;
    }
    if (jvmti->SetEventNotificationMode(JVMTI_ENABLE,
                                        JVMTI_EVENT_METHOD_ENTRY,
                                        nullptr) != JVMTI_ERROR_NONE) {
      LOG(ERROR) << "Unable to enable JVMTI_EVENT_METHOD_ENTRY event!";
      return 1;
    }
    if (jvmti->SetEventNotificationMode(JVMTI_ENABLE,
                                        JVMTI_EVENT_METHOD_EXIT,
                                        nullptr) != JVMTI_ERROR_NONE) {
      LOG(ERROR) << "Unable to enable JVMTI_EVENT_METHOD_EXIT event!";
      return 1;
    }
  }
  if (data->field_stress) {
    if (jvmti->SetEventNotificationMode(JVMTI_ENABLE,
                                        JVMTI_EVENT_FIELD_MODIFICATION,
                                        nullptr) != JVMTI_ERROR_NONE) {
      LOG(ERROR) << "Unable to enable FIELD_MODIFICATION event!";
      return 1;
    }
    if (jvmti->SetEventNotificationMode(JVMTI_ENABLE,
                                        JVMTI_EVENT_FIELD_ACCESS,
                                        nullptr) != JVMTI_ERROR_NONE) {
      LOG(ERROR) << "Unable to enable FIELD_ACCESS event!";
      return 1;
    }
    if (!WatchAllFields(vm, jvmti)) {
      return 1;
    }
  }
  if (data->step_stress) {
    if (jvmti->SetEventNotificationMode(JVMTI_ENABLE,
                                        JVMTI_EVENT_SINGLE_STEP,
                                        nullptr) != JVMTI_ERROR_NONE) {
      return 1;
    }
  }
  return 0;
}

}  // namespace art
