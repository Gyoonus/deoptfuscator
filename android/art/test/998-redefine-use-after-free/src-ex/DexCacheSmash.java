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

import art.Redefinition;
import java.util.Base64;

public class DexCacheSmash {
  static class Transform {
    public void foo() {}
    public void bar() {}
    public String getId() {
      return "TRANSFORM_INITIAL";
    }
  }

  static class Transform2 {
    public String getId() {
      return "TRANSFORM2_INITIAL";
    }
  }

  /**
   * A base64 encoding of the dex/class file of the Transform class above.
   */
  static final  Redefinition.CommonClassDefinition TRANSFORM_INITIAL =
      new Redefinition.CommonClassDefinition(Transform.class,
          Base64.getDecoder().decode(
            "yv66vgAAADQAFwoABAAPCAAQBwASBwAVAQAGPGluaXQ+AQADKClWAQAEQ29kZQEAD0xpbmVOdW1i" +
            "ZXJUYWJsZQEAA2ZvbwEAA2JhcgEABWdldElkAQAUKClMamF2YS9sYW5nL1N0cmluZzsBAApTb3Vy" +
            "Y2VGaWxlAQASRGV4Q2FjaGVTbWFzaC5qYXZhDAAFAAYBABFUUkFOU0ZPUk1fSU5JVElBTAcAFgEA" +
            "F0RleENhY2hlU21hc2gkVHJhbnNmb3JtAQAJVHJhbnNmb3JtAQAMSW5uZXJDbGFzc2VzAQAQamF2" +
            "YS9sYW5nL09iamVjdAEADURleENhY2hlU21hc2gAIAADAAQAAAAAAAQAAAAFAAYAAQAHAAAAHQAB" +
            "AAEAAAAFKrcAAbEAAAABAAgAAAAGAAEAAAATAAEACQAGAAEABwAAABkAAAABAAAAAbEAAAABAAgA" +
            "AAAGAAEAAAAUAAEACgAGAAEABwAAABkAAAABAAAAAbEAAAABAAgAAAAGAAEAAAAVAAEACwAMAAEA" +
            "BwAAABsAAQABAAAAAxICsAAAAAEACAAAAAYAAQAAABcAAgANAAAAAgAOABQAAAAKAAEAAwARABMA" +
            "CA=="),
          Base64.getDecoder().decode(
            "ZGV4CjAzNQDhg9CfghG1SRlLClguRuFYsqihr4F7NsGQAwAAcAAAAHhWNBIAAAAAAAAAAOQCAAAS" +
            "AAAAcAAAAAcAAAC4AAAAAgAAANQAAAAAAAAAAAAAAAUAAADsAAAAAQAAABQBAABcAgAANAEAAKgB" +
            "AACwAQAAxAEAAMcBAADiAQAA8wEAABcCAAA3AgAASwIAAF8CAAByAgAAfQIAAIACAACNAgAAkgIA" +
            "AJcCAACeAgAApAIAAAMAAAAEAAAABQAAAAYAAAAHAAAACAAAAAsAAAACAAAABQAAAAAAAAALAAAA" +
            "BgAAAAAAAAAAAAEAAAAAAAAAAQANAAAAAAABAA4AAAAAAAAADwAAAAQAAQAAAAAAAAAAAAAAAAAE" +
            "AAAAAAAAAAEAAACYAQAAzgIAAAAAAAACAAAAvwIAAMUCAAABAAEAAQAAAKsCAAAEAAAAcBAEAAAA" +
            "DgABAAEAAAAAALACAAABAAAADgAAAAEAAQAAAAAAtQIAAAEAAAAOAAAAAgABAAAAAAC6AgAAAwAA" +
            "ABoACQARAAAANAEAAAAAAAAAAAAAAAAAAAY8aW5pdD4AEkRleENhY2hlU21hc2guamF2YQABTAAZ" +
            "TERleENhY2hlU21hc2gkVHJhbnNmb3JtOwAPTERleENhY2hlU21hc2g7ACJMZGFsdmlrL2Fubm90" +
            "YXRpb24vRW5jbG9zaW5nQ2xhc3M7AB5MZGFsdmlrL2Fubm90YXRpb24vSW5uZXJDbGFzczsAEkxq" +
            "YXZhL2xhbmcvT2JqZWN0OwASTGphdmEvbGFuZy9TdHJpbmc7ABFUUkFOU0ZPUk1fSU5JVElBTAAJ" +
            "VHJhbnNmb3JtAAFWAAthY2Nlc3NGbGFncwADYmFyAANmb28ABWdldElkAARuYW1lAAV2YWx1ZQAT" +
            "AAcOABUABw4AFAAHDgAXAAcOAAICAREYAQIDAgwECBAXCgAAAQMAgIAEwAIBAdgCAQHsAgEBgAMO" +
            "AAAAAAAAAAEAAAAAAAAAAQAAABIAAABwAAAAAgAAAAcAAAC4AAAAAwAAAAIAAADUAAAABQAAAAUA" +
            "AADsAAAABgAAAAEAAAAUAQAAAxAAAAEAAAA0AQAAASAAAAQAAABAAQAABiAAAAEAAACYAQAAAiAA" +
            "ABIAAACoAQAAAyAAAAQAAACrAgAABCAAAAIAAAC/AgAAACAAAAEAAADOAgAAABAAAAEAAADkAgAA"));

