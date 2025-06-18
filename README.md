# Dynamic Memory Allocator in C

This project is my own implementation of a dynamic memory allocator—essentially, I recreated the core functionality of `malloc`, `free`, and `realloc` in C. The goal was to better understand low-level memory management and allocator design by building one from scratch.

## Features

- **Custom `malloc`, `free`, and `realloc`**: Handles dynamic memory allocation, deallocation, and resizing, with 16-byte alignment.
- **Heap Consistency Checker**: Includes a built-in tool to verify heap integrity and catch bugs early.
- **Flexible Design**: Started with an implicit free list and iteratively improved performance and space utilization with more advanced techniques.
- **No Standard Library Allocation Calls**: All memory management is handled internally, using a simulated heap interface.
- **Debugging Support**: Integrated with GDB and includes detailed error reporting for easier troubleshooting.

## How it Works

- The allocator manages a simulated heap and tracks free and allocated blocks.
- All pointer arithmetic and block management are handled manually, providing insight into how real allocators work under the hood.
- The heap checker scans for overlapping blocks, uncoalesced free space, and other common issues.

## Why I Built This

I wanted hands-on experience with the challenges of memory management in C—pointer arithmetic, fragmentation, and performance trade-offs. This project provided a deep dive into how allocators work and the subtle bugs that can arise in low-level code.

## Testing

- Built-in tests simulate real-world allocation patterns.
- Performance is measured in both space utilization and throughput.
- Debugging is supported with GDB and custom heap checks.

## Lessons Learned

- Writing a memory allocator is a great way to understand systems programming.
- Heap consistency checking is essential for catching subtle bugs.
- Incremental development and frequent testing are key to success.

---

Feel free to fork, explore, and experiment with allocator design!

[1] https://ppl-ai-file-upload.s3.amazonaws.com/web/direct-files/attachments/48964814/bed62008-a7fa-4c0d-815f-dfb78f821d81/paste.txt
[2] https://ppl-ai-file-upload.s3.amazonaws.com/web/direct-files/attachments/48964814/aa215c85-7d7f-4ef0-b6ab-f8cff7d9336e/paste.txt
