/*
 * mm.c
 *
 * Name: Avie Vasantlal
 * Email: adv5201@psu.edu
 *
 * How malloc works:
 * align to nearest multiple within ALIGNMENT
 * find the block that is large enough to fit the payload
 * extend the heap
 * place the block in 
 * 
 * How free works:
 * check if pointer is NULL
 * mark it as free by clearing the allocated bit
 * coalesce (4 cases):
 * both previous & next are allocated
 * if prev is allocated then next is free
 * if prev is free then next is allocated
 * both prev & next are free
 * 
 * coalesced block is then added to free list 
 * 
 * 
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
 
 static const size_t WSIZE = 8;
 static const size_t  DSIZE = 8;
 //static const size_t CHUNKSIZE = (1<<12);
 static char *heap_listp;

 struct block_meta *free_list = NULL; //global pointer
 
 static size_t PACK(size_t size, int alloc){
     return size | alloc;
 }
 
 static size_t GET(void *p){
     return *(size_t*) (p);
 }
 
 static void PUT(void *p, size_t val){
     *(size_t *)(p) = val;
 }
 
 static size_t GET_SIZE(void *p) {
     return GET(p) & ~0x7;
 }
 
 static int GET_ALLOC(void *p) {
     return GET(p) & 0x1;
 }
 
 static void* HDRP(void *bp) { 
     return (char *)(bp) - WSIZE;
 }
 
 static void* FTRP(void *bp) {  
     return (char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE;
 }
 
 static void* NEXT_BLKP(void *bp) { 
    return (char *)(bp) + GET_SIZE(HDRP(bp));
}


static void* PREV_BLKP(void *bp) { 
    return (char *)(bp) - GET_SIZE((char *)(bp) - DSIZE);
}

//returns max size between x and y.
static size_t MAX(size_t x, size_t y) {
    return (x > y) ? x : y;
}

struct block_meta{
    size_t size;
    struct block_meta *next;
    int is_free;
};


//function declarations
static void *extend_heap(size_t words);
static void *find_fit(size_t size);
static void place(void *bp, size_t asize);
static void *coalesce(void *bp);
size_t get_size(void *ptr);
bool is_free(void *ptr);
void merge_blocks(void *oldptr, void *next_block);

typedef struct free_block_t{
    size_t header;
    struct free_block_t *next;
    struct free_block_t *prev;
} free_block_t;

//set head pointer to first free block in free list
free_block_t *head = NULL;


bool is_free(void *ptr) {
    return !GET_ALLOC(HDRP(ptr));
}

static void *coalesce(void*bp)
 {
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); 
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); 
    size_t size = GET_SIZE(HDRP(bp));

    //case 1
    if (prev_alloc && next_alloc){
        return bp;
    }
    
    //case 2
    else if(prev_alloc && !next_alloc){ 
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp),PACK(size,0));
        PUT(FTRP(bp),PACK(size,0));
    }

    //case 3
    else if(!prev_alloc && next_alloc){ 
        size+=GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp),PACK(size,0));
        PUT(HDRP(PREV_BLKP(bp)),PACK(size,0));
        bp=PREV_BLKP(bp);
    }
 
    //case 4
    else{ 
        size += GET_SIZE(HDRP(PREV_BLKP(bp)))+ GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)),PACK(size,0));
        PUT(FTRP(NEXT_BLKP(bp)),PACK(size,0));
        bp=PREV_BLKP(bp);
    }
    return bp;
}

 //check for corruption
 void validate_heap() {
    uint64_t* curr = (uint64_t*)mm_heap_lo();
    while (curr < (uint64_t*)mm_heap_hi()) {
        size_t block_size = *curr & ~1;
        if (block_size < ALIGNMENT || (uintptr_t)curr % ALIGNMENT != 0) {
            printf("Heap corruption detected at %p\n", (void*)curr);
            exit(1);
        }
        curr = (uint64_t*)((char*)curr + block_size);
    }
}



static void *extend_heap(size_t size) {
    char *bp;

    if ((void *)(bp = mem_sbrk(size)) == (void *)-1) {
        return NULL;
    }

    PUT(HDRP(bp), PACK(size, 0));           // Free block header
    PUT(FTRP(bp), PACK(size, 0));           // Free block footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));   // New epilogue header

    return coalesce(bp);
}

//searches free list for block that fits
static void *find_fit(size_t asize){ 
    void *bp;
    
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)){
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))){
            return bp;
        }
        
    }
    return NULL;
}

//merges in coalescing
void merge_blocks(void *oldptr, void *next_block){
    size_t oldsize = get_size(oldptr);
    size_t nextsize = get_size(next_block);

    PUT(HDRP(oldptr), PACK(oldsize + nextsize, 0));
    PUT(FTRP(oldptr), PACK(oldsize + nextsize, 0));
}

//places allocated block & splits
static void place (void *bp, size_t asize){
    size_t csize = GET_SIZE(HDRP(bp));

    if ((csize - asize) >= (2*DSIZE)){
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize, 0));
        PUT(FTRP(bp), PACK(csize-asize, 0));
    }
    else{
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }

}

size_t get_size(void *ptr){
    return GET_SIZE(HDRP(ptr));
}



 
 /*
  * mm_init: returns false on error, true on success.
  */
 bool mm_init(void)
 {
     // IMPLEMENT THIS
     //  uint64_t* start = mm_sbrk(align(32));
     //  if (start == (void *)-1) {
    //      return false;
    //  }
 
    //  start[0] = 0;
    //  start[1] = 0;
    //  start[2] = 0;
    //  start[3] = 0;
 
    //  return true;
    if((heap_listp=mm_sbrk(8))==(void*)-1){
        return false;}
    // PUT(heap_listp,0); 
    // PUT(heap_listp+(1*WSIZE),PACK(DSIZE,1));
    // PUT(heap_listp+(2*WSIZE),PACK(DSIZE,1));
    // PUT(heap_listp+(3*WSIZE),PACK(0,1)); 
    // heap_listp+=(2*WSIZE);
 
  
    // if(extend_heap(CHUNKSIZE/WSIZE)==NULL){
    //     return false;}
    head = NULL;
    return true;
 }




 
 /*
  * malloc
  */
 void* malloc(size_t size)
 {
     // IMPLEMENT THIS
    
    size_t asize;
    size_t extend;
    char *bp;

    if (size == 0){
        return NULL;
    }


    asize = align(size + 8);
    free_block_t *prev = NULL;
    free_block_t *curr = head;

    int iterations = 0;
    free_block_t *prevBestFit = NULL;
    free_block_t *currBestFit = NULL;

    while (curr != NULL && iterations < 500 ){
        if (asize <= GET_SIZE(&curr -> header)){
            if (currBestFit == NULL || GET_SIZE(&curr -> header) < GET_SIZE(&currBestFit -> header)){
                currBestFit = curr;
                prevBestFit = prev;
            }
        }
        prev = curr;
        curr = curr -> next;
        iterations += 1;
    }

    if (currBestFit != NULL){
        if(prevBestFit == NULL){
            head = currBestFit -> next;
        }
        else{
            prevBestFit -> next = currBestFit -> next;
        }
        PUT(currBestFit, PACK(GET_SIZE(&currBestFit -> header), 0));
        return (char *)currBestFit + 8;
    }

    extend = asize;
    if ((bp = mm_sbrk(extend))== NULL){
        return NULL;
    }
    PUT(bp, PACK(asize, 1));
    return bp + 8;


    // if (size <= DSIZE){
    //     asize = 2*DSIZE;
    // }

    // else{
    //     asize = DSIZE * ((size + DSIZE + (DSIZE - 1)) / DSIZE);
    // }
    //     if ((bp = find_fit(asize)) != NULL){
    //         place(bp, asize);
    //         return bp;
    //     }
    //     validate_heap();
    //     //no fit
    //     extend = MAX(asize, CHUNKSIZE);
    //     if (( bp = extend_heap(extend/WSIZE)) == NULL){
    //         return NULL;
    //     }
    //     validate_heap();
    //     place(bp, asize);
    //     return bp;
}

     













     // if size is 0, return NULL
    //  if (size == 0) return NULL;
 
    //  //align
    //  size = align(size);
 
    //  // check if size is less than the minimum block size
    //  uint64_t* present = (uint64_t*) mm_heap_lo() + 3; 
    //  uint64_t* end = (uint64_t*) mm_heap_hi();   
 
    //  //find da block
    //  while (present < end) {
    //      //check if present is header, aligned, and last bit is 0
    //      if (!(*present & 0xF) && !(*present & 1)){
    //          // this is a header
    //          size_t block_size = *present & ~0xF;
    //          if (*present == size){
    //              *present = size | 1;
    //              present[size/8 + 1] = size | 1; //footer
    //              return (void*) (present + 1); // return payload
    //          }
             
    //          else if (block_size >= size){
    //              if(block_size > ALIGNMENT - size){
    //                  size_t remaining = block_size - size - ALIGNMENT;
    //                  *present = size | 1;
    //                  *(present + size/8 + 1) = size | 1;
 
    //                  uint64_t* new_free = present + size/8 + 2; // make the empty
    //                  *new_free = remaining | 0;
    //                  *(new_free + remaining / 8 + 1) = remaining | 0; // footer
    //                  return (void*) (present + 1);
    //              }
    //          }
    //          //payload + H&F fits in available space. if so- add block, add footer & header for empty space
    //          else if (*present > ALIGNMENT - size){ 
    //              // ! split
    //              uint64_t* new = present + size/8 + 2; //make the empty
    //              *new = (*(present) - size - ALIGNMENT) | 0; // add the header
    //              *present = size | 1;
    //              present[size/8 + 1] = size | 1; // footer
    //              new[*new/8 + 1] = *new; // footer for empty
    //              return (void*) (present + 1); // return payload 
    //          }
    //          // all else fails            
    //          else{
    //              // hello epilogue! become the header.
    //              present[(mm_heapsize()/8 - 1)] = size| 1;
    //              // extend heap
    //              uint64_t* new = mm_sbrk(size + ALIGNMENT);      // ! take this out and free the block and should get 100 or figure out coalescing
    //              if (new == (void*)-1) {   //error handling
    //                  return NULL;
    //              }
    //              // set footer
    //              new[size / 8] = size | 1; 
 
    //              // set epilogue
    //              new[size / 8 + 1] = ALIGNMENT | 1; 
    //              return new;
 
    //          }
    //      }
         
    //      present++;
    //  }
 
    //  return NULL; // oops - nothing fits
 
 

 
 /*
  * free
  */
 void free(void* ptr){
     //IMPLEMENT THIS
     /*
     ! THE GLORIUS SEGMENTATION FAULT MAKER
     */ 
    // validate_heap();

    if (ptr == NULL) return;

    PUT(HDRP(ptr), PACK(GET_SIZE(ptr - 8), 0));
    free_block_t * free = (free_block_t *) HDRP(ptr);
    
    free->next = head;

    head = free;

    
 
    //  if (ptr == NULL){ // null
    //      return;
    //  }
 
    //  uint64_t* header = (uint64_t*)ptr - 1;
    //      return; // already free
    //  }
 
    //  // Mark the block as free
    //  size_t block_size = *header & ~1; // Clear the allocated bit
     
    //  // Set the footer for the freed block
    //  uint64_t *footer = (uint64_t *)((char *)ptr + block_size - ALIGNMENT);
     
    //  *footer = block_size | 0;
     //coalesce_blocks(&header, &footer, &block_size);
 
     //  //? time to coalesce next block ---- past has always resulted in segmentation fault
      
     // //coalesce with the next block if it's free
     // uint64_t *next_header = (uint64_t *)((char *)ptr + block_size);
     // if (next_header < (uint64_t*)mm_heap_hi() && !(*next_header & 1)) {
     //     size_t next_size = *next_header & ~1; // Clear the allocated bit
     //     block_size += next_size + ALIGNMENT; // Add the size of the next block and header/footer overhead
     //     *header = block_size|0;              // Update header
     //     *footer = block_size|0;              // Update footer
     //     //printf("Coalesced with next block: new size = %zu\n", block_size);
     // }
 
     // // try to coalesce with the previous block if it's free
     // uint64_t* prev_footer = (uint64_t*)((char*)header - ALIGNMENT);
     // if (prev_footer > (uint64_t*)mm_heap_lo() && !(*prev_footer & 1)) {
     //     size_t prev_size = *prev_footer & ~1; // Clear the allocated bit
     //     block_size += prev_size + ALIGNMENT; // Add the size of the previous block and header/footer overhead
     //     //header = (uint64_t*)((char*)header - prev_size - ALIGNMENT); // ! problem child
     //     *header = block_size | 0;                // Update header
     //     *footer = block_size | 0;                // Update footer
     //     //printf("Coalesced with previous block: new size = %zu\n", block_size);
     //}
 
 }
 
 
 /*
  * realloc
  */
 void* realloc(void* oldptr, size_t size)
 {
     // IMPLEMENT THIS
     if (size == 0) {
         free(oldptr);
         return NULL;
     }
     //frees block if size is 0
     if (oldptr == NULL) {
         return malloc(size);
     }
     // temp store data & copy to new block
     unsigned char buf[size];
     mm_memcpy(buf, oldptr, size);
     void * ptr = malloc(size);
     mm_memcpy(ptr, buf, size);
 
 
     return ptr;
 
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
  * function wpresent tpresent was an invalid heap.
  */
 bool mm_checkheap(int line_number)
 {
 #ifdef DEBUG
     // Write code to check heap invariants present
     // IMPLEMENT THIS
 #endif // DEBUG
     return true;
 }
 