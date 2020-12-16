/*
 * Copyright (C) 2014 The Android Open Source Project
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

package dexfuzz.program;

import dexfuzz.Log;
import dexfuzz.MutationStats;
import dexfuzz.Options;
import dexfuzz.listeners.BaseListener;
import dexfuzz.program.mutators.ArithOpChanger;
import dexfuzz.program.mutators.BranchShifter;
import dexfuzz.program.mutators.CmpBiasChanger;
import dexfuzz.program.mutators.CodeMutator;
import dexfuzz.program.mutators.ConstantValueChanger;
import dexfuzz.program.mutators.ConversionRepeater;
import dexfuzz.program.mutators.FieldFlagChanger;
import dexfuzz.program.mutators.InstructionDeleter;
import dexfuzz.program.mutators.InstructionDuplicator;
import dexfuzz.program.mutators.InstructionSwapper;
import dexfuzz.program.mutators.InvokeChanger;
import dexfuzz.program.mutators.NewArrayLengthChanger;
import dexfuzz.program.mutators.NewInstanceChanger;
import dexfuzz.program.mutators.NewMethodCaller;
import dexfuzz.program.mutators.NonsenseStringPrinter;
import dexfuzz.program.mutators.OppositeBranchChanger;
import dexfuzz.program.mutators.PoolIndexChanger;
import dexfuzz.program.mutators.RandomBranchChanger;
import dexfuzz.program.mutators.RandomInstructionGenerator;
import dexfuzz.program.mutators.RegisterClobber;
import dexfuzz.program.mutators.SwitchBranchShifter;
import dexfuzz.program.mutators.TryBlockShifter;
import dexfuzz.program.mutators.ValuePrinter;
import dexfuzz.program.mutators.VRegChanger;
import dexfuzz.rawdex.ClassDataItem;
import dexfuzz.rawdex.ClassDefItem;
import dexfuzz.rawdex.CodeItem;
import dexfuzz.rawdex.DexRandomAccessFile;
import dexfuzz.rawdex.EncodedField;
import dexfuzz.rawdex.EncodedMethod;
import dexfuzz.rawdex.FieldIdItem;
import dexfuzz.rawdex.MethodIdItem;
import dexfuzz.rawdex.ProtoIdItem;
import dexfuzz.rawdex.RawDexFile;
import dexfuzz.rawdex.TypeIdItem;
import dexfuzz.rawdex.TypeList;
import dexfuzz.rawdex.formats.ContainsPoolIndex.PoolIndexKind;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Random;

/**
 * After the raw DEX file has been parsed, it is passed into this class
 * that represents the program in a mutatable form.
 * The class uses a CodeTranslator to translate between the raw DEX form
 * for a method, and the mutatable form. It also controls all CodeMutators,
 * deciding which ones should be applied to each CodeItem.
 */
public class Program {
  /**
   * The RNG used during mutation.
   */
  private Random rng;

  /**
   * The seed that was given to the RNG.
   */
  public long rngSeed;

  /**
   * The parsed raw DEX file.
   */
  private RawDexFile rawDexFile;

  /**
   * The system responsible for translating from CodeItems to MutatableCode and vice-versa.
   */
  private CodeTranslator translator;

  /**
   * Responsible for adding new class ID items, method ID items, etc.
   */
  private IdCreator idCreator;

  /**
   * A list of all the MutatableCode that the CodeTranslator produced from
   * CodeItems that are acceptable to mutate.
   */
  private List<MutatableCode> mutatableCodes;

  /**
   * A list of all MutatableCode items that were mutated when mutateTheProgram()
   * was called. updateRawDexFile() will update the relevant CodeItems when called,
   * and then clear this list.
   */
  private List<MutatableCode> mutatedCodes;

  /**
   * A list of all registered CodeMutators that this Program can use to mutate methods.
   */
  private List<CodeMutator> mutators;

