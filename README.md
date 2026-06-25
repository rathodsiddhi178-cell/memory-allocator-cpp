# Memory Allocator in C++

## Overview

A custom memory allocator developed in C++ implementing two memory management strategies:

- Segregated Free List Allocator
- Buddy System Allocator

The project simulates low-level heap management techniques used in operating systems and performance-critical applications. It provides custom implementations of memory allocation functions along with fragmentation analysis and benchmarking utilities.

---

## Features

- Custom malloc implementation
- Custom calloc implementation
- Custom realloc implementation
- Custom free implementation
- Segregated Free List allocation strategy
- Buddy System allocation strategy
- Block splitting and coalescing
- Memory usage tracking
- Fragmentation analysis
- Allocation benchmarking
- Interactive command-line interface

---

## Architecture

### Segregated Free List Allocator

Maintains separate free lists for different block sizes:

- Small Blocks (<= 64 Bytes)
- Medium Blocks (<= 256 Bytes)
- Large Blocks (> 256 Bytes)

Benefits:

- Faster allocation lookup
- Reduced search overhead
- Improved memory utilization

### Buddy System Allocator

Allocates memory in power-of-two sized blocks.

Benefits:

- Fast allocation and deallocation
- Efficient block splitting
- Reduced external fragmentation
- Simplified memory management

---

## Benchmarking

The allocator includes benchmarking functionality that:

- Performs random allocation operations
- Performs random deallocation operations
- Measures execution time
- Reports memory statistics
- Compares allocator performance

---

## Technologies Used

- C++
- Object-Oriented Programming
- Data Structures
- Dynamic Memory Management
- Operating System Concepts
- Performance Optimization

---

## Learning Outcomes

This project demonstrates practical understanding of:

- Heap Management
- Memory Allocation Algorithms
- Fragmentation Handling
- Allocator Design
- Low-Level Systems Programming
- Performance Analysis

---

## Build and Run

Compile:

```bash
g++ memory_allocator.cpp -o allocator
```

Run:

```bash
./allocator
```

---

## Future Improvements

- Thread-safe allocation support
- Dynamic heap expansion
- Allocation visualization
- Performance profiling dashboard
- Multi-threaded benchmarking

---

## Author

Siddhi Rathod