  /**
   * A base64 encoding of the following (invalid) class.
   *
   *  .class LDexCacheSmash$Transform2;
   *  .super Ljava/lang/Object;
   *  .source "DexCacheSmash.java"
   *
   *  # annotations
   *  .annotation system Ldalvik/annotation/EnclosingClass;
   *      value = LDexCacheSmash;
   *  .end annotation
   *
   *  .annotation system Ldalvik/annotation/InnerClass;
   *      accessFlags = 0x8
   *      name = "Transform2"
   *  .end annotation
   *
   *
   *  # direct methods
   *  .method constructor <init>()V
   *      .registers 1
   *
   *      .prologue
   *      .line 26
   *      invoke-direct {p0}, Ljava/lang/Object;-><init>()V
   *
   *      return-void
   *  .end method
   *
   *
   *  # virtual methods
   *  .method public getId()Ljava/lang/String;
   *      .registers 2
   *
   *      .prologue
   *      .line 28
   *      # NB Fails verification due to this function not returning a String.
   *      return-void
   *  .end method
   */
  static final  Redefinition.CommonClassDefinition TRANSFORM2_INVALID =
      new Redefinition.CommonClassDefinition(Transform2.class,
          Base64.getDecoder().decode(
            "yv66vgAAADQAEwcAEgcAEQEABjxpbml0PgEAAygpVgEABENvZGUKAAIAEAEAD0xpbmVOdW1iZXJU" +
            "YWJsZQEABWdldElkAQAUKClMamF2YS9sYW5nL1N0cmluZzsBAApTb3VyY2VGaWxlAQASRGV4Q2Fj" +
            "aGVTbWFzaC5qYXZhAQAMSW5uZXJDbGFzc2VzBwAPAQAKVHJhbnNmb3JtMgEADURleENhY2hlU21h" +
            "c2gMAAMABAEAEGphdmEvbGFuZy9PYmplY3QBABhEZXhDYWNoZVNtYXNoJFRyYW5zZm9ybTIAIAAB" +
            "AAIAAAAAAAIAAAADAAQAAQAFAAAAHQABAAEAAAAFKrcABrEAAAABAAcAAAAGAAEAAAAaAAEACAAJ" +
            "AAEABQAAABkAAQABAAAAAbEAAAABAAcAAAAGAAEAAAAcAAIACgAAAAIACwAMAAAACgABAAEADQAO" +
            "AAg="),
          Base64.getDecoder().decode(
            "ZGV4CjAzNQCFcegr6Ns+I7iEF4uLRkUX4yGrLhP6soEgAwAAcAAAAHhWNBIAAAAAAAAAAHQCAAAP" +
            "AAAAcAAAAAcAAACsAAAAAgAAAMgAAAAAAAAAAAAAAAMAAADgAAAAAQAAAPgAAAAIAgAAGAEAABgB" +
            "AAAgAQAANAEAADcBAABTAQAAZAEAAIgBAACoAQAAvAEAANABAADcAQAA3wEAAOwBAADzAQAA+QEA" +
            "AAMAAAAEAAAABQAAAAYAAAAHAAAACAAAAAoAAAACAAAABQAAAAAAAAAKAAAABgAAAAAAAAAAAAEA" +
            "AAAAAAAAAAAMAAAABAABAAAAAAAAAAAAAAAAAAQAAAAAAAAAAQAAACACAABmAgAAAAAAAAY8aW5p" +
            "dD4AEkRleENhY2hlU21hc2guamF2YQABTAAaTERleENhY2hlU21hc2gkVHJhbnNmb3JtMjsAD0xE" +
            "ZXhDYWNoZVNtYXNoOwAiTGRhbHZpay9hbm5vdGF0aW9uL0VuY2xvc2luZ0NsYXNzOwAeTGRhbHZp" +
            "ay9hbm5vdGF0aW9uL0lubmVyQ2xhc3M7ABJMamF2YS9sYW5nL09iamVjdDsAEkxqYXZhL2xhbmcv" +
            "U3RyaW5nOwAKVHJhbnNmb3JtMgABVgALYWNjZXNzRmxhZ3MABWdldElkAARuYW1lAAV2YWx1ZQAC" +
            "AwILBAgNFwkCAgEOGAEAAAAAAAIAAAAJAgAAAAIAABQCAAAAAAAAAAAAAAAAAAAaAAcOABwABw4A" +
            "AAABAAEAAQAAADACAAAEAAAAcBACAAAADgACAAEAAAAAADUCAAABAAAADgAAAAEBAICABLwEAQHU" +
            "BA4AAAAAAAAAAQAAAAAAAAABAAAADwAAAHAAAAACAAAABwAAAKwAAAADAAAAAgAAAMgAAAAFAAAA" +
            "AwAAAOAAAAAGAAAAAQAAAPgAAAACIAAADwAAABgBAAAEIAAAAgAAAAACAAADEAAAAgAAABACAAAG" +
            "IAAAAQAAACACAAADIAAAAgAAADACAAABIAAAAgAAADwCAAAAIAAAAQAAAGYCAAAAEAAAAQAAAHQC" +
            "AAA="));

  public static void run() throws Exception {
    try {
      Redefinition.doMultiClassRedefinition(TRANSFORM2_INVALID);
    } catch (Exception e) {
      if (!e.getMessage().endsWith("JVMTI_ERROR_FAILS_VERIFICATION")) {
        throw new Error(
            "Unexpected error: Expected failure due to JVMTI_ERROR_FAILS_VERIFICATION", e);
      }
    }
    // Doing this redefinition after a redefinition that failed due to FAILS_VERIFICATION could
    // cause a use-after-free of the Transform2's DexCache by the redefinition code if it happens
    // that the native pointer of the art::DexFile created for the Transform redefinition aliases
    // the one created for Transform2's failed redefinition.
    //
    // Due to the order of checks performed by the redefinition code FAILS_VERIFICATION is the only
    // failure mode that can cause Use-after-frees in this way.
    //
    // This should never throw any exceptions (except perhaps OOME in very strange circumstances).
    Redefinition.doMultiClassRedefinition(TRANSFORM_INITIAL);
  }
}
