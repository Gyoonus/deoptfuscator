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

package dexfuzz.program.mutators;

import dexfuzz.Log;
import dexfuzz.MutationStats;
import dexfuzz.Options;
import dexfuzz.program.MutatableCode;
import dexfuzz.program.Mutation;

import java.util.List;
import java.util.Random;

/**
 * The base class for all classes that can mutate methods.
 */
public abstract class CodeMutator {
  /**
   * The RNG, passed in by the Program that initialised us.
   */
  protected Random rng;

  /**
   * Used to track which mutations happen.
   */
  protected MutationStats stats;

  /**
   * Used to track mutations that have been applied so far.
   */
  protected List<Mutation> mutations;

  /**
   * The chance, out of 100, that this mutator actually mutates the the program
   * when asked to by the Program. The default is 50% chance, but each mutator that
   * extends CodeMutator should its own default.
   */
  protected int likelihood = 50;

  /**
   * This constructor is only intended for use in MutationRecorder...
   */
  public CodeMutator() {

  }

  /**
   * Constructor that all subclasses must call...
   *
   * @param rng The RNG that the Program created.
   */
  public CodeMutator(Random rng, MutationStats stats, List<Mutation> mutations) {
    this.rng = rng;
    this.stats = stats;
    this.mutations = mutations;

    String name = this.getClass().getSimpleName().toLowerCase();

    if (Options.mutationLikelihoods.containsKey(name)) {
      likelihood = Options.mutationLikelihoods.get(name);
      Log.info("Set mutation likelihood to " + likelihood
          + "% for " + this.getClass().getSimpleName());
    }
  }

  /**
   * When the Program picks a particular mutator to mutate the code, it calls
   * this function, that determines if the mutator will actually mutate the code.
   * If so, it then calls the mutationFunction() method, that every subclass CodeMutator
   * is expected to implement to perform its mutation.
   *
   * @return If mutation took place.
   */
  public boolean attemptToMutate(MutatableCode mutatableCode) {
    if (shouldMutate(mutatableCode)) {
      generateAndApplyMutation(mutatableCode);
      return true;
    }
    Log.info("Skipping mutation.");
    return false;
  }

  public void forceMutate(Mutation mutation) {
    Log.info("Forcing mutation.");
    applyMutation(mutation);
  }

  public boolean canBeTriggered() {
    return (likelihood > 0);
  }

  /**
   * Randomly determine if the mutator will actually mutate a method, based on its
   * provided likelihood of mutation.
   *
   * @return If the method should be mutated.
   */
  private boolean shouldMutate(MutatableCode mutatableCode) {
    return ((rng.nextInt(100) < likelihood) && canMutate(mutatableCode));
  }

  private void generateAndApplyMutation(MutatableCode mutatableCode) {
    Mutation mutation = generateMutation(mutatableCode);
    // Always save the mutation.
    mutations.add(mutation);
    applyMutation(mutation);
  }

  /**
   * A CodeMutator must override this method if there is any reason why could not mutate
   * a particular method, and return false in that case.
   */
  protected boolean canMutate(MutatableCode mutatableCode) {
    return true;
  }

  protected abstract Mutation generateMutation(MutatableCode mutatableCode);

  protected abstract void applyMutation(Mutation uncastMutation);

  public abstract Mutation getNewMutation();
}
