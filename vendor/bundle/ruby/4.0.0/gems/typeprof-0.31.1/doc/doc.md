# TypeProf: A type analysis tool for Ruby code based on abstract interpretation

## How to use TypeProf as a CLI tool

Analyze app.rb:

```
$ typeprof app.rb
```

Analyze app.rb with sig/app.rbs that specifies some method types:

```
$ typeprof sig/app.rbs app.rb
```

Here is a typical use case:

```
$ typeprof sig/app.rbs app.rb -o sig/app.gen.rbs
```

## How to use TypeProf as a Language Server

See [the slide deck of my talk in RubyKaigi 2024](https://speakerdeck.com/mame/good-first-issues-of-typeprof) for now.

## What is a TypeProf?

TypeProf is a Ruby interpreter that *abstractly* executes Ruby programs at the type level.
It executes a given program and observes what types are passed to and returned from methods and what types are assigned to instance variables.
All values are, in principle, abstracted to the class to which the object belongs, not the object itself (detailed in the next section).

Here is an example of a method call.

```
def foo(n)
  p n      #=> Integer
  n.to_s
end

p foo(42)  #=> String
```

The analysis results of TypeProf are as follows.

```
$ ruby exe/typeprof test.rb
# Revealed types
#  test.rb:2 #=> Integer
#  test.rb:6 #=> String

# Classes
class Object
  def foo : (Integer) -> String
end
```

When the method call `foo(42)` is executed, the type (abstract value) "`Integer`" is passed instead of the `Integer` object 42.
The method `foo` executes `n.to_s`.
Then, the built-in method `Integer#to_s` is called and you get the type "`String`", which the method `foo` returns.
Collecting observations of these execution results, TypeProf outputs, "the method `foo` receives `Integer` and returns `String`" in the RBS format.
Also, the argument of `p` is output in the `Revealed types` section.

Instance variables are stored in each object in Ruby, but are aggregated in class units in TypeProf.

```
class Foo
  def initialize
    @a = 42
  end

  attr_accessor :a
end

Foo.new.a = "str"

p Foo.new.a #=> Integer | String
```

```
$ ruby exe/typeprof test.rb
# Revealed types
#  test.rb:11 #=> Integer | String

# Classes
class Foo
  attr_accessor a : Integer | String
  def initialize : -> Integer
end
```


## Abstract values

As mentioned above, TypeProf abstracts almost all Ruby values to the type level, with some exceptions like class objects.
To avoid confusion with normal Ruby values, we use the word "abstract value" to refer the values that TypeProf handles.

TypeProf handles the following abstract values.

* Instance of a class
* Class object
* Symbol
* `untyped`
* Union of abstract values
* Instance of a container class
* Proc object

Instances of classes are the most common values.
A Ruby code `Foo.new` returns an instance of the class `Foo`.
This abstract value is represented as `Foo` in the RBS format, though it is a bit confusing.
The integer literal `42` generates an instance of `Integer` and the string literal `"str"` generates an instance of `String`.

A class object is a value that represents the class itself.
For example, the constants `Integer` and `String` has class objects.
In Ruby semantics, a class object is an instance of the class `Class`, but it is not abstracted into `Class` in TypeProf.
This is because, if it is abstracted, TypeProf cannot handle constant references and class methods correctly.

A symbol is an abstract value returned by Symbol literals like `:foo`.
A symbol object is not abstracted to an instance of the class `Symbol` because its concrete value is often required in many cases, such as keyword arguments, JSON data keys, the argument of `Module#attr_reader`, etc.
Note that some Symbol objects are handled as instances of the class `Symbol`, for example, the return value of `String#to_sym` and Symbol literals that contains interpolation like `:"foo_#{ x }"`.

`untyped` is an abstract value generated when TypeProf fails to trace values due to analysis limits or restrictions.
Any operations and method calls on `untyped` are ignored, and the evaluation result is also `untyped`.

A union of abstract values is a value that represents multiple possibilities.,
For (a bit artificial) example, the result of `rand < 0.5 ? 42 : "str"` is a union, `Integer | String`.

An instance of a container class, such as Array and Hash, is an object that contains other abstract values as elements.
At present, only Array, Enumerator and Hash are supported.
Details will be described later.

A Proc object is a closure produced by lambda expressions (`-> {... }`) and block parameters (`&blk`).
During the interpretation, these objects are not abstracted but treated as concrete values associated with a piece of code.
In the RBS result, they are represented by using anonymous proc type, whose types they accepted and returned.

TODO: write more