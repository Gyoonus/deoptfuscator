/*
 * Copyright (C) 2016 Google Inc.
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

package com.android.ahat.proguard;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.IOException;
import java.io.Reader;
import java.text.ParseException;
import java.util.HashMap;
import java.util.Map;

/**
 * A representation of a proguard mapping for deobfuscating class names,
 * field names, and stack frames.
 */
public class ProguardMap {

  private static final String ARRAY_SYMBOL = "[]";

  private static class FrameData {
    public FrameData(String clearMethodName, int lineDelta) {
      this.clearMethodName = clearMethodName;
      this.lineDelta = lineDelta;
    }

    public final String clearMethodName;
    public final int lineDelta;   // lineDelta = obfuscatedLine - clearLine
  }

  private static class ClassData {
    private final String mClearName;

    // Mapping from obfuscated field name to clear field name.
    private final Map<String, String> mFields = new HashMap<String, String>();

    // obfuscatedMethodName + clearSignature -> FrameData
    private final Map<String, FrameData> mFrames = new HashMap<String, FrameData>();

    // Constructs a ClassData object for a class with the given clear name.
    public ClassData(String clearName) {
      mClearName = clearName;
    }

    // Returns the clear name of the class.
    public String getClearName() {
      return mClearName;
    }

    public void addField(String obfuscatedName, String clearName) {
      mFields.put(obfuscatedName, clearName);
    }

    // Get the clear name for the field in this class with the given
    // obfuscated name. Returns the original obfuscated name if a clear
    // name for the field could not be determined.
    // TODO: Do we need to take into account the type of the field to
    // propery determine the clear name?
    public String getField(String obfuscatedName) {
      String clearField = mFields.get(obfuscatedName);
      return clearField == null ? obfuscatedName : clearField;
    }

    // TODO: Does this properly interpret the meaning of line numbers? Is
    // it possible to have multiple frame entries for the same method
    // name and signature that differ only by line ranges?
    public void addFrame(String obfuscatedMethodName, String clearMethodName,
        String clearSignature, int obfuscatedLine, int clearLine) {
      String key = obfuscatedMethodName + clearSignature;
      mFrames.put(key, new FrameData(clearMethodName, obfuscatedLine - clearLine));
    }

    public Frame getFrame(String clearClassName, String obfuscatedMethodName,
        String clearSignature, String obfuscatedFilename, int obfuscatedLine) {
      String key = obfuscatedMethodName + clearSignature;
      FrameData frame = mFrames.get(key);
      if (frame == null) {
        frame = new FrameData(obfuscatedMethodName, 0);
      }
      return new Frame(frame.clearMethodName, clearSignature,
          getFileName(clearClassName), obfuscatedLine - frame.lineDelta);
    }
  }

  private Map<String, ClassData> mClassesFromClearName = new HashMap<String, ClassData>();
  private Map<String, ClassData> mClassesFromObfuscatedName = new HashMap<String, ClassData>();

  /**
   * Information associated with a stack frame that identifies a particular
   * line of source code.
   */
  public static class Frame {
    Frame(String method, String signature, String filename, int line) {
      this.method = method;
      this.signature = signature;
      this.filename = filename;
      this.line = line;
    }

    /**
     * The name of the method the stack frame belongs to.
     * For example, "equals".
     */
    public final String method;

    /**
     * The signature of the method the stack frame belongs to.
     * For example, "(Ljava/lang/Object;)Z".
     */
    public final String signature;

    /**
     * The name of the file with containing the line of source that the stack
     * frame refers to.
     */
    public final String filename;

    /**
     * The line number of the code in the source file that the stack frame
     * refers to.
     */
    public final int line;
  }

  private static void parseException(String msg) throws ParseException {
    throw new ParseException(msg, 0);
  }

  /**
   * Creates a new empty proguard mapping.
   * The {@link #readFromFile readFromFile} and
   * {@link #readFromReader readFromReader} methods can be used to populate
   * the proguard mapping with proguard mapping information.
   */
  public ProguardMap() {
  }

  /**
   * Adds the proguard mapping information in <code>mapFile</code> to this
   * proguard mapping.
   * The <code>mapFile</code> should be a proguard mapping file generated with
   * the <code>-printmapping</code> option when proguard was run.
   *
   * @param mapFile the name of a file with proguard mapping information
   * @throws FileNotFoundException If the <code>mapFile</code> could not be
   *                               found
   * @throws IOException If an input exception occurred.
   * @throws ParseException If the <code>mapFile</code> is not a properly
   *                        formatted proguard mapping file.
   */
  public void readFromFile(File mapFile)
    throws FileNotFoundException, IOException, ParseException {
    readFromReader(new FileReader(mapFile));
  }