  /**
   * Used if we're loading mutations from a file, so we can find the correct mutator.
   */
  private Map<Class<? extends CodeMutator>, CodeMutator> mutatorsLookupByClass;

  /**
   * Tracks mutation stats.
   */
  private MutationStats mutationStats;

  /**
   * A list of all mutations used for loading/dumping mutations from/to a file.
   */
  private List<Mutation> mutations;

  /**
   * The listener who is interested in events.
   * May be a listener that is responsible for multiple listeners.
   */
  private BaseListener listener;

  /**
   * Given a maximum number of mutations that can be performed on a method, n,
   * give up after attempting (n * this value) mutations for any method.
   */
  private static final int MAXIMUM_MUTATION_ATTEMPT_FACTOR = 10;

  /**
   * Construct the mutatable Program based on the raw DEX file that was parsed initially.
   */
  public Program(RawDexFile rawDexFile, List<Mutation> previousMutations,
      BaseListener listener) {
    this.listener = listener;

    idCreator = new IdCreator(rawDexFile);

    // Set up the RNG.
    rng = new Random();
    if (Options.usingProvidedSeed) {
      rng.setSeed(Options.rngSeed);
      rngSeed = Options.rngSeed;
    } else {
      long seed = System.currentTimeMillis();
      listener.handleSeed(seed);
      rng.setSeed(seed);
      rngSeed = seed;
    }

    if (previousMutations != null) {
      mutations = previousMutations;
    } else {
      // Allocate the mutations list.
      mutations = new ArrayList<Mutation>();

      // Read in the mutations if we need to.
      if (Options.loadMutations) {
        // Allocate the mutators lookup table.
        mutatorsLookupByClass = new HashMap<Class<? extends CodeMutator>, CodeMutator>();
        loadMutationsFromDisk(Options.loadMutationsFile);
      }
    }

    // Allocate the mutators list.
    mutators = new ArrayList<CodeMutator>();

    this.rawDexFile = rawDexFile;

    mutatableCodes = new ArrayList<MutatableCode>();
    mutatedCodes = new ArrayList<MutatableCode>();

    translator = new CodeTranslator();

    mutationStats = new MutationStats();

    // Register all the code mutators here.
    registerMutator(new ArithOpChanger(rng, mutationStats, mutations));
    registerMutator(new BranchShifter(rng, mutationStats, mutations));
    registerMutator(new CmpBiasChanger(rng, mutationStats, mutations));
    registerMutator(new ConstantValueChanger(rng, mutationStats, mutations));
    registerMutator(new ConversionRepeater(rng, mutationStats, mutations));
    registerMutator(new FieldFlagChanger(rng, mutationStats, mutations));
    registerMutator(new InstructionDeleter(rng, mutationStats, mutations));
    registerMutator(new InstructionDuplicator(rng, mutationStats, mutations));
    registerMutator(new InstructionSwapper(rng, mutationStats, mutations));
    registerMutator(new InvokeChanger(rng, mutationStats, mutations));
    registerMutator(new NewArrayLengthChanger(rng, mutationStats, mutations));
    registerMutator(new NewInstanceChanger(rng, mutationStats, mutations));
    registerMutator(new NewMethodCaller(rng, mutationStats, mutations));
    registerMutator(new NonsenseStringPrinter(rng, mutationStats, mutations));
    registerMutator(new OppositeBranchChanger(rng, mutationStats, mutations));
    registerMutator(new PoolIndexChanger(rng, mutationStats, mutations));
    registerMutator(new RandomBranchChanger(rng, mutationStats, mutations));
    registerMutator(new RandomInstructionGenerator(rng, mutationStats, mutations));
    registerMutator(new RegisterClobber(rng, mutationStats, mutations));
    registerMutator(new SwitchBranchShifter(rng, mutationStats, mutations));
    registerMutator(new TryBlockShifter(rng, mutationStats, mutations));
    registerMutator(new ValuePrinter(rng, mutationStats, mutations));
    registerMutator(new VRegChanger(rng, mutationStats, mutations));

    associateClassDefsAndClassData();
    associateCodeItemsWithMethodNames();

    int codeItemIdx = 0;
    for (CodeItem codeItem : rawDexFile.codeItems) {
      if (legalToMutate(codeItem)) {
        Log.debug("Legal to mutate code item " + codeItemIdx);
        int mutatableCodeIdx = mutatableCodes.size();
        mutatableCodes.add(translator.codeItemToMutatableCode(this, codeItem,
            codeItemIdx, mutatableCodeIdx));
      } else {
        Log.debug("Not legal to mutate code item " + codeItemIdx);
      }
      codeItemIdx++;
    }
  }

