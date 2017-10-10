# The Fleece Mutability Library

## Introduction
This is an optional component of Fleece that addresses two concerns with integrating Fleece into an application or framework:

* **Mutability:**The Fleece API uses the raw encoded data directly, instead of translating it into transient objects the way JSON or XML APIs do. Fleece object references are just pointers into the data. This makes the API very lightweight, but also means that it’s impractical for the objects to be mutable: changing a value is very likely to change its size, which means pushing aside all the following data, invalidating its pointers.

* **Integration with native object classes:** If Fleece is being used as part of a framework (like Couchbase Lite), then the values it stores should be exposed as objects in the framework being used by the developer.  This isn’t hard on its own, though it’s annoying boilerplate. But it gets quite difficult when combined with mutability, as we’ve discovered while implementing Couchbase Lite 2.

## Architecture
There are three core classes in the mutability library: `MValue`, `MArray`, `MDict`. These are actually C++ templates, parameterized by a type `Native` that represents a pointer to a native (framework) object, like an Objective-C id or JNI jobject, etc.

* An `MValue` is a mutable “slot” that holds a Fleece Value, a `Native` object, or both. (In the “both” state, the `Native` object is a cached representation of the Value. If the slot is mutated, the Value is cleared, leaving only the current `Native` object.)
* An `MArray` is a mutable array of `MValue`s. It usually shadows a Fleece `Array`.
* An `MDict` is a mutable dictionary of `MValue`s. It usually shadows a Fleece `Dict`.

Both `MArray` and `MDict` are implemented such that they can be constructed from a Fleece `Array`/`Dict` cheaply in constant time, without having to copy data.

There’s also an MRoot class, which acts as a special top-level container for the root object of a Fleece document. It’s used to create the root object, but isn’t needed after that.

## Implementing the native side

Adapting the mutability library to a new framework is fairly straightforward; here are the steps:

### 1. Create an appropriate `Native` type
The `Native` type declared by the templates needs to be a ‘smart pointer’ that holds a strong reference to a native object; that is, the reference keeps the object in memory. In most cases this means you’ll need to implement a pointer-sized class with the right constructors and destructor and `operator=` overloads to manage the strong reference.

(In the specific case of Objective-C, this happens to be taken care of automatically by the compiler’s ARC (Automatic Ref-Counting) feature; the `Native` type used is simply `id`.)

### 2. Implement a factory method to create a `Native` from a Value
The `MValue` class template has three methods that are declared but not implemented. That means you have to provide your own implementations customized for your `Native` type.

The first of these is:
```
        static Native toNative(MValue *mv, MCollection*parent, bool &cacheIt);
```

The implementation should look at the Value in `mv` (`mv->value()`), create an equivalent native object, and return it. If the native object should be cached in memory to speed up the next call, set `cacheIt` to `true`.

If the Value is an `Array` or `Dict`, this method needs to return an appropriate custom collection class that can shadow that Value, using the provided `MArray` / `MDict` classes to do most of the work. Those classes’ constructors need a reference to the `MValue` and its parent collection (`MArray` or `MDict`), which is why those are passed to the `toNative` method. Speaking of those classes…

### 3. Implement native array and dictionary classes that wrap `MArray` and `MDict`
These classes can subclass standard collection classes, or be your own classes with a custom API; it doesn’t matter. These will be the classes that the application programmer interacts with.

An instance of your array class needs to contain (have a 1::1 relationship with) an `MArray`. The `MArray` is initialized with the `MValue` representing the array, and also its parent container (if any). In return, you can call the `MArray` to get and set elements, get its size, iterate it, etc.

(The same goes for your dictionary class.)

### 4. Implement an accessor to get a native array/dictionary’s `MArray`/`MDict` object
Fleece needs to be able to traverse from an `MValue` object to the `MArray` or `MDict` object representing its contents. But for efficiency reasons, that connection is only known to the `Native` object, which is opaque to Fleece. So you need to provide a method that traverses that link:

```
static MCollection* collectionFromNative(Native);
```

MCollection is the abstract superclass of `MArray` and `MDict`. So this method is simple: just cast the `Native` reference to the appropriate collection class you’ve implemented, get that object’s `MArray` or `MDict`, and return a pointer to it.

### 5. Implement an encoder for `Native` objects
```
static void encodeNative(Encoder&, Native);
```

This method just needs to encode the `Native` object by writing a single Value (which may of course be an `Array` or `Dict`) to the Encoder.