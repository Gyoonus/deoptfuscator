/*
 * Copyright (C) 2017 The Android Open Source Project
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

package art;

import java.lang.reflect.Executable;
import java.util.Objects;

public class Locals {
  public static native void EnableLocalVariableAccess();

  public static class VariableDescription {
    public final long start_location;
    public final int length;
    public final String name;
    public final String signature;
    public final String generic_signature;
    public final int slot;

    public VariableDescription(
        long start, int length, String name, String sig, String gen_sig, int slot) {
      this.start_location = start;
      this.length = length;
      this.name = name;
      this.signature = sig;
      this.generic_signature = gen_sig;
      this.slot = slot;
    }

    @Override
    public String toString() {
      return String.format(
          "VariableDescription { " +
            "Sig: '%s', Name: '%s', Gen_sig: '%s', slot: %d, start: %d, len: %d" +
          "}",
          this.signature,
          this.name,
          this.generic_signature,
          this.slot,
          this.start_location,
          this.length);
    }
    public boolean equals(Object other) {
      if (!(other instanceof VariableDescription)) {
        return false;
      } else {
        VariableDescription v = (VariableDescription)other;
        return Objects.equals(v.signature, signature) &&
            Objects.equals(v.name, name) &&
            Objects.equals(v.generic_signature, generic_signature) &&
            v.slot == slot &&
            v.start_location == start_location &&
            v.length == length;
      }
    }
    public int hashCode() {
      return Objects.hash(this.signature, this.name, this.generic_signature, this.slot,
          this.start_location, this.length);
    }
  }

  public static native VariableDescription[] GetLocalVariableTable(Executable e);

  public static VariableDescription GetVariableAtLine(
      Executable e, String name, String sig, int line) throws Exception {
    return GetVariableAtLocation(e, name, sig, Breakpoint.lineToLocation(e, line));
  }

  public static VariableDescription GetVariableAtLocation(
      Executable e, String name, String sig, long loc) {
    VariableDescription[] vars = GetLocalVariableTable(e);
    for (VariableDescription var : vars) {
      if (var.start_location <= loc &&
          var.length + var.start_location > loc &&
          var.name.equals(name) &&
          var.signature.equals(sig)) {
        return var;
      }
    }
    throw new Error(
        "Unable to find variable " + name + " (sig: " + sig + ") in " + e + " at loc " + loc);
  }

  public static native int GetLocalVariableInt(Thread thr, int depth, int slot);
  public static native long GetLocalVariableLong(Thread thr, int depth, int slot);
  public static native float GetLocalVariableFloat(Thread thr, int depth, int slot);
  public static native double GetLocalVariableDouble(Thread thr, int depth, int slot);
  public static native Object GetLocalVariableObject(Thread thr, int depth, int slot);
  public static native Object GetLocalInstance(Thread thr, int depth);

  public static void SetLocalVariableInt(Thread thr, int depth, int slot, Object val) {
    SetLocalVariableInt(thr, depth, slot, ((Number)val).intValue());
  }
  public static void SetLocalVariableLong(Thread thr, int depth, int slot, Object val) {
    SetLocalVariableLong(thr, depth, slot, ((Number)val).longValue());
  }
  public static void SetLocalVariableFloat(Thread thr, int depth, int slot, Object val) {
    SetLocalVariableFloat(thr, depth, slot, ((Number)val).floatValue());
  }
  public static void SetLocalVariableDouble(Thread thr, int depth, int slot, Object val) {
    SetLocalVariableDouble(thr, depth, slot, ((Number)val).doubleValue());
  }
  public static native void SetLocalVariableInt(Thread thr, int depth, int slot, int val);
  public static native void SetLocalVariableLong(Thread thr, int depth, int slot, long val);
  public static native void SetLocalVariableFloat(Thread thr, int depth, int slot, float val);
  public static native void SetLocalVariableDouble(Thread thr, int depth, int slot, double val);
  public static native void SetLocalVariableObject(Thread thr, int depth, int slot, Object val);
}