  private void registerMutator(CodeMutator mutator) {
    if (mutator.canBeTriggered()) {
      Log.debug("Registering mutator " + mutator.getClass().getSimpleName());
      mutators.add(mutator);
    }
    if (Options.loadMutations) {
      mutatorsLookupByClass.put(mutator.getClass(), mutator);
    }
  }

  /**
   * Associate ClassDefItem to a ClassDataItem and vice-versa.
   * This is so when we're associating method names with code items,
   * we can find the name of the class the method belongs to.
   */
  private void associateClassDefsAndClassData() {
    for (ClassDefItem classDefItem : rawDexFile.classDefs) {
      if (classDefItem.classDataOff.pointsToSomething()) {
        ClassDataItem classDataItem = (ClassDataItem)
            classDefItem.classDataOff.getPointedToItem();
        classDataItem.meta.classDefItem = classDefItem;
        classDefItem.meta.classDataItem = classDataItem;
      }
    }
  }

  /**
   * For each CodeItem, find the name of the method the item represents.
   * This is done to allow the filtering of mutating methods based on if
   * they have the name *_MUTATE, but also for debugging info.
   */
  private void associateCodeItemsWithMethodNames() {
    // Associate method names with codeItems.
    for (ClassDataItem classDataItem : rawDexFile.classDatas) {

      String className = "";
      if (classDataItem.meta.classDefItem != null) {
        int typeIdx = classDataItem.meta.classDefItem.classIdx;
        TypeIdItem typeIdItem = rawDexFile.typeIds.get(typeIdx);
        className = rawDexFile.stringDatas.get(typeIdItem.descriptorIdx).getString() + ".";
      }

      // Do direct methods...
      // Track the current method index with this value, since the encoding in
      // each EncodedMethod is the absolute index for the first EncodedMethod,
      // and then relative index for the rest...
      int methodIdx = 0;
      for (EncodedMethod method : classDataItem.directMethods) {
        methodIdx = associateMethod(method, methodIdx, className);
      }
      // Reset methodIdx for virtual methods...
      methodIdx = 0;
      for (EncodedMethod method : classDataItem.virtualMethods) {
        methodIdx = associateMethod(method, methodIdx, className);
      }
    }
  }

  /**
   * Associate the name of the provided method with its CodeItem, if it
   * has one.
   *
   * @param methodIdx The method index of the last EncodedMethod that was handled in this class.
   * @return The method index of the EncodedMethod that has just been handled in this class.
   */
  private int associateMethod(EncodedMethod method, int methodIdx, String className) {
    if (!method.codeOff.pointsToSomething()) {
      // This method doesn't have a code item, so we won't encounter it later.
      return methodIdx;
    }

    // First method index is an absolute index.
    // The rest are relative to the previous.
    // (so if methodIdx is initialised to 0, this single line works)
    methodIdx = methodIdx + method.methodIdxDiff;

    // Get the name.
    MethodIdItem methodIdItem = rawDexFile.methodIds.get(methodIdx);
    ProtoIdItem protoIdItem = rawDexFile.protoIds.get(methodIdItem.protoIdx);
    String shorty = rawDexFile.stringDatas.get(protoIdItem.shortyIdx).getString();
    String methodName = className
        + rawDexFile.stringDatas.get(methodIdItem.nameIdx).getString();

    // Get the codeItem.
    if (method.codeOff.getPointedToItem() instanceof CodeItem) {
      CodeItem codeItem = (CodeItem) method.codeOff.getPointedToItem();
      codeItem.meta.methodName = methodName;
      codeItem.meta.shorty = shorty;
      codeItem.meta.isStatic = method.isStatic();
    } else {
      Log.errorAndQuit("You've got an EncodedMethod that points to an Offsettable"
          + " that does not contain a CodeItem");
    }

    return methodIdx;
  }

