# CMU 15-213 Malloc Lab

| Ver. | Type | Free List | Insertion<br>Policy | Placement<br>Policy | Footer | Best `util` | Best `thru` |
| -------- | -------- | -------- | --- | --- | --- | --- | --- |
| v1 | Explicit | Singly<br>linked list | Size<br>(non-<br>decreasing<br>order) | First fit | No | 44 | 6 |
| v2 | Explicit | Singly<br>linked list | Address | First fit | No | 46 | 14 |
| v3 | Explicit | Singly<br>linked list | Address | Best fit | No | 46 | 14 |
| v4 | Explicit | Singly<br>linked list | Last-In-<br>First-Out<br>(LIFO) | First fit | No | 45 | 18 |
| v5 | Explicit | Doubly<br>linked list | LIFO | First fit | Yes | 44 | 40 |
| v6 | Explicit | Doubly<br>linked list | LIFO | Next fit | Yes | 44 | 40 |
| v7 | Segregate | Doubly<br>linked list | Size<br>(non-<br>decreasing<br>order) | First fit | Yes | 45 | 24 |
| v8 | Segregate | Doubly<br>linked list | LIFO | First fit | Yes | 45 | 27 |
| v9 | Segregate | Doubly<br>linked list | LIFO | First fit | Yes | 44 | 40 |