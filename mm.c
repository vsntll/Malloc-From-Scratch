/*
 * mm.c
 *
 * Name: [FILL IN]
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 * Also, read the README carefully and in its entirety before beginning.
 *
 */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>

#include "mm.h"
#include "memlib.h"

/*
 * If you want to enable your debugging output and heap checker code,
 * uncomment the following line. Be sure not to have debugging enabled
 * in your final submission.
 */
// #define DEBUG

#ifdef DEBUG
// When debugging is enabled, the underlying functions get called
#define dbg_printf(...) printf(__VA_ARGS__)
#define dbg_assert(...) assert(__VA_ARGS__)
#else
// When debugging is disabled, no code gets generated
#define dbg_printf(...)
#define dbg_assert(...)
#endif // DEBUG

// do not change the following!
#ifdef DRIVER
// create aliases for driver tests
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#define memset mm_memset
#define memcpy mm_memcpy
#endif // DRIVER

#define ALIGNMENT 16

// rounds up to the nearest multiple of ALIGNMENT
static size_t align(size_t x)
{
    return ALIGNMENT * ((x+ALIGNMENT-1)/ALIGNMENT);
}

typedef uint64_t word_t;

static const size_t WSIZE = sizeof(word_t); // word and header/footer size
static const size_t DSIZE = 2*WSIZE; // doubleword size
static const size_t min_block_size = DSIZE; // min block size
static const size_t CHUNKSIZE = (1<<12); // extend heap by this amount

static uint64_t* prologue; // prologue
static uint64_t* epilogue; // epilogue
static uint64_t* noFreeBlock; //list of free blocks

void set_header(word_t* block, size_t size, int prev_alloc, int alloc) {
    *block = (size) | (prev_alloc << 1) | alloc;
}

void set_footer(word_t* block, size_t size, int prev_alloc, int alloc) {
    word_t* footer = block + (size / WSIZE) - 1;
    *footer = (size) | (prev_alloc << 1) | alloc;
}

int getBit(uint64_t num, int index){
    return (num & (1ULL << index)) != 0;
}


// returning T/F based on whether or not it is a header or a footer
int is_header(uint64_t num, uint64_t* ptr, int i_offset){
    uint64_t potBlock = num >> 1;
    int potBlockStat = getBit(num, 63);
    uint64_t offset = potBlock/8;
    if (inheap(ptr + i_offset + offset + 1)== false){
        return 0;
    }
    //
    uint64_t futureBlock = ptr[i_offset + offset + 1];
    if (potBlock == (futureBlock >> 1) && potBlockStat == getBit(futureBlock, 63)){
        return 1;
    }
        return 0;
}

/*
 * mm_init: returns false on error, true on success.
 */
bool mm_init(void)
{
    // IMPLEMENT THIS
    size_t initial = align(CHUNKSIZE);
    word_t* start = (word_t*)mm_sbrk(initial);
    if (start == (void*)-1) {
        return false;
    }

    prologue = start;
    set_header(prologue, DSIZE, 1, 1);
    set_footer(prologue, DSIZE, 1, 1);
    
    epilogue = (word_t*)((char*)start + initial - WSIZE);
    set_header(epilogue, 0, 1, 1);

    noFreeBlock = NULL; //initialize free list

    if (prologue == (word_t)mm_heap_lo()&& initial == mm_heapsize()){
        return true;
    }
    else {
        return false;
    }
}

/*
 * malloc
 */
void* malloc(size_t size)
{
    if (size == 0) {
        return NULL;
    }
    size_t aligned = align(size);
    uint64_t* ptr = mm_heap_lo() + 3;
    int i = 0;
    while(ptr < mm_heap_hi()){
        if ((is_header(ptr[1], ptr, i) == 1) && (getBit(ptr[i], 63) == 0)){
            uint64_t blockSize = ptr[i] >> 1;
            if (blockSize == aligned){
                ptr[i] = ptr[i] | 1;
                ptr[i + aligned/8 + 1] = ptr[i];
                return ptr + i + 1;
            }
            else if (blockSize > aligned){
                ptr[i] = aligned << 1 | 1;
                ptr[i + aligned/8 + 1] = ptr[i];
                ptr[i + aligned/8 + 2] = (blockSize - aligned) << 1 | 0;
                ptr[i + aligned/8 + 1] = blockSize - aligned << 1 | 0;
                return ptr + i + 1;
            }
        }
    }
    
}

/*
 * free
 */
void free(void* ptr)
{
    // IMPLEMENT THIS
    if (!ptr) {
        return;
    }

/*
 * realloc
 */
void* realloc(void* oldptr, size_t size)
{
    // IMPLEMENT THIS
    return NULL;
}

/*
 * calloc
 * This function is not tested by mdriver, and has been implemented for you.
 */
void* calloc(size_t nmemb, size_t size)
{
    void* ptr;
    size *= nmemb;
    ptr = malloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

/*
 * Returns whether the pointer is in the heap.
 * May be useful for debugging.
 */
static bool in_heap(const void* p)
{
    return p <= mm_heap_hi() && p >= mm_heap_lo();
}

/*
 * Returns whether the pointer is aligned.
 * May be useful for debugging.
 */
static bool aligned(const void* p)
{
    size_t ip = (size_t) p;
    return align(ip) == ip;
}

/*
 * mm_checkheap
 * You call the function via mm_checkheap(__LINE__)
 * The line number can be used to print the line number of the calling
 * function where there was an invalid heap.
 */
bool mm_checkheap(int line_number)
{
#ifdef DEBUG
    // Write code to check heap invariants here
    // IMPLEMENT THIS
#endif // DEBUG
    return true;
}
