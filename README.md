tyre
====

`tyre` is generic TYpe erasuRE for c++.

`tyre` is a tiny (400 LOC) header-only C++17 library with no dependencies except the standard library.

`tyre` provides a single new abstraction: `tyre::any`.

`tyre::any` is built upon the `std::any` abstraction already available in C++17, and fills a gap in the standard's current provision for type erasure abstractions.


Motivation
----------

`std::variant` can contain an object of any one of a fixed number of types specified at design time. The contained object can be operated upon by arbitrary visitor functions.

`std::any` can contain an object of arbitrary type. The contained object can not easily by operated upon by visitor functions. One must use `std::any_cast` to access a contained object of a type specified at design time.

`tyre::any` can contain an object of arbitrary type that satisfies some implicit constraints. The object can be operated upon by a fixed set of visitor functions specified at design time. It is a general purpose duck-typed polymorphic value wrapper.


Usage
-----

The repository contains a [simple example](example.cpp) of usage.

`tyre::any` is a class template with a single type template parameter. Its template argument `VP` is a policy class that describes the set of visitor functions that can be used on contained objects. `Vs` must be a class with a single static constexpr data member `visitors` of type instantiated from the class template `tyre::visitor_list`. The constructor of `tyre::visitor_list` accepts an arbitrary number of arguments that result from calling the function template `tyre::visitor`.

Once instantiated with a template argument, `tyre::any<VP>` supports a superset of the API of `std::any`.

Visitor functions can be invoked by the non-member function template `tyre::visit`.


Supported compilers
-------------------

Minimum versions required:

- GCC 7.1
- Clang 5.0 (6.0 if using `-stdlib=libc++`)
- MSVC 14.2 (VS 2019 16.0; `cl.exe` 19.20)


Efficiency
----------

The size of `tyre::any` is the size of one `std::any` plus the size of one pointer, plus possible padding.

Where the API of `tyre::any` models that of `std::any`, the efficiency guarantees and noexcept specifications are equivalent to those of `std::any`. The overhead when invoking a visitor function is the cost of one pointer indirection and an invocation of a [P0792](https://wg21.link/P0792) `std::function_ref`, plus the cost of one `any_cast` for each `tyre::any` argument.


Prior art
---------

These libraries implement essentially the same idea:

- [Boost.TypeErasure](https://www.boost.org/doc/libs/1_70_0/doc/html/boost_typeerasure.html)
- [Boost.dynamic_any](http://cpp-experiment.sourceforge.net/boost/libs/dynamic_any/doc/)
- [Adobe Poly](http://stlab.adobe.com/group__poly__related.html)

The standard library already contains some special cases of this pattern. For example, `std::function` supports exactly one visitor function, which is invoked using its function call operator.

Qt's [meta type](https://doc.qt.io/qt-5/qmetatype.html) system supports similar functionality, but requires all types to be registered with a global registry object at runtime.

The `std::polymorphic_value` [proposal](http://wg21.link/p0201) solves a similar problem, but requires that the allowable types of the contained object all share a common public base class.


Reference
---------

### `tyre::any`

```cpp
// Synopsis
template <typename VP>
class any
{
public:
    // See https://en.cppreference.com/w/cpp/utility/any
    // or http://eel.is/c++draft/any

private:
    // Exposition only
    std::any object;
    void const* concrete_visitors;
};
```

Requires: `VP` be a class with a single static constexpr data member `visitors` of type instantiated from `tyre::visitor_list`. This shall describe the set of visitor functions that can be used with this instantiation.

The templated constructors, assignment operator, and `emplace` member functions do not participate in overload resolution unless:

1. The corresponding member of `std::any` would participate in overload resolution, and;
2. `T` is not an instantiation of `tyre::any`, and;
3. All the visitor functions are callable for the argument types and return type of their function signatures, except that `T` is substituted for arguments of type `std::any`.

Duplicates in all other respects the API of `std::any`.

### `tyre::visitor_list`

A literal type describing a set of visitor functions.

```cpp
// Synopsis
template <typename... Vs>
class visitor_list
{
public:
    constexpr visitor_list(Vs... visitors);
};
```

Requires: `Vs` be a pack of types returned by `tyre::visitor`. Their types can be deduced by class template argument deduction.

### `tyre::visitor`

Returns an object describing a visitor function.

```cpp
// Synopsis
template <typename Tag, typename Sig, typename Func>
constexpr auto visitor(Func function);
```

`Func` is typically deduced, while `Tag` and `Sig` must be explicitly specified.

Requires: `Tag` is a tag type that uniquely identifies the visitor.

Requires: `Sig` is a function type that describes the function signature of the visitor, using `std::any` as a placeholder for the type of the contained object(s).

Requires: `Func` is a literal, callable type that conforms to the signature of `Sig`, except that the type of the contained object will be substituted for the `std::any` parameter(s). _[Note: This contains the business logic of the visitor. Typically, a generic lambda.]_ A violation of this rule is not required to be diagnosed unless the program calls a templated constructor, assignment operator, or `emplace` member function of a corresponding instantiation of `tyre::any`.

Returns: a visitor description object which can be supplied as an argument to the constructor of `tyre::visitor_list`.

### `tyre::visit`

Invokes the visitor function corresponding to the tag type `Tag`, with the supplied arguments.

```cpp
// Synopsis
template <typename Tag, typename... Ts>
decltype(auto) visit(Ts&&... args);
```

Requires: at least one of `args` must be an instantiation of `tyre::any`, and if there are more than one then they must all have the same `VP` template argument and contain the same runtime type. The implementation will substitute the contained object(s) for these arguments in the call to the visitor function.

Returns: the return value of the visitor (which may be void). If the visitor returns `std::any`, it is assumed to contain the same type as the `tyre::any` argument(s), and a `tyre::any` is returned.

Throws: `std::bad_any_cast` if the contained objects do not all have the same type.

### `tyre::make_any`

Constructs a `tyre::any` in the same manner that `std::make_any` constructs a `std::any`.

```cpp
// Synopsis
template <typename VP, typename T, typename... Ts>
any<VP> make_any(Ts&&... args);
template <typename VP, typename T, typename U, typename... Ts>
any<VP> make_any(std::initializer_list<U> il, Ts&&... args);
```

Differs from `std::make_any` only in that the template argument for `VP` is required to be explicitly specified in addition to `T`. _[Note: The call is ill-formed if there is no matching overload for the corresponding in-place constructor of `tyre::any`.]_
