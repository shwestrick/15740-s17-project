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
    foreach k where 2 ≤ k < ⌈n/p⌉ {
      isPrime[p·k] := false;
    }
  }
  return {i : 2 ≤ i < n | isPrime[i]};
}
```

This algorithm relies on multiple processors writing to the same flag at the
same time (write-write races), and is therefore not DRF. However, it does not
exhibit read-write races. In this sense its memory locations are
_phase-concurrent_, meaning that they each alternate between concurrent read and
concurrent write phases. During a location's read phase, it is read-only. During
a write phase, it is write-only. Note that phases are not global: it is possible
for a process to be reading from one cell while writing to another without
synchronizing in between.

In the example algorithm above, there is an added
guarantee that concurrent writes to the same location will always be writing
the same value. In general, we will not require this, as will be discussed in
more detail later.

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
to the shared LLC and the directory interchangeably.** Note that this approach
requires inclusive caching: every line in an L1 cache must also be in the
shared LLC.

At the end of a write phase, we need to commit the winning write back to the
directory so that it is visible to other processes. Although we could rely
on programmer-directed cache self-invalidations, we would rather not do so.
Instead, we will conservatively guess that each synchronization instruction is
a barrier which signals the end of a phase.

### 3.1 Local Caches

Each line in an L1 cache can be in one of 7 states. We summarize them briefly
here.

  1. (**D**) **Exclusive Dirty**: this is the only valid copy of the line in the
  system, and it is dirty. No other processors have tried to concurrently write
  to (or read from) this line.
  2. (**C**) **Exclusive Clean**: this is the only valid copy of the line in the
  system, and it is clean. No other processors have tried to concurrently write
  to (or read from) this line.
  3. (**W**) **Winner**: this is the only valid copy of the line in the system, and it
  is dirty. At least one other processor has tried to concurrently write to this
  line.
  4. (**S**) **Shared**: this is one of (possibly) many valid copies of the line in
  the system.
  5. (**O**) **Old**: this is one of (possibly) many copies of the line in the
  system, but it may be invalid. Unless the data is refreshed by a read, this
  line needs to be self-invalidated at the next synchronization.
  6. (**L**) **Loser**: this line is invalid due to some other processor having
  concurrently written to this line.
  7. (**I**) **Invalid**: this line is not in the cache.

For a particular line in an L1 cache, the events which cause
state transitions are as follows.
  1. (**Wr**) **Write**: a write to this line, issued by the local processor.
  2. (**Re**) **Read**: a read of this line, issued by the local processor.
  3. (**Ba**) **Barrier**: a read-modify-write of *any* line, issued by the
  local processor.
  4. (**Co**) **Conflict**: a message from the directory indicating a write to
  this line by some other processor.
  5. (**Fo**) **Forward**: a message from the directory indicating a read of
  this line by some other processor. The cache has to respond to this directory by
  sending the contents of the line.
  6. (**Ev**) **Eviction**: an eviction of this line due to the local caching
  policy.

### 3.2 Directory

Each line in the directory can be in one of 5 states. The basic idea is to
keep track of the status of the owner if possible.
  1. (**D<sub>p</sub>**) **Registered _p_ as dirty**: the only valid copy of this line
  is in state **D** and is stored in processor _p_'s local cache (_0 ≤ p < P_).
  The local storage reserved for this cache line is overwritten by the
  identifier _p_.
  2. (**C<sub>p</sub>**) **Registered _p_ as clean**: the only valid copy of this line
  is in state **C** and is stored in processor _p_'s local cache (_0 ≤ p < P_).
  The local storage reserved for this cache line is overwritten by the
  identifier _p_.
  3. (**W<sub>p</sub>**) **Registered _p_ as winner**: the only valid copy of this line
  is in state **W** and is stored in processor _p_'s local cache (_0 ≤ p < P_).
  The local storage reserved for this cache line is overwritten by the
  identifier _p_.
  4. (**V**) **Valid**: this is one of (possibly) many valid copies of the line in
  the system.
  5. (**I**) **Invalid**: this line is not in the directory.

For a particular line in the directory, the events which cause state transitions
are as follows. They are indexed by the processor identifier of the sender.
  1. (**Wr<sub>i</sub>**) **Write**: processor _i_ wants to write to this line.
  The directory may have to respond to this message with either an accept or reject.
  2. (**Ac<sub>i</sub>**) **Acquire**: processor _i_ wants to read from this line,
  and is not currently a sharer of it. The directory has to respond to this
  message with an accept or reject as well as the contents of the line.
  3. (**Cl<sub>i</sub>**) **Clean**: processor _i_ is downgrading its status
  from **D** to **C**.
  3. (**Sh<sub>i</sub>**) **Share**: processor _i_ wants to make its (dirty)
  copy of this line visible to other processors by downgrading its status from
  **W** to **O**. The processor also has to send the contents of the line.
  4. (**Fl<sub>i</sub>**) **Flush**: processor _i_ wants to push this line out
  of its cache. The processor also has to send the contents of the line.

### 3.3 Protocol

The above states and transition events are coordinated in a message-passing
style as follows. Transitions marked \- are impossible, and those marked &#9785;
violate phase-concurrency.

Transitions for L1 cache _i_. All messages are sent to (and received from) the
directory only. For transitions written _X_/_Y_, we transition to
_X_ if the directory replies with an accept and _Y_ if it rejects.

| | D | C | W | S | O | L | I |
| - | - | - | - | - | - | - | - |
| **Wr** | D | Send **Wr<sub>i</sub>**; <br> D | W | &#9785; | Send **Wr<sub>i</sub>**;<br> D/L | L | Send **Wr<sub>i</sub>**;<br> D/L |
| **Re** | D | C | &#9785; | S | S | &#9785; | Send **Ac<sub>i</sub>**;<br> Receive data; <br> C/S |
| **Ba** | Send **Cl<sub>i</sub>**; <br> C | C | Send **Sh<sub>i</sub>**;<br> Send data;<br> O | O | I | I | I |
| **Co** | W | L | - | - | - | - | - |
| **Fo** | - | Send data; <br> S | - | - | - | - | - |
| **Ev** | Send **Fl<sub>i</sub>**;<br> Send data;<br> I | Send **Fl<sub>i</sub>**;<br> Send data;<br> I | Send **Fl<sub>i</sub>**;<br> Send data;<br> I | I | I | I | - |


Transitions for the directory:

| | D<sub>p</sub> | C<sub>p</sub> | W<sub>p</sub> | V | I |
| - | - | - | - | - | - |
| **Wr<sub>i</sub>** | Assert _i ≠ p_;<br> Reject _i_;<br> Send **Co** to _p_;<br> W<sub>p</sub> | if _i = p_ then D<sub>p</sub><br> else <br>&nbsp;&nbsp; Accept _i_;<br>&nbsp;&nbsp; Send **Co** to _p_;<br>&nbsp;&nbsp; W<sub>i</sub> | Assert _i ≠ p_;<br> Reject _i_;<br> W<sub>p</sub> | Accept _i_;<br> D<sub>i</sub> | Accept _i_;<br> D<sub>i</sub>
| **Ac<sub>i</sub>** | &#9785; | Send **Fo** to _p_;<br> Receive data from _p_;<br> Reject _i_;<br> Send data to _i_;<br> V | &#9785; | Reject _i_;<br> Send data to _i_;<br> V | Retrieve data;<br> Accept _i_;<br> Send data to _i_;<br> C<sub>i</sub>
| **Cl<sub>i</sub>** | Assert _i = p_;<br> C<sub>p</sub> | - | - | - | - |
| **Sh<sub>i</sub>** | - | - | Assert _i = p_;<br> Receive data from _p_;<br> V | - | - |
| **Fl<sub>i</sub>** | Assert _i = p_;<br> Receive data from _p_;<br> V | Assert _i = p_;<br> Receive data from _p_;<br> V | Assert _i = p_;<br> Receive data from _p_;<br> V | - | - |

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
