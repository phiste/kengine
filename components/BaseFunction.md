# [BaseFunction](BaseFunction.hpp)

```cpp
template<typename Ret, typename ... Args>
struct BaseFunction<Ret(Args...)>;
```

Base type for "function `Components`" (see [README](../README.md) for more information about these). Function `Components` can simply inherit from `BaseFunction`, templated on the function signature.

## Members

### func

```cpp
putils::function<Ret(Args...), KENGINE_FUNCTION_MAX_SIZE> func = nullptr;
```

Functor to be called. Its maximum size defaults to 64, and can be changed by defining `KENGINE_FUNCTION_MAX_SIZE`.

### call

```cpp
Ret call(Args ... args) const noexcept;
Ret operator()(Args... args) const noexcept;
```

Calls the functor.