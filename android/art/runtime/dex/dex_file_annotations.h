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

#ifndef ART_RUNTIME_DEX_DEX_FILE_ANNOTATIONS_H_
#define ART_RUNTIME_DEX_DEX_FILE_ANNOTATIONS_H_

#include "dex/dex_file.h"

#include "handle.h"
#include "mirror/dex_cache.h"
#include "mirror/object_array.h"

namespace art {

namespace mirror {
class ClassLoader;
}  // namespace mirror
class ArtField;
class ArtMethod;
class ClassLinker;

namespace annotations {

// Field annotations.
mirror::Object* GetAnnotationForField(ArtField* field, Handle<mirror::Class> annotation_class)
    REQUIRES_SHARED(Locks::mutator_lock_);
mirror::ObjectArray<mirror::Object>* GetAnnotationsForField(ArtField* field)
    REQUIRES_SHARED(Locks::mutator_lock_);
mirror::ObjectArray<mirror::String>* GetSignatureAnnotationForField(ArtField* field)
    REQUIRES_SHARED(Locks::mutator_lock_);
bool IsFieldAnnotationPresent(ArtField* field, Handle<mirror::Class> annotation_class)
    REQUIRES_SHARED(Locks::mutator_lock_);

// Method annotations.
mirror::Object* GetAnnotationDefaultValue(ArtMethod* method)
    REQUIRES_SHARED(Locks::mutator_lock_);
mirror::Object* GetAnnotationForMethod(ArtMethod* method, Handle<mirror::Class> annotation_class)
    REQUIRES_SHARED(Locks::mutator_lock_);
mirror::ObjectArray<mirror::Object>* GetAnnotationsForMethod(ArtMethod* method)
    REQUIRES_SHARED(Locks::mutator_lock_);
mirror::ObjectArray<mirror::Class>* GetExceptionTypesForMethod(ArtMethod* method)
    REQUIRES_SHARED(Locks::mutator_lock_);
mirror::ObjectArray<mirror::Object>* GetParameterAnnotations(ArtMethod* method)
    REQUIRES_SHARED(Locks::mutator_lock_);
uint32_t GetNumberOfAnnotatedMethodParameters(ArtMethod* method)
    REQUIRES_SHARED(Locks::mutator_lock_);
mirror::Object* GetAnnotationForMethodParameter(ArtMethod* method,
                                                uint32_t parameter_idx,
                                                Handle<mirror::Class> annotation_class)
    REQUIRES_SHARED(Locks::mutator_lock_);
bool GetParametersMetadataForMethod(ArtMethod* method,
                                    MutableHandle<mirror::ObjectArray<mirror::String>>* names,
                                    MutableHandle<mirror::IntArray>* access_flags)
    REQUIRES_SHARED(Locks::mutator_lock_);
mirror::ObjectArray<mirror::String>* GetSignatureAnnotationForMethod(ArtMethod* method)
    REQUIRES_SHARED(Locks::mutator_lock_);
// Check whether `method` is annotated with `annotation_class`.
// If `lookup_in_resolved_boot_classes` is true, look up any of the
// method's annotations' classes in the bootstrap class loader's
// resolved types; if it is false (default value), resolve them as a
// side effect.
bool IsMethodAnnotationPresent(ArtMethod* method,
                               Handle<mirror::Class> annotation_class,
                               uint32_t visibility = DexFile::kDexVisibilityRuntime)
    REQUIRES_SHARED(Locks::mutator_lock_);
// Check whether a method from the `dex_file` with the given `method_index`
// is annotated with @dalvik.annotation.optimization.FastNative or
// @dalvik.annotation.optimization.CriticalNative with build visibility.
// If yes, return the associated access flags, i.e. kAccFastNative or kAccCriticalNative.
uint32_t GetNativeMethodAnnotationAccessFlags(const DexFile& dex_file,
                                              const DexFile::ClassDef& class_def,
                                              uint32_t method_index);

// Class annotations.
mirror::Object* GetAnnotationForClass(Handle<mirror::Class> klass,
                                      Handle<mirror::Class> annotation_class)
    REQUIRES_SHARED(Locks::mutator_lock_);
mirror::ObjectArray<mirror::Object>* GetAnnotationsForClass(Handle<mirror::Class> klass)
    REQUIRES_SHARED(Locks::mutator_lock_);
mirror::ObjectArray<mirror::Class>* GetDeclaredClasses(Handle<mirror::Class> klass)
    REQUIRES_SHARED(Locks::mutator_lock_);
mirror::Class* GetDeclaringClass(Handle<mirror::Class> klass)
    REQUIRES_SHARED(Locks::mutator_lock_);
mirror::Class* GetEnclosingClass(Handle<mirror::Class> klass)
    REQUIRES_SHARED(Locks::mutator_lock_);
mirror::Object* GetEnclosingMethod(Handle<mirror::Class> klass)
    REQUIRES_SHARED(Locks::mutator_lock_);
bool GetInnerClass(Handle<mirror::Class> klass, mirror::String** name)
    REQUIRES_SHARED(Locks::mutator_lock_);
bool GetInnerClassFlags(Handle<mirror::Class> klass, uint32_t* flags)
    REQUIRES_SHARED(Locks::mutator_lock_);
mirror::ObjectArray<mirror::String>* GetSignatureAnnotationForClass(Handle<mirror::Class> klass)
    REQUIRES_SHARED(Locks::mutator_lock_);
const char* GetSourceDebugExtension(Handle<mirror::Class> klass)
    REQUIRES_SHARED(Locks::mutator_lock_);
bool IsClassAnnotationPresent(Handle<mirror::Class> klass,
                              Handle<mirror::Class> annotation_class)
    REQUIRES_SHARED(Locks::mutator_lock_);

// Map back from a PC to the line number in a method.
int32_t GetLineNumFromPC(const DexFile* dex_file, ArtMethod* method, uint32_t rel_pc)
    REQUIRES_SHARED(Locks::mutator_lock_);

// Annotations iterator.
class RuntimeEncodedStaticFieldValueIterator : public EncodedStaticFieldValueIterator {
 public:
  // A constructor meant to be called from runtime code.
  RuntimeEncodedStaticFieldValueIterator(Handle<mirror::DexCache> dex_cache,
                                         Handle<mirror::ClassLoader> class_loader,
                                         ClassLinker* linker,
                                         const DexFile::ClassDef& class_def)
      REQUIRES_SHARED(Locks::mutator_lock_)
      : EncodedStaticFieldValueIterator(*dex_cache->GetDexFile(), class_def),
        dex_cache_(dex_cache),
        class_loader_(class_loader),
        linker_(linker) {
  }

  template<bool kTransactionActive>
  void ReadValueToField(ArtField* field) const REQUIRES_SHARED(Locks::mutator_lock_);

 private:
  const Handle<mirror::DexCache> dex_cache_;  // Dex cache to resolve literal objects.
  const Handle<mirror::ClassLoader> class_loader_;  // ClassLoader to resolve types.
  ClassLinker* const linker_;  // Linker to resolve literal objects.
  DISALLOW_IMPLICIT_CONSTRUCTORS(RuntimeEncodedStaticFieldValueIterator);
};

}  // namespace annotations

}  // namespace art

#endif  // ART_RUNTIME_DEX_DEX_FILE_ANNOTATIONS_H_
