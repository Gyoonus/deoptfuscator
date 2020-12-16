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

#ifndef ART_RUNTIME_TI_AGENT_H_
#define ART_RUNTIME_TI_AGENT_H_

#include <dlfcn.h>
#include <jni.h>  // for jint, JavaVM* etc declarations

#include <memory>

#include "base/logging.h"

namespace art {
namespace ti {

class Agent;

enum LoadError {
  kNoError,              // No error occurred..
  kLoadingError,         // dlopen or dlsym returned an error.
  kInitializationError,  // The entrypoint did not return 0. This might require an abort.
};

class AgentSpec {
 public:
  explicit AgentSpec(const std::string& arg);

  const std::string& GetName() const {
    return name_;
  }

  const std::string& GetArgs() const {
    return args_;
  }

  bool HasArgs() const {
    return !GetArgs().empty();
  }

  std::unique_ptr<Agent> Load(/*out*/jint* call_res,
                              /*out*/LoadError* error,
                              /*out*/std::string* error_msg);

  // Tries to attach the agent using its OnAttach method. Returns true on success.
  std::unique_ptr<Agent> Attach(JNIEnv* env,
                                jobject class_loader,
                                /*out*/jint* call_res,
                                /*out*/LoadError* error,
                                /*out*/std::string* error_msg);

 private:
  std::unique_ptr<Agent> DoDlOpen(JNIEnv* env,
                                  jobject class_loader,
                                  /*out*/LoadError* error,
                                  /*out*/std::string* error_msg);

  std::unique_ptr<Agent> DoLoadHelper(JNIEnv* env,
                                      bool attaching,
                                      jobject class_loader,
                                      /*out*/jint* call_res,
                                      /*out*/LoadError* error,
                                      /*out*/std::string* error_msg);

  std::string name_;
  std::string args_;

  friend std::ostream& operator<<(std::ostream &os, AgentSpec const& m);
};

std::ostream& operator<<(std::ostream &os, AgentSpec const& m);

using AgentOnLoadFunction = jint (*)(JavaVM*, const char*, void*);
using AgentOnUnloadFunction = void (*)(JavaVM*);

// Agents are native libraries that will be loaded by the runtime for the purpose of
// instrumentation. They will be entered by Agent_OnLoad or Agent_OnAttach depending on whether the
// agent is being attached during runtime startup or later.
//
// The agent's Agent_OnUnload function will be called during runtime shutdown.
//
// TODO: consider splitting ti::Agent into command line, agent and shared library handler classes
// TODO Support native-bridge. Currently agents can only be the actual runtime ISA of the device.
class Agent {
 public:
  const std::string& GetName() const {
    return name_;
  }

  void* FindSymbol(const std::string& name) const;

  // TODO We need to acquire some locks probably.
  void Unload();

  Agent(Agent&& other);
  Agent& operator=(Agent&& other);

  ~Agent();

 private:
  Agent(const std::string& name, void* dlopen_handle) : name_(name),
                                                        dlopen_handle_(dlopen_handle),
                                                        onload_(nullptr),
                                                        onattach_(nullptr),
                                                        onunload_(nullptr) {
    DCHECK(dlopen_handle != nullptr);
  }

  void PopulateFunctions();

  std::string name_;
  void* dlopen_handle_;

  // The entrypoints.
  AgentOnLoadFunction onload_;
  AgentOnLoadFunction onattach_;
  AgentOnUnloadFunction onunload_;

  friend class AgentSpec;
  friend std::ostream& operator<<(std::ostream &os, Agent const& m);

  DISALLOW_COPY_AND_ASSIGN(Agent);
};

std::ostream& operator<<(std::ostream &os, Agent const& m);
std::ostream& operator<<(std::ostream &os, const Agent* m);

}  // namespace ti
}  // namespace art

#endif  // ART_RUNTIME_TI_AGENT_H_