  /**
   * Determine, based on the current options supplied to dexfuzz, as well as
   * its capabilities, if the provided CodeItem can be mutated.
   * @param codeItem The CodeItem we may wish to mutate.
   * @return If the CodeItem can be mutated.
   */
  private boolean legalToMutate(CodeItem codeItem) {
    if (!Options.mutateLimit) {
      Log.debug("Mutating everything.");
      return true;
    }
    if (codeItem.meta.methodName.endsWith("_MUTATE")) {
      Log.debug("Code item marked with _MUTATE.");
      return true;
    }
    Log.debug("Code item not marked with _MUTATE, but not mutating all code items.");
    return false;
  }

  private int getNumberOfMutationsToPerform() {
    // We want n mutations to be twice as likely as n+1 mutations.
    //
    // So if we have max 3,
    // then 0 has 8 chances ("tickets"),
    //      1 has 4 chances
    //      2 has 2 chances
    //  and 3 has 1 chance

    // Allocate the tickets
    // n mutations need (2^(n+1) - 1) tickets
    // e.g.
    // 3 mutations => 15 tickets
    // 4 mutations => 31 tickets
    int tickets = (2 << Options.methodMutations) - 1;

    // Pick the lucky ticket
    int luckyTicket = rng.nextInt(tickets);

    // The tickets are put into buckets with accordance with log-base-2.
    // have to make sure it's luckyTicket + 1, because log(0) is undefined
    // so:
    // log_2(1) => 0
    // log_2(2) => 1
    // log_2(3) => 1
    // log_2(4) => 2
    // log_2(5) => 2
    // log_2(6) => 2
    // log_2(7) => 2
    // log_2(8) => 3
    // ...
    // so to make the highest mutation value the rarest,
    //   subtract log_2(luckyTicket+1) from the maximum number
    // log2(x) <=> 31 - Integer.numberOfLeadingZeros(x)
    int luckyMutation = Options.methodMutations
        - (31 - Integer.numberOfLeadingZeros(luckyTicket + 1));

    return luckyMutation;
  }

  /**
   * Returns true if we completely failed to mutate this method's mutatable code after
   * attempting to.
   */
  private boolean mutateAMutatableCode(MutatableCode mutatableCode) {
    int mutations = getNumberOfMutationsToPerform();

    Log.info("Attempting " + mutations + " mutations for method " + mutatableCode.name);

    int mutationsApplied = 0;

    int maximumMutationAttempts = Options.methodMutations * MAXIMUM_MUTATION_ATTEMPT_FACTOR;
    int mutationAttempts = 0;
    boolean hadToBail = false;

    while (mutationsApplied < mutations) {
      int mutatorIdx = rng.nextInt(mutators.size());
      CodeMutator mutator = mutators.get(mutatorIdx);
      Log.info("Running mutator " + mutator.getClass().getSimpleName());
      if (mutator.attemptToMutate(mutatableCode)) {
        mutationsApplied++;
      }
      mutationAttempts++;
      if (mutationAttempts > maximumMutationAttempts) {
        Log.info("Bailing out on mutation for this method, tried too many times...");
        hadToBail = true;
        break;
      }
    }

    // If any of them actually mutated it, excellent!
    if (mutationsApplied > 0) {
      Log.info("Method was mutated.");
      mutatedCodes.add(mutatableCode);
    } else {
      Log.info("Method was not mutated.");
    }

    return ((mutationsApplied == 0) && hadToBail);
  }

