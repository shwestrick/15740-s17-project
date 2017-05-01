# Cache Coherence for Phase-Concurrent Programs

By Anson Kahng, Charlie McGuffey, and Sam Westrick ({<tt>akahng</tt>,<tt>cmcguffe</tt>,<tt>swestric</tt>} at <tt>cs.cmu.edu</tt>).
Course Project for 15-740 Computer Architecture, Spring 2017.

## 1 Introduction

In an effort to simplify the complexity and improve the scalability of cache
coherence schemes, some researchers have begun looking at the benefits of
restricted forms of parallelism. For example, it could be reasonable to assume
that parallel programs are *data-race free* (DRF). Under this restriction, it is
possible to obtain improved performance with a simplified
protocol.<sup>[1](#choi-denovo),[2](#ros-complexity)</sup>

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

The term "phase-concurrent" is inspired by Shun and Blelloch's work
on concurrent hash tables.<sup>[3](#shun-hash)</sup>

## 2 Definitions and Semantics

A _**program**_
consists of _P_ dynamic streams of instructions, where _P_ is the number of processes.
An _**execution**_ is an interleaving of those _P_ streams, produced by some
scheduler. (At each time step, the scheduler lets one stream advance by a
single instruction.)

Two distinct instructions _A_ and _B_ are _**concurrent**_ if there exist two
executions _E<sub>1</sub>_ and _E<sub>2</sub>_ such that
  * _A_ appears before _B_ in _E<sub>1</sub>_, and
  * _B_ appears before _A_ in _E<sub>2</sub>_.

\[TODO: are these actually good definitions?\]

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

### 2.1 Concurrent Writes

When two or more processes concurrently write to the same location, we need to
specify what value will be returned at the next read. We propose
non-deterministically "choosing a winner".

Specifically, consider a particular execution and a read operation of interest.
Identify the write _w_ which occurred most recently before the read, and let _W_
be the set of all writes that are concurrent with _w_. Partition _W_ into sets
_W<sub>p</sub>_ containing only the writes issued by process _p_. For each of
these, identify the write _w<sub>p</sub>_ which is the last write in
_W<sub>p</sub>_. The winning write is chosen
non-deterministically from _{w<sub>p</sub> : 0 ≤ p < P}_.

The above specification is a bit nasty, but (we claim) actually quite intuitive.
For each memory location, at the end of each of its write phases, we
non-deterministically choose one written value to be visible in the following
read phase. The value which is chosen must be the "last" write of some process
within that phase.

For example, we can modify the array-sum example to print not only the sum of
the array, but also the identifier of the process which finished their portion
of the array first. To illustrate the requirement that the winning write be
the "last" write of some process, consider the assignment `w := 42` below.
In both processes, within the same write phase of `w`, that value is
overwritten. So it will never be visible.

```
r[p] := 0;
for i where p·n ≤ i < (p+1)·n {
  r[p] := r[p] + a[i];
}
w := 42; // this value will never be visible
w := p;
if (sub-and-fetch(s,1) == 0) {
  printf("sum: %d\n", r[0] + r[1]);
  printf("winner: %d\n", w);
}
```

## 3 Cache Coherence

One of the primary challenges in developing a cache coherence protocol for
phase-concurrent programs is managing winners and losers at writes. Some
existing techniques can be recycled here. For example, Choi et
al<sup>[1](#choi-denovo)</sup> proposed reusing the shared LLC as a directory,
allowing them to track the "owner" of a modified cache line with no asymptotic
space overhead. We will utilize a similar trick. **From now on, we will refer
to the shared LLC and the directory interchangeably.**

At the end of a write phase, we need to commit the winning write back to the
directory so that it is visible to other processes. Although we could rely
on programmer-directed cache self-invalidations, we would rather not do so.
Instead, we will conservatively guess that each synchronization instruction is
a barrier which signals the end of a phase.

### 3.1 Local Caches

Each line in an L1 cache can be in one of 7 states. We summarize them briefly
here.

  1. (**D**) Exclusive Dirty
  2. (**C**) Exclusive Clean
  3. (**W**) Winner
  4. (**S**) Shared
  5. (**O**) Old
  6. (**L**) Loser
  7. (**I**) Invalid

|               | D        | C        | W        | S        | O        | L        | I        |
|---------------|----------|----------|----------|----------|----------|----------|----------|
| dirty         | &#10003; |          | &#10003; |          |          |          |          |
| clean         |          | &#10003; |          | &#10003; | &#10003; |          |          |
| only copy     | &#10003; | &#10003; | &#10003; |          |          |          |          |
| shared copy   |          |          |          | &#10003; | &#10003; |          |          |
| no data       |          |          |          |          |          | &#10003; | &#10003; |


The inputs which are visible to the _i<sup>th</sup>_ L1 cache are
  1. (**Wr**) Write, issued by processor _i_.
  2. (**Re**) Read, issued by processor _i_.
  3. (**Ba**) Barrier, issued by processor _i_.
  4. (**Co**) Conflict, issued by the directory, indicating a write by some other processor _j_.
  5. (**Fo**) Forward, issued by the directory, indicating a read by some other
  processor _j_.
  6. (**Ev**) Eviction, issued by the _i<sup>th</sup>_ cache itself.

These produce the following transitions. Transitions marked "&#9785;" are an error,
"\-" are impossible, "\*" requires sending data to the directory, and "\*\*" requires both sending and receiving data from the directory. A
transition of the form _X_/_Y_ may go to either _X_ or _Y_, as dictated by the
directory.

|        |  D  |  C   |  W      |  S      |  O      |  L      |  I     |
|:------:|:---:|:----:|:-------:|:-------:|:-------:|:-------:|:------:|
| **Wr** |  D  |  D   |  W      |  W/L**  |  W/L**  |  L      |  D/L** |
| **Re** |  D  |  C   | &#9785; |  S      |  S      | &#9785; |  C/S** |
| **Ba** |  C  |  C   |  S*     |  O      |  I*     |  I      |  I     |
| **Co** |  W  |  L   |  W      |  -      |  -      |  -      |  -     |
| **Fo** |  -  |  S*  |  -      |  -      |  -      |  -      |  -     |
| **Ev** |  I* |  I*  |  I*     |  I*     |  I*     |  I      |  -     |

Transitions for L1 cache _i_:

|        |  D  |  C   |  W      |  S      |  O      |  L      |  I     |
|------|---|----|-------|-------|-------|-------|------|
| **Wr** |  D  |  D   |  W      |  Send **Ov<sub>i</sub>**; <br> W/L  |  Send **Ov<sub>i</sub>**; <br> W/L  |  L      |  Send **Wr<sub>i</sub>**; <br> W/L |
| **Re** |  D  |  C   | &#9785; |  S      |  S      | &#9785; |  Send **Re<sub>i</sub>**; <br> C/S |
| **Ba** |  C  |  C   |  Send **Sh<sub>i</sub>**; <br> S     |  O      |  Send **Fl<sub>i</sub>**; <br> I     |  I      |  I     |
| **Co** |  W  |  L   |  W      |  -      |  -      |  -      |  -     |
| **Fo** |  -  |  Send data; <br> S  |  -      |  -      |  -      |  -      |  -     |
| **Ev** |  Send **Fl<sub>i</sub>**; <br> I |  Send **Fl<sub>i</sub>**; <br> I  |  Send **Fl<sub>i</sub>**; <br> I     |  Send **Fl<sub>i</sub>**; <br> I     |  Send **Fl<sub>i</sub>**; <br> I     |  I      |  -     |

### 3.2 Directory

Each line in the directory can be in one of 3 states:
  1. (**R<sub>p</sub>**) Registered with _p_ (_0 ≤ p < P_)
  2. (**S<sub>k</sub>**) Shared amongst _k_ processors (_0 ≤ k ≤ P_)
  3. (**V**) Valid
  4. (**I**) Invalid

The inputs which are visible to the directory are the following, indexed by the processor id of the sender.
  1. (**Ov<sub>i</sub>**) Overwrite (processor _i_ is a sharer)
  2. (**Wr<sub>i</sub>**) Write (processor _i_ is not a sharer)
  3. (**Re<sub>i</sub>**) Read
  4. (**Sh<sub>i</sub>**) Share
  4. (**Fl<sub>i</sub>**) Flush

These produce the following transitions.

|                    | R<sub>p</sub> | S<sub>k</sub> | V | I |
|--------------------|---------------|---------------|---|---|
| **Ov<sub>i</sub>** | Send **Co** to _p_; <br> R<sub>p</sub> |
| **Wr<sub>i</sub>** | Send **Co** to _p_; <br> R<sub>p</sub>
| **Re<sub>i</sub>** | Send **Fo** to _p_; <br> Receive data from _p_; <br> Forward data to _i_; <br> S<sub>2</sub>
| **Sh<sub>i</sub>** | Assert _i_ = _p_; <br> S<sub>1</sub>
| **Fl<sub>i</sub>** | Assert _i_ = _p_; <br> Receive data from _p_; <br> V

## References

<a name="choi-denovo">1</a>:  _Choi, Byn, et al. "DeNovo: Rethinking the memory
hierarchy for disciplined parallelism." Parallel Architectures and Compilation
Techniques (PACT), 2011 International Conference on. IEEE, 2011._

<a name="ros-complexity">2</a>: _Ros, Alberto, and Stefanos Kaxiras.
"Complexity-effective multicore coherence." Proceedings of the 21st
international conference on Parallel architectures and compilation techniques.
ACM, 2012._

<a name="shun-hash">3</a>: _Shun, Julian, and Guy E. Blelloch. "Phase-concurrent
hash tables for determinism." Proceedings of the 26th ACM Symposium on
Parallelism in Algorithms and Architectures. ACM, 2014._
