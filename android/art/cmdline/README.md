Cmdline
===================

Introduction
-------------
This directory contains the classes that do common command line tool initialization and parsing. The
long term goal is eventually for all `art` command-line tools to be using these helpers.

----------


## Cmdline Parser
-------------

The `CmdlineParser` class provides a fluent interface using a domain-specific language to quickly
generate a type-safe value parser that process a user-provided list of strings (`argv`). Currently,
it can parse a string into a `VariantMap`, although in the future it might be desirable to parse
into any struct of any field.

To use, create a `CmdlineParser::Builder` and then chain the `Define` methods together with
`WithType` and `IntoXX` methods.

### Quick Start
For example, to save the values into a user-defined variant map:

```
struct FruitVariantMap : VariantMap {
  static const Key<int> Apple;
  static const Key<double> Orange;
  static const Key<bool> Help;
};
// Note that some template boilerplate has been avoided for clarity.
// See variant_map_test.cc for how to completely define a custom map.

using FruitParser = CmdlineParser<FruitVariantMap, FruitVariantMap::Key>;

FruitParser MakeParser() {
  auto&& builder = FruitParser::Builder();
  builder.
   .Define("--help")
      .IntoKey(FruitVariantMap::Help)
    Define("--apple:_")
      .WithType<int>()
      .IntoKey(FruitVariantMap::Apple)
   .Define("--orange:_")
      .WithType<double>()
      .WithRange(0.0, 1.0)
      .IntoKey(FruitVariantMap::Orange);

  return builder.Build();
}

int main(char** argv, int argc) {
  auto parser = MakeParser();
  auto result = parser.parse(argv, argc));
  if (result.isError()) {
     std::cerr << result.getMessage() << std::endl;
     return EXIT_FAILURE;
  }
  auto map = parser.GetArgumentsMap();
  std::cout << "Help? " << map.GetOrDefault(FruitVariantMap::Help) << std::endl;
  std::cout << "Apple? " << map.GetOrDefault(FruitVariantMap::Apple) << std::endl;
  std::cout << "Orange? " << map.GetOrDefault(FruitVariantMap::Orange) << std::endl;

  return EXIT_SUCCESS;
}
```

In the above code sample, we define a parser which is capable of parsing something like `--help
--apple:123 --orange:0.456` . It will error out automatically if invalid flags are given, or if the
appropriate flags are given but of the the wrong type/range. So for example, `--foo` will not parse
(invalid argument), neither will `--apple:fruit` (fruit is not an int) nor `--orange:1234` (1234 is
out of range of [0.0, 1.0])

### Argument Definitions in Detail
#### Define method
The 'Define' method takes one or more aliases for the argument. Common examples might be `{"-h",
"--help"}` where both `--help` and `-h` are aliases for the same argument.

The simplest kind of argument just tests for presence, but we often want to parse out a particular
type of value (such as an int or double as in the above `FruitVariantMap` example). To do that, a
_wildcard_ must be used to denote the location within the token that the type will be parsed out of.

For example with `-orange:_` the parse would know to check all tokens in an `argv` list for the
`-orange:` prefix and then strip it, leaving only the remains to be parsed.

#### WithType method (optional)
After an argument definition is provided, the parser builder needs to know what type the argument
will be in order to provide the type safety and make sure the rest of the argument definition is
correct as early as possible (in essence, everything but the parsing of the argument name is done at
compile time).

Everything that follows a `WithType<T>()` call is thus type checked to only take `T` values.

If this call is omitted, the parser generator assumes you are building a `Unit` type (i.e. an
argument that only cares about presence).

#### WithRange method (optional)
Some values will not make sense outside of a `[min, max]` range, so this is an option to quickly add
a range check without writing custom code. The range check is performed after the main parsing
happens and happens for any type implementing the `<=` operators.

#### WithValueMap (optional)
When parsing an enumeration, it might be very convenient to map a list of possible argument string
values into its runtime value.

With something like
```
    .Define("-hello:_")
      .WithValueMap({"world", kWorld},
                    {"galaxy", kGalaxy})
```
It will parse either `-hello:world` or `-hello:galaxy` only (and error out on other variations of
`-hello:whatever`), converting it to the type-safe value of `kWorld` or `kGalaxy` respectively.

This is meant to be another shorthand (like `WithRange`) to avoid writing a custom type parser. In
general it takes a variadic number of `pair<const char* /*arg name*/, T /*value*/>`.

#### WithValues (optional)
When an argument definition has multiple aliases with no wildcards, it might be convenient to
quickly map them into discrete values.