  /**
   * Go through each mutatable method in turn, and attempt to mutate it.
   * Afterwards, call updateRawDexFile() to apply the results of mutation to the
   * original code.
   */
  public void mutateTheProgram() {
    if (Options.loadMutations) {
      applyMutationsFromList();
      return;
    }

    // Typically, this is 2 to 10...
    int methodsToMutate = Options.minMethods
        + rng.nextInt((Options.maxMethods - Options.minMethods) + 1);

    // Check we aren't trying to mutate more methods than we have.
    if (methodsToMutate > mutatableCodes.size()) {
      methodsToMutate = mutatableCodes.size();
    }

    // Check if we're going to end up mutating all the methods.
    if (methodsToMutate == mutatableCodes.size()) {
      // Just do them all in order.
      Log.info("Mutating all possible methods.");
      for (MutatableCode mutatableCode : mutatableCodes) {
        if (mutatableCode == null) {
          Log.errorAndQuit("Why do you have a null MutatableCode?");
        }
        mutateAMutatableCode(mutatableCode);
      }
      Log.info("Finished mutating all possible methods.");
    } else {
      // Pick them at random.
      Log.info("Randomly selecting " + methodsToMutate + " methods to mutate.");
      while (mutatedCodes.size() < methodsToMutate) {
        int randomMethodIdx = rng.nextInt(mutatableCodes.size());
        MutatableCode mutatableCode = mutatableCodes.get(randomMethodIdx);
        if (mutatableCode == null) {
          Log.errorAndQuit("Why do you have a null MutatableCode?");
        }
        if (!mutatedCodes.contains(mutatableCode)) {
          boolean completelyFailedToMutate = mutateAMutatableCode(mutatableCode);
          if (completelyFailedToMutate) {
            methodsToMutate--;
          }
        }
      }
      Log.info("Finished mutating the methods.");
    }

    listener.handleMutationStats(mutationStats.getStatsString());

    if (Options.dumpMutations) {
      writeMutationsToDisk(Options.dumpMutationsFile);
    }
  }

  private void writeMutationsToDisk(String fileName) {
    Log.debug("Writing mutations to disk.");
    try {
      BufferedWriter writer = new BufferedWriter(new FileWriter(fileName));
      for (Mutation mutation : mutations) {
        MutationSerializer.writeMutation(writer, mutation);
      }
      writer.close();
    } catch (IOException e) {
      Log.errorAndQuit("IOException while writing mutations to disk...");
    }
  }

  private void loadMutationsFromDisk(String fileName) {
    Log.debug("Loading mutations from disk.");
    try {
      BufferedReader reader = new BufferedReader(new FileReader(fileName));
      while (reader.ready()) {
        Mutation mutation = MutationSerializer.readMutation(reader);
        mutations.add(mutation);
      }
      reader.close();
    } catch (IOException e) {
      Log.errorAndQuit("IOException while loading mutations from disk...");
    }
  }

  private void applyMutationsFromList() {
    Log.info("Applying preloaded list of mutations...");
    for (Mutation mutation : mutations) {
      // Repopulate the MutatableCode field from the recorded index into the Program's list.
      mutation.mutatableCode = mutatableCodes.get(mutation.mutatableCodeIdx);

      // Get the right mutator.
      CodeMutator mutator = mutatorsLookupByClass.get(mutation.mutatorClass);

      // Apply the mutation.
      mutator.forceMutate(mutation);

      // Add this mutatable code to the list of mutated codes, if we haven't already.
      if (!mutatedCodes.contains(mutation.mutatableCode)) {
        mutatedCodes.add(mutation.mutatableCode);
      }
    }
    Log.info("...finished applying preloaded list of mutations.");
  }