  /**
   * Adds the proguard mapping information read from <code>mapReader</code> to
   * this proguard mapping.
   * <code>mapReader</code> should be a Reader of a proguard mapping file
   * generated with the <code>-printmapping</code> option when proguard was run.
   *
   * @param mapReader a Reader for reading the proguard mapping information
   * @throws IOException If an input exception occurred.
   * @throws ParseException If the <code>mapFile</code> is not a properly
   *                        formatted proguard mapping file.
   */
  public void readFromReader(Reader mapReader) throws IOException, ParseException {
    BufferedReader reader = new BufferedReader(mapReader);
    String line = reader.readLine();
    while (line != null) {
      // Class lines are of the form:
      //   'clear.class.name -> obfuscated_class_name:'
      int sep = line.indexOf(" -> ");
      if (sep == -1 || sep + 5 >= line.length()) {
        parseException("Error parsing class line: '" + line + "'");
      }
      String clearClassName = line.substring(0, sep);
      String obfuscatedClassName = line.substring(sep + 4, line.length() - 1);

      ClassData classData = new ClassData(clearClassName);
      mClassesFromClearName.put(clearClassName, classData);
      mClassesFromObfuscatedName.put(obfuscatedClassName, classData);

      // After the class line comes zero or more field/method lines of the form:
      //   '    type clearName -> obfuscatedName'
      line = reader.readLine();
      while (line != null && line.startsWith("    ")) {
        String trimmed = line.trim();
        int ws = trimmed.indexOf(' ');
        sep = trimmed.indexOf(" -> ");
        if (ws == -1 || sep == -1) {
          parseException("Error parse field/method line: '" + line + "'");
        }

        String type = trimmed.substring(0, ws);
        String clearName = trimmed.substring(ws + 1, sep);
        String obfuscatedName = trimmed.substring(sep + 4, trimmed.length());

        // If the clearName contains '(', then this is for a method instead of a
        // field.
        if (clearName.indexOf('(') == -1) {
          classData.addField(obfuscatedName, clearName);
        } else {
          // For methods, the type is of the form: [#:[#:]]<returnType>
          int obfuscatedLine = 0;
          int colon = type.indexOf(':');
          if (colon != -1) {
            obfuscatedLine = Integer.parseInt(type.substring(0, colon));
            type = type.substring(colon + 1);
          }
          colon = type.indexOf(':');
          if (colon != -1) {
            type = type.substring(colon + 1);
          }

          // For methods, the clearName is of the form: <clearName><sig>[:#[:#]]
          int op = clearName.indexOf('(');
          int cp = clearName.indexOf(')');
          if (op == -1 || cp == -1) {
            parseException("Error parse method line: '" + line + "'");
          }

          String sig = clearName.substring(op, cp + 1);

          int clearLine = obfuscatedLine;
          colon = clearName.lastIndexOf(':');
          if (colon != -1) {
            clearLine = Integer.parseInt(clearName.substring(colon + 1));
            clearName = clearName.substring(0, colon);
          }

          colon = clearName.lastIndexOf(':');
          if (colon != -1) {
            clearLine = Integer.parseInt(clearName.substring(colon + 1));
            clearName = clearName.substring(0, colon);
          }

          clearName = clearName.substring(0, op);

          String clearSig = fromProguardSignature(sig + type);
          classData.addFrame(obfuscatedName, clearName, clearSig,
              obfuscatedLine, clearLine);
        }

        line = reader.readLine();
      }
    }
    reader.close();
  }

  /**
   * Returns the deobfuscated version of the given obfuscated class name.
   * If this proguard mapping does not include information about how to
   * deobfuscate the obfuscated class name, the obfuscated class name
   * is returned.
   *
   * @param obfuscatedClassName the obfuscated class name to deobfuscate
   * @return the deobfuscated class name.
   */
  public String getClassName(String obfuscatedClassName) {
    // Class names for arrays may have trailing [] that need to be
    // stripped before doing the lookup.
    String baseName = obfuscatedClassName;
    String arraySuffix = "";
    while (baseName.endsWith(ARRAY_SYMBOL)) {
      arraySuffix += ARRAY_SYMBOL;
      baseName = baseName.substring(0, baseName.length() - ARRAY_SYMBOL.length());
    }

    ClassData classData = mClassesFromObfuscatedName.get(baseName);
    String clearBaseName = classData == null ? baseName : classData.getClearName();
    return clearBaseName + arraySuffix;
  }

