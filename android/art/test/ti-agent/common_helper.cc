/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include "common_helper.h"

#include <sstream>
#include <string>

#include "android-base/stringprintf.h"
#include "jni.h"
#include "jvmti.h"

#include "jvmti_helper.h"

namespace art {

jobject GetJavaField(jvmtiEnv* jvmti, JNIEnv* env, jclass field_klass, jfieldID f) {
  jint mods = 0;
  if (JvmtiErrorToException(env, jvmti, jvmti->GetFieldModifiers(field_klass, f, &mods))) {
    return nullptr;
  }

  bool is_static = (mods & kAccStatic) != 0;
  return env->ToReflectedField(field_klass, f, is_static);
}

jobject GetJavaMethod(jvmtiEnv* jvmti, JNIEnv* env, jmethodID m) {
  jint mods = 0;
  if (JvmtiErrorToException(env, jvmti, jvmti->GetMethodModifiers(m, &mods))) {
    return nullptr;
  }

  bool is_static = (mods & kAccStatic) != 0;
  jclass method_klass = nullptr;
  if (JvmtiErrorToException(env, jvmti, jvmti->GetMethodDeclaringClass(m, &method_klass))) {
    return nullptr;
  }
  jobject res = env->ToReflectedMethod(method_klass, m, is_static);
  env->DeleteLocalRef(method_klass);
  return res;
}

jobject GetJavaValueByType(JNIEnv* env, char type, jvalue value) {
  std::string name;
  switch (type) {
    case 'V':
      return nullptr;
    case '[':
    case 'L':
      return value.l;
    case 'Z':
      name = "java/lang/Boolean";
      break;
    case 'B':
      name = "java/lang/Byte";
      break;
    case 'C':
      name = "java/lang/Character";
      break;
    case 'S':
      name = "java/lang/Short";
      break;
    case 'I':
      name = "java/lang/Integer";
      break;
    case 'J':
      name = "java/lang/Long";
      break;
    case 'F':
      name = "java/lang/Float";
      break;
    case 'D':
      name = "java/lang/Double";
      break;
    default:
      LOG(FATAL) << "Unable to figure out type!";
      return nullptr;
  }
  std::ostringstream oss;
  oss << "(" << type << ")L" << name << ";";
  std::string args = oss.str();
  jclass target = env->FindClass(name.c_str());
  jmethodID valueOfMethod = env->GetStaticMethodID(target, "valueOf", args.c_str());

  CHECK(valueOfMethod != nullptr) << args;
  jobject res = env->CallStaticObjectMethodA(target, valueOfMethod, &value);
  env->DeleteLocalRef(target);
  return res;
}

jobject GetJavaValue(jvmtiEnv* jvmtienv, JNIEnv* env, jmethodID m, jvalue value) {
  char *fname, *fsig, *fgen;
  if (JvmtiErrorToException(env, jvmtienv, jvmtienv->GetMethodName(m, &fname, &fsig, &fgen))) {
    return nullptr;
  }
  std::string type(fsig);
  type = type.substr(type.find(')') + 1);
  jvmtienv->Deallocate(reinterpret_cast<unsigned char*>(fsig));
  jvmtienv->Deallocate(reinterpret_cast<unsigned char*>(fname));
  jvmtienv->Deallocate(reinterpret_cast<unsigned char*>(fgen));
  return GetJavaValueByType(env, type[0], value);
}

}  // namespace art
