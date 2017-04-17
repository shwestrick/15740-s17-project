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
function primes(n) {
  if n < 2 then return {};
  let isPrime = [true : 0 ≤ i < n];
  foreach p in primes(√n) {
    foreach k where 2 ≤ k < ⌊n/p⌋ {
      isPrime[p·k] := false;
    }
  }
  return {i : 2 ≤ i < n | isPrime[i]};
}
```

This algorithm relies on multiple processors writing to the same flag at the
same time (write-write races), and is therefore not DRF. However, it does not
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
consists of _P_ dynamic streams of instructions, where _P_ is the number of processes.
An _**execution**_ is an interleaving of those _P_ streams, produced by some
scheduler. (At each time step, the scheduler lets one stream advance by a
single instruction.)

Two distinct instructions _A_ and _B_ are _**concurrent**_ if there exist two
executions _E<sub>1</sub>_ and _E<sub>2</sub>_ such that
 * _A_ appears before _B_ in _E<sub>1</sub>_, and
 * _B_ appears before _A_ in _E<sub>2</sub>_.

We distinguish between three classes of atomic memory operations: read, write,
and read-modify-write (such as compare-and-swap, fetch-and-add, etc).
We say that a multi-threaded program is
_**phase-concurrent**_ if all concurrent operations at the same memory location
are of the same class.

Under this definition, phase-concurrent programs must rely on read-modify-write
operations to synchronize effectively. For example, here is a phase-concurrent
program using two processes to sum an array `a` of length `2n` and
print the result.
Both processes run the same code. The value `p` is the
identifier of the process (in this example, either 0 or 1). Assume there is some
shared cell `s` initially set to 2. We use `r` to store intermediate results,
and the RMW operation sub-and-fetch (which atomically subtracts then fetches the
new value) to synchronize.

```
// initially s == 2
r[p] := 0;
for i where p·n ≤ i < (p+1)·n {
  r[p] := r[p] + a[i];
}
if (sub-and-fetch(s,1) == 0) {
  // exactly one process will execute this
  printf("%d\n", r[0] + r[1]);
}
```

Note that this program is actually DRF. We haven't yet discussed how to handle
concurrent writes.

### Concurrent Writes

When two or more processes concurrently write to the same location, we need to
specify what value will be returned at the next read. We propose
non-deterministically "choosing a winner".

Specifically, consider some execution _E_ and a read operation _r_ of interest.
Identify the write _w_ which occurred most recently before _r_, and let _W_ be
the set of all writes that are concurrent with _w_. Let _w<sub>0</sub>_ be the
member of _W_ which occurred earliest in this execution, and _p_ be the
identifier of the process which issued _w<sub>0</sub>_. Now identify the last
write in _W_ which was issued by process _p_. The value written by this write is
the value returned by _r_.

The above specification is a bit nasty, but (we claim) actually quite intuitive.
From the perspective of a process, at each write, it either "wins" or "loses".
If it wins, then all successive writes at the same location within the same
phase also win. Similarly, if it loses, then all successive writes at the same
location within the same phase also lose. At the end of a write phase, the
winning write is committed to memory and is visible to other processes.

For example, we can modify the array-sum example to print not only the sum of
the array, but also the identifier of the process which finished their portion
of the array first.

```
r[p] := 0;
for i where p·n ≤ i < (p+1)·n {
  r[p] := r[p] + a[i];
}
w := p;
if (sub-and-fetch(s,1) == 0) {
  printf("sum: %d\n", r[0] + r[1]);
  printf("winner: %d\n", w);
}
```

## References

<a name="fn1">1</a>:  _Choi, Byn, et al. "DeNovo: Rethinking the memory hierarchy for disciplined parallelism." Parallel Architectures and Compilation Techniques (PACT), 2011 International Conference on. IEEE, 2011._

<a name="fn2">2</a>: _Ros, Alberto, and Stefanos Kaxiras. "Complexity-effective multicore coherence." Proceedings of the 21st international conference on Parallel architectures and compilation techniques. ACM, 2012._

<a name="fn3">3</a>: _Shun, Julian, and Guy E. Blelloch. "Phase-concurrent hash tables for determinism." Proceedings of the 26th ACM Symposium on Parallelism in Algorithms and Architectures. ACM, 2014._
