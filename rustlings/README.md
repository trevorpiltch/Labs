# Rustlings

## ABOUT
The [rustlings](https://github.com/rust-lang/rustlings) exercise set: a few hundred small Rust
programs that don't compile (or don't do what they claim), each one fixed by applying a single
concept from [The Book](https://doc.rust-lang.org/book/). Exercises live in `exercises/`, the
reference answers in `solutions/`, and `.rustlings-state.txt` tracks how far I've gotten.

## MOTIVATION
I wanted ownership, borrowing, and lifetimes to be reflexes rather than things I look up. Reading
about the borrow checker doesn't teach you much; arguing with it does. This is the compiler-as-tutor
approach — every exercise is a failure I have to explain before I can move on.

## BUILDING
Needs a Rust toolchain and the `rustlings` CLI:

```
cargo install rustlings
```

Then from this directory:

```
rustlings          # interactive watch mode, walks the next unsolved exercise
rustlings run <name>   # run one exercise, e.g. `rustlings run intro1`
rustlings hint <name>  # hint for a specific exercise
```

`target/` is build output and is ignored.
