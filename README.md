# CS:APP Malloc Lab

## Getting Started

> The malloc lab has to run on Linux OS

### Installing `gcc-multilib`

If you encounter the following error message:

```shell
$ make
gcc -Wall -O2 -m32   -c -o mdriver.o mdriver.c
In file included from /usr/include/bits/errno.h:26,
                 from /usr/include/errno.h:28,
                 from mdriver.c:13:
/usr/include/linux/errno.h:1:10: fatal error: asm/errno.h: No such file or directory
    1 | #include <asm/errno.h>
      |          ^~~~~~~~~~~~~
compilation terminated.
make: *** [<builtin>: mdriver.o] Error 1
```

Install `gcc-multilib`：

```shell
$ sudo apt install gcc-multilib
```

### Modify `TRACEDIR`

If you encounter the following error message:

```shell
$ ./mdriver -V
Testing mm malloc
Reading tracefile: amptjp-bal.rep
Could not open ~/malloc-lab/tracefilesamptjp-bal.rep in read_trace: No such file or directory
```

Modify the macro `TRACEDIR` in `config.h` to the absolute path to `tracefiles` in your computer, for example:

```cpp
#define TRACEDIR "/home/cpt1020/malloc-lab/tracefiles/"
```

### Compile & Run `mdriver`

```shell
$ make && ./mdriver -V
```

## My Implementations

| Ver. | Type | Free List | Insertion<br>Policy | Placement<br>Policy | Footer | Best `util` | Best `thru` |
| -------- | -------- | -------- | --- | --- | --- | --- | --- |
| v1 | Explicit | Singly<br>linked list | Size<br>(non-<br>decreasing<br>order) | First fit | No | 46 | 7 |
| v2 | Explicit | Singly<br>linked list | Address | First fit | No | 46 | 11 |
| v3 | Explicit | Singly<br>linked list | Address | Best fit | No | 46 | 11 |
| v4 | Explicit | Singly<br>linked list | Last-In-<br>First-Out<br>(LIFO) | First fit | No | 44 | 17 |
| v5 | Explicit | Doubly<br>linked list | LIFO | First fit | Yes | 44 | 40 |
| v6 | Explicit | Doubly<br>linked list | LIFO | Next fit | Yes | 44 | 40 |
| v7 | Segregate | Doubly<br>linked list | Size<br>(non-<br>decreasing<br>order) | First fit | Yes | 45 | 23 |
| v8 | Segregate | Doubly<br>linked list | LIFO | First fit | Yes | 45 | 25 |
| v9 | Segregate | Doubly<br>linked list | LIFO | First fit | Yes | 44 | 40 |

### v1

<p align=center>
    <img src="img/v1.png">
</p>

### v2

<p align=center>
    <img src="img/v2.png">
</p>

### v3

<p align=center>
    <img src="img/v3.png">
</p>

### v4

<p align=center>
    <img src="img/v4.png">
</p>

### v5

<p align=center>
    <img src="img/v5_1.png">
</p>

<p align=center>
    <img src="img/v5_2.png">
</p>

### v6

<p align=center>
    <img src="img/v6.png">
</p>

### v7

<p align=center>
    <img src="img/v7_1.png">
</p>

<p align=center>
    <img src="img/v7_2.png">
</p>

### v8

<p align=center>
    <img src="img/v8.png">
</p>

### v9

<p align=center>
    <img src="img/v9.png">
</p>