# __nocast vs __bitwise

`__nocast` warns about explicit or implicit casting to different types.
HOWEVER, it doesn't consider two 32-bit integers to be different
types, so a `__nocast int` type may be returned as a regular `int`
type and then the `__nocast` is lost.

So `__nocast` on integer types is usually not that powerful. It just
gets lost too easily. It's more useful for things like pointers. It
also doesn't warn about the mixing: you can add integers to `__nocast`
integer types, and it's not really considered anything wrong.

`__bitwise` ends up being a *stronger integer separation*. That one
doesn't allow you to mix with non-bitwise integers, so now it's much
harder to lose the type by mistake.

So the basic rule is:

 - `__nocast` on its own tends to be more useful for *big* integers
that still need to act like integers, but you want to make it much
less likely that they get truncated by mistake. So a 64-bit integer
that you don't want to mistakenly/silently be returned as `int`, for
example. But they mix well with random integer types, so you can add
to them etc without using anything special. However, that mixing also
means that the `__nocast` really gets lost fairly easily.

 - `__bitwise` is for *unique types* that cannot be mixed with other
types, and that you'd never want to just use as a random integer (the
integer `0` is special, though, and gets silently accepted - it's
kind of like `NULL` for pointers). So `gfp_t` or the `safe endianness`
types would be `__bitwise`: you can only operate on them by doing
specific operations that know about *that* particular type.

Generally, you want `__bitwise` if you are looking for type safety.
`__nocast` really is pretty weak.

## Reference:

* Linus' e-mail about `__nocast` vs `__bitwise`:

  <https://marc.info/?l=linux-mm&m=133245421127324&w=2>