  /**
   * Returns the deobfuscated version of the obfuscated field name for the
   * given deobfuscated class name.
   * If this proguard mapping does not include information about how to
   * deobfuscate the obfuscated field name, the obfuscated field name is
   * returned.
   *
   * @param clearClass the deobfuscated name of the class the field belongs to
   * @param obfuscatedField the obfuscated field name to deobfuscate
   * @return the deobfuscated field name.
   */
  public String getFieldName(String clearClass, String obfuscatedField) {
    ClassData classData = mClassesFromClearName.get(clearClass);
    if (classData == null) {
      return obfuscatedField;
    }
    return classData.getField(obfuscatedField);
  }

  /**
   * Returns the deobfuscated version of the obfuscated stack frame
   * information for the given deobfuscated class name.
   * If this proguard mapping does not include information about how to
   * deobfuscate the obfuscated stack frame information, the obfuscated stack
   * frame information is returned.
   *
   * @param clearClassName the deobfuscated name of the class the stack frame's
   * method belongs to
   * @param obfuscatedMethodName the obfuscated method name to deobfuscate
   * @param obfuscatedSignature the obfuscated method signature to deobfuscate
   * @param obfuscatedFilename the obfuscated file name to deobfuscate.
   * @param obfuscatedLine the obfuscated line number to deobfuscate.
   * @return the deobfuscated stack frame information.
   */
  public Frame getFrame(String clearClassName, String obfuscatedMethodName,
      String obfuscatedSignature, String obfuscatedFilename, int obfuscatedLine) {
    String clearSignature = getSignature(obfuscatedSignature);
    ClassData classData = mClassesFromClearName.get(clearClassName);
    if (classData == null) {
      return new Frame(obfuscatedMethodName, clearSignature,
          obfuscatedFilename, obfuscatedLine);
    }
    return classData.getFrame(clearClassName, obfuscatedMethodName, clearSignature,
        obfuscatedFilename, obfuscatedLine);
  }

  // Converts a proguard-formatted method signature into a Java formatted
  // method signature.
  private static String fromProguardSignature(String sig) throws ParseException {
    if (sig.startsWith("(")) {
      int end = sig.indexOf(')');
      if (end == -1) {
        parseException("Error parsing signature: " + sig);
      }

      StringBuilder converted = new StringBuilder();
      converted.append('(');
      if (end > 1) {
        for (String arg : sig.substring(1, end).split(",")) {
          converted.append(fromProguardSignature(arg));
        }
      }
      converted.append(')');
      converted.append(fromProguardSignature(sig.substring(end + 1)));
      return converted.toString();
    } else if (sig.endsWith(ARRAY_SYMBOL)) {
      return "[" + fromProguardSignature(sig.substring(0, sig.length() - 2));
    } else if (sig.equals("boolean")) {
      return "Z";
    } else if (sig.equals("byte")) {
      return "B";
    } else if (sig.equals("char")) {
      return "C";
    } else if (sig.equals("short")) {
      return "S";
    } else if (sig.equals("int")) {
      return "I";
    } else if (sig.equals("long")) {
      return "J";
    } else if (sig.equals("float")) {
      return "F";
    } else if (sig.equals("double")) {
      return "D";
    } else if (sig.equals("void")) {
      return "V";
    } else {
      return "L" + sig.replace('.', '/') + ";";
    }
  }

  // Return a clear signature for the given obfuscated signature.
  private String getSignature(String obfuscatedSig) {
    StringBuilder builder = new StringBuilder();
    for (int i = 0; i < obfuscatedSig.length(); i++) {
      if (obfuscatedSig.charAt(i) == 'L') {
        int e = obfuscatedSig.indexOf(';', i);
        builder.append('L');
        String cls = obfuscatedSig.substring(i + 1, e).replace('/', '.');
        builder.append(getClassName(cls).replace('.', '/'));
        builder.append(';');
        i = e;
      } else {
        builder.append(obfuscatedSig.charAt(i));
      }
    }
    return builder.toString();
  }

  // Return a file name for the given clear class name.
  private static String getFileName(String clearClass) {
    String filename = clearClass;
    int dot = filename.lastIndexOf('.');
    if (dot != -1) {
      filename = filename.substring(dot + 1);
    }

    int dollar = filename.indexOf('$');
    if (dollar != -1) {
      filename = filename.substring(0, dollar);
    }
    return filename + ".java";
  }
}
