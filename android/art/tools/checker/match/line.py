# Copyright (C) 2014 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from common.logger              import Logger
from file_format.checker.struct import TestExpression, TestAssertion

import re

def headAndTail(list):
  return list[0], list[1:]

def splitAtSeparators(expressions):
  """ Splits a list of TestExpressions at separators. """
  splitExpressions = []
  wordStart = 0
  for index, expression in enumerate(expressions):
    if expression.variant == TestExpression.Variant.Separator:
      splitExpressions.append(expressions[wordStart:index])
      wordStart = index + 1
  splitExpressions.append(expressions[wordStart:])
  return splitExpressions

def getVariable(name, variables, pos):
  if name in variables:
    return variables[name]
  else:
    Logger.testFailed("Missing definition of variable \"{}\"".format(name), pos, variables)

def setVariable(name, value, variables, pos):
  if name not in variables:
    return variables.copyWith(name, value)
  else:
    Logger.testFailed("Multiple definitions of variable \"{}\"".format(name), pos, variables)

def matchWords(checkerWord, stringWord, variables, pos):
  """ Attempts to match a list of TestExpressions against a string.
      Returns updated variable dictionary if successful and None otherwise.
  """
  for expression in checkerWord:
    # If `expression` is a variable reference, replace it with the value.
    if expression.variant == TestExpression.Variant.VarRef:
      pattern = re.escape(getVariable(expression.name, variables, pos))
    else:
      pattern = expression.text

    # Match the expression's regex pattern against the remainder of the word.
    # Note: re.match will succeed only if matched from the beginning.
    match = re.match(pattern, stringWord)
    if not match:
      return None

    # If `expression` was a variable definition, set the variable's value.
    if expression.variant == TestExpression.Variant.VarDef:
      variables = setVariable(expression.name, stringWord[:match.end()], variables, pos)

    # Move cursor by deleting the matched characters.
    stringWord = stringWord[match.end():]

  # Make sure the entire word matched, i.e. `stringWord` is empty.
  if stringWord:
    return None

  return variables

def MatchLines(checkerLine, stringLine, variables):
  """ Attempts to match a CHECK line against a string. Returns variable state
      after the match if successful and None otherwise.
  """
  assert checkerLine.variant != TestAssertion.Variant.Eval

  checkerWords = splitAtSeparators(checkerLine.expressions)
  stringWords = stringLine.split()

  while checkerWords:
    # Get the next run of TestExpressions which must match one string word.
    checkerWord, checkerWords = headAndTail(checkerWords)

    # Keep reading words until a match is found.
    wordMatched = False
    while stringWords:
      stringWord, stringWords = headAndTail(stringWords)
      newVariables = matchWords(checkerWord, stringWord, variables, checkerLine)
      if newVariables is not None:
        wordMatched = True
        variables = newVariables
        break
    if not wordMatched:
      return None

  # All TestExpressions matched. Return new variable state.
  return variables

def getEvalText(expression, variables, pos):
  if expression.variant == TestExpression.Variant.PlainText:
    return expression.text
  else:
    assert expression.variant == TestExpression.Variant.VarRef
    return getVariable(expression.name, variables, pos)

def EvaluateLine(checkerLine, variables):
  assert checkerLine.variant == TestAssertion.Variant.Eval
  eval_string = "".join(map(lambda expr: getEvalText(expr, variables, checkerLine),
                            checkerLine.expressions))
  return eval(eval_string)