  public List<Mutation> getMutations() {
    return mutations;
  }

  /**
   * Updates any CodeItems that need to be updated after mutation.
   */
  public boolean updateRawDexFile() {
    boolean anythingMutated = !(mutatedCodes.isEmpty());
    for (MutatableCode mutatedCode : mutatedCodes) {
      translator.mutatableCodeToCodeItem(rawDexFile.codeItems
          .get(mutatedCode.codeItemIdx), mutatedCode);
    }
    mutatedCodes.clear();
    return anythingMutated;
  }

  public void writeRawDexFile(DexRandomAccessFile file) throws IOException {
    rawDexFile.write(file);
  }

  public void updateRawDexFileHeader(DexRandomAccessFile file) throws IOException {
    rawDexFile.updateHeader(file);
  }

  /**
   * Used by the CodeMutators to determine legal index values.
   */
  public int getTotalPoolIndicesByKind(PoolIndexKind poolIndexKind) {
    switch (poolIndexKind) {
      case Type:
        return rawDexFile.typeIds.size();
      case Field:
        return rawDexFile.fieldIds.size();
      case String:
        return rawDexFile.stringIds.size();
      case Method:
        return rawDexFile.methodIds.size();
      case Invalid:
        return 0;
      default:
    }
    return 0;
  }

  /**
   * Used by the CodeMutators to lookup and/or create Ids.
   */
  public IdCreator getNewItemCreator() {
    return idCreator;
  }

  /**
   * Used by FieldFlagChanger, to find an EncodedField for a specified field in an insn,
   * if that field is actually defined in this DEX file. If not, null is returned.
   */
  public EncodedField getEncodedField(int fieldIdx) {
    if (fieldIdx >= rawDexFile.fieldIds.size()) {
      Log.debug(String.format("Field idx 0x%x specified is not defined in this DEX file.",
          fieldIdx));
      return null;
    }
    FieldIdItem fieldId = rawDexFile.fieldIds.get(fieldIdx);

    for (ClassDefItem classDef : rawDexFile.classDefs) {
      if (classDef.classIdx == fieldId.classIdx) {
        ClassDataItem classData = classDef.meta.classDataItem;
        return classData.getEncodedFieldWithIndex(fieldIdx);
      }
    }

    Log.debug(String.format("Field idx 0x%x specified is not defined in this DEX file.",
        fieldIdx));
    return null;
  }

  /**
   * Used to convert the type index into string format.
   * @param typeIdx
   * @return string format of type index.
   */
  public String getTypeString(int typeIdx) {
    TypeIdItem typeIdItem = rawDexFile.typeIds.get(typeIdx);
    return rawDexFile.stringDatas.get(typeIdItem.descriptorIdx).getString();
  }

  /**
   * Used to convert the method index into string format.
   * @param methodIdx
   * @return string format of method index.
   */
  public String getMethodString(int methodIdx) {
    MethodIdItem methodIdItem = rawDexFile.methodIds.get(methodIdx);
    return rawDexFile.stringDatas.get(methodIdItem.nameIdx).getString();
  }

  /**
   * Used to convert methodID to string format of method proto.
   * @param methodIdx
   * @return string format of shorty.
   */
  public String getMethodProto(int methodIdx) {
    MethodIdItem methodIdItem = rawDexFile.methodIds.get(methodIdx);
    ProtoIdItem protoIdItem = rawDexFile.protoIds.get(methodIdItem.protoIdx);

    if (!protoIdItem.parametersOff.pointsToSomething()) {
      return "()" + getTypeString(protoIdItem.returnTypeIdx);
    }

    TypeList typeList = (TypeList) protoIdItem.parametersOff.getPointedToItem();
    String typeItem = "(";
    for (int i= 0; i < typeList.size; i++) {
      typeItem = typeItem + typeList.list[i];
    }
    return typeItem + ")" + getTypeString(protoIdItem.returnTypeIdx);
  }
}