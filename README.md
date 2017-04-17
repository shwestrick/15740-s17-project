# Cache Coherence for Phase-Concurrent Programs

By Anson Kahng, Charlie McGuffey, and Sam Westrick ({<tt>akahng</tt>,<tt>cmcguffe</tt>,<tt>swestric</tt>} at <tt>cs.cmu.edu</tt>).
Course Project for 15-740 Computer Architecture, Spring 2017.

## Introduction

In an effort to simplify the complexity and improve the scalability of cache
coherence schemes, some researchers have begun looking at the benefits of
restricted forms of parallelism. For example, it could be reasonable to assume
that parallel programs are *data-race free* (DRF). Under this restriction, it is
possible to obtain improved performance with a simplified
protocol.<sup>[1](#fn1),[2](#fn2)</sup>

However, while DRF programs are common, there are some programs which benefit
from "well-behaved" data races.  For example, consider the following algorithm
which generates all prime numbers less than `n` in parallel, based on the Sieve
of Eratosthenes.

```
function primes(n) =
  if n < 2 then return {};
  let isPrime = [true : 0 ≤ i < n];
  foreach p in primes(√n)
    foreach k where 2 ≤ k < ⌊n/p⌋
      isPrime[p·k] := false;
  return {i : 2 ≤ i < n | isPrime[i]};
```

This algorithm relies on multiple processors writing to the same flag at the
same time (write-write races) ,and is therefore not DRF. However, it does not
exhibit read-write races. In this sense its memory accesses are
_phase-concurrent_, meaning that memory cells alternate between concurrent read
and concurrent write phases. During a read phase, a cell is read-only. During a
write phase, it is write-only.

In the example algorithm above, there is an added
guarantee that concurrent writes to the same location will always be writing
the same value. In general, we will not require this, as will be discussed in
the next section.

The term _phase-concurrency_ is inspired by Shun and Blelloch's work
on concurrent hash tables.<sup>[3](#fn3)</sup>

## Definitions and Semantics

A _**program**_
consists of _P_ streams of instructions, where _P_ is the number of processes.
An _**execution**_ is an interleaving of those _P_ streams into one. Two distinct instructions _A_ and _B_ are _**concurrent**_ if there exist two
executions _E<sub>1</sub>_ and _E<sub>2</sub>_ such that
 * _A_ appears before _B_ in _E<sub>1</sub>_, and
 * _B_ appears before _A_ in _E<sub>2</sub>_.

We distinguish between three classes of atomic memory operations: read, write,
and read-modify-write (such as compare-and-swap, fetch-and-add, etc).
We say that a multi-threaded program is
_**phase-concurrent**_ if all concurrent operations at the same memory location
are of the same class.

## References

<a name="fn1">1</a>:  _Choi, Byn, et al. "DeNovo: Rethinking the memory hierarchy for disciplined parallelism." Parallel Architectures and Compilation Techniques (PACT), 2011 International Conference on. IEEE, 2011._

<a name="fn2">2</a>: _Ros, Alberto, and Stefanos Kaxiras. "Complexity-effective multicore coherence." Proceedings of the 21st international conference on Parallel architectures and compilation techniques. ACM, 2012._

<a name="fn3">3</a>: _Shun, Julian, and Guy E. Blelloch. "Phase-concurrent hash tables for determinism." Proceedings of the 26th ACM Symposium on Parallelism in Algorithms and Architectures. ACM, 2014._