For example:
```
  .Define({"-xinterpret", "-xnointerpret"})
    .WithValues({true, false}
```
It will parse `-xinterpret` as `true` and `-xnointerpret` as `false`.

In general, it uses the position of the argument alias to map into the WithValues position value.

(Note that this method will not work when the argument definitions have a wildcard because there is
no way to position-ally match that).

#### AppendValues (optional)
By default, the argument is assumed to appear exactly once, and if the user specifies it more than
once, only the latest value is taken into account (and all previous occurrences of the argument are
ignored).

In some situations, we may want to accumulate the argument values instead of discarding the previous
ones.

For example
```
  .Define("-D")
     .WithType<std::vector<std::string>)()
     .AppendValues()
```
Will parse something like `-Dhello -Dworld -Dbar -Dbaz` into `std::vector<std::string>{"hello",
"world", "bar", "baz"}`.

### Setting an argument parse target (required)
To complete an argument definition, the parser generator also needs to know where to save values.
Currently, only `IntoKey` is supported, but that may change in the future.

#### IntoKey (required)
This specifies that when a value is parsed, it will get saved into a variant map using the specific
key.

For example,
```
   .Define("-help")
     .IntoKey(Map::Help)
```
will save occurrences of the `-help` argument by doing a `Map.Set(Map::Help, ParsedValue("-help"))`
where `ParsedValue` is an imaginary function that parses the `-help` argment into a specific type
set by `WithType`.

### Ignoring unknown arguments
This is highly discouraged, but for compatibility with `JNI` which allows argument ignores, there is
an option to ignore any argument tokens that are not known to the parser. This is done with the
`Ignore` function which takes a list of argument definition names.

It's semantically equivalent to making a series of argument definitions that map to `Unit` but don't
get saved anywhere. Values will still get parsed as normal, so it will *not* ignore known arguments
with invalid values, only user-arguments for which it could not find a matching argument definition.

### Parsing custom types
Any type can be parsed from a string by specializing the `CmdlineType` class and implementing the
static interface provided by `CmdlineTypeParser`. It is recommended to inherit from
`CmdlineTypeParser` since it already provides default implementations for every method.

The `Parse` method should be implemented for most types. Some types will allow appending (such as an
`std::vector<std::string>` and are meant to be used with `AppendValues` in which case the
`ParseAndAppend` function should be implemented.

For example:
```
template <>
struct CmdlineType<double> : CmdlineTypeParser<double> {
  Result Parse(const std::string& str) {
    char* end = nullptr;
    errno = 0;
    double value = strtod(str.c_str(), &end);

    if (*end != '\0') {
      return Result::Failure("Failed to parse double from " + str);
    }
    if (errno == ERANGE) {
      return Result::OutOfRange(
          "Failed to parse double from " + str + "; overflow/underflow occurred");
    }

    return Result::Success(value);
  }

  static const char* Name() { return "double"; }
  // note: Name() is just here for more user-friendly errors,
  // but in the future we will use non-standard ways of getting the type name
  // at compile-time and this will no longer be required
};
```
Will parse any non-append argument definitions with a type of `double`.

For an appending example:
```
template <>
struct CmdlineType<std::vector<std::string>> : CmdlineTypeParser<std::vector<std::string>> {
  Result ParseAndAppend(const std::string& args,
                        std::vector<std::string>& existing_value) {
    existing_value.push_back(args);
    return Result::SuccessNoValue();
  }
  static const char* Name() { return "std::vector<std::string>"; }
};
```
Will parse multiple instances of the same argument repeatedly into the `existing_value` (which will
be default-constructed to `T{}` for the first occurrence of the argument).

#### What is a `Result`?
`Result` is a typedef for `CmdlineParseResult<T>` and it acts similar to a poor version of
`Either<Left, Right>` in Haskell. In particular, it would be similar to `Either< int ErrorCode,
Maybe<T> >`.

There are helpers like `Result::Success(value)`, `Result::Failure(string message)` and so on to
quickly construct these without caring about the type.

When successfully parsing a single value, `Result::Success(value)` should be used, and when
successfully parsing an appended value, use `Result::SuccessNoValue()` and write back the new value
into `existing_value` as an out-parameter.

When many arguments are parsed, the result is collapsed down to a `CmdlineResult` which acts as a
`Either<int ErrorCode, Unit>` where the right side simply indicates success. When values are
successfully stored, the parser will automatically save it into the target destination as a side
effect.
