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
 * if prevBestFit is allocated then next is free
 * if prevBestFit is free then next is allocated
 * both prevBestFit & next are free
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
//! HELLO! THE FOLLOWING FUNCTIONS AND HELPERS INCLUDE CODE TAKEN FROM THE TEXTBOOK 'COMPUTER SYSTEMS'. THIS BOOK WAS USED AS A BASELINE FOR THE PACK/GET/PUT/ETC FUNCTIONS WHERE WERE STATED AS MACROS ANDT] WERE ADAPTED. THANK YOU FOR COMING TO MY TED TALK!
// rounds up to the nearest multiple of ALIGNMENT
static size_t align(size_t x)
{
    return ALIGNMENT * ((x+ALIGNMENT-1)/ALIGNMENT);
}
//? i dont know why this only works if 8 , 8 but it does and anything else breaks it so we proceed as if all is good
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


//function declarations so that i can declare in any order i desire
static void *extend_heap(size_t words);
static void *find_fit(size_t size);
static void place(void *bp, size_t asize);
static void *coalesce(void *bp);
size_t get_size(void *ptr);
bool is_free(void *ptr);
void merge_blocks(void *oldptr, void *next_block);

// woohoo! this one is the most important one. used to keep track of free blocks in heap
typedef struct free_block_t{
   size_t header;
   struct free_block_t *next;
   struct free_block_t *prevBestFit;
} free_block_t;

//set head pointer to first free block in free list
free_block_t *head = NULL;

//array of free lists
free_block_t *segregated_free_lists[10];


//check if block is free
bool is_free(void *ptr) {
   return !GET_ALLOC(HDRP(ptr));
}

// Helper function to get the list index range
int get_list_index(size_t size) {
   if (size <= 32) {
       return 0;
   }
   else if (size <= 64) {
       return 1;
   }
   else if (size <= 128) {
       return 2;
   }
   else if (size <= 256) {
       return 3;
   }
   else if (size <= 512) {
       return 4;
   }
   else if (size <= 1024) {
       return 5;
   }
   else if (size <= 2048) {
       return 6;
   }
   else if (size <= 4096) {
       return 7;
   }
   else if (size <= 8192) {
       return 8;
    }
    
    return 9;
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

static void *coalesce(void*bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); 
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); 
    size_t size = GET_SIZE(HDRP(bp));

    int index = get_list_index(size);
    free_block_t *curr = (free_block_t *)bp;
    free_block_t *prev = NULL;
    free_block_t *element = segregated_free_lists[index];

    while (element != NULL && element != curr) {
        prev = element;
        element = element->next;
    }

    if (element == curr) {
        if (prev == NULL) {
            segregated_free_lists[index] = element->next;
        } 
        else {
        prev->next = element->next;
        }
    }
    
   //case 1
   if (prev_alloc && next_alloc){
       index = get_list_index(size);
       curr -> next = segregated_free_lists[index];
       segregated_free_lists[index] = curr;
       return bp;
   }
   
   //case 2
   else if(prev_alloc && !next_alloc){ 
       void *next_block = NEXT_BLKP(bp);
       size += GET_SIZE(HDRP(next_block));

       //remove next from free
       index = get_list_index(GET_SIZE(HDRP(next_block)));
       free_block_t *nextfree = (free_block_t *)next_block;
       prev = NULL;
       element = segregated_free_lists[index];

       while(element != NULL && element != nextfree){
           prev = element;
           element = element -> next;
       }

       if (element == nextfree){
            if(prev == NULL){
                segregated_free_lists[index] = element -> next;
            }
            else{
                prev -> next = element -> next;
            }
       }
       PUT(HDRP(bp), PACK(size, 0));
       PUT(FTRP(bp), PACK(size, 0));

       //this makes sense but let's see if it works because base coalesce hardly does
   }

   //case 3
   else if(!prev_alloc && next_alloc){ 
       void *prev_block = PREV_BLKP(bp);
       size += GET_SIZE(HDRP(prev_block));

       //remove prev from free
       index = get_list_index(GET_SIZE(HDRP(prev_block)));
       free_block_t *prevfree = (free_block_t *)prev_block;
       prev = NULL;
       element = segregated_free_lists[index];

       while(element != NULL && element != prevfree){
           prev = element;
           element = element -> next;
       }

       if (element == prevfree){
           if(prev == NULL){
               segregated_free_lists[index] = element -> next;
           }
           else{
               prev -> next = element -> next;
           }
       }
       PUT(HDRP(prev_block), PACK(size, 0));
       PUT(FTRP(prev_block), PACK(size, 0));
       bp = prev_block;
       
   }
   //case 4
   else{ 
       void *prev_block = PREV_BLKP(bp);
       void *next_block = NEXT_BLKP(bp);
       size += GET_SIZE(HDRP(prev_block)) + GET_SIZE(HDRP(next_block));

       //remove prev & next from free
       index = get_list_index(GET_SIZE(HDRP(prev_block)));
       free_block_t *prevfree = (free_block_t *)prev_block;
       prev = NULL;
       element = segregated_free_lists[index];

       while(element != NULL && element != prevfree){
           prev = element;
           element = element -> next;
       }

       if (element == prevfree){
           if(prev == NULL){
               segregated_free_lists[index] = element -> next;
           }
           else{
               prev -> next = element -> next;
           }
       }
       index = get_list_index(GET_SIZE(HDRP(next_block)));
       free_block_t *nextfree = (free_block_t *)next_block;
       prev = NULL;
       element = segregated_free_lists[index];

       while(element != NULL && element != nextfree){
           prev = element;
           element = element -> next;
       }
       if (element == nextfree){
           if(prev == NULL){
               segregated_free_lists[index] = element -> next;
           }
           else{
               prev -> next = element -> next;
           }
       }
       PUT(HDRP(prev_block), PACK(size, 0));
       PUT(FTRP(prev_block), PACK(size, 0));
       bp = prev_block;
   }
   index = get_list_index(GET_SIZE(HDRP(bp)));
   curr = (free_block_t *)bp;
   curr->next = segregated_free_lists[index];
   segregated_free_lists[index] = curr;

   return bp;
}

// Helper function that splits a free block if excess space is 32 bytes or more, adding the remainder to the free list.
static void split(size_t size, free_block_t *curblock) {
   size_t free_size = GET_SIZE(&curblock->header);
   size_t diff = free_size - size;
   if (diff >= 32) {  
       PUT((char *)curblock, PACK(size, 1));
       PUT((char *)curblock + size, PACK(diff, 0));

       int index = get_list_index(diff);
       free_block_t *next_block = (free_block_t *)((char *)curblock + size);
       next_block->next = segregated_free_lists[index];
       segregated_free_lists[index] = next_block;
   } else {
       PUT(curblock, PACK(free_size, 1));  
   }
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
   for(int i = 0; i < 10; i++) {
       segregated_free_lists[i] = NULL;
   }
   return true;
}





/*
 * malloc
 */
void* malloc(size_t size)
{
    // IMPLEMENT THIS
   if (size == 0){
       return NULL;
   }


   size_t asize = align(size + 8);
   //free_block_t *curr = head;

   //int iterations = 0;
   int index = get_list_index(asize);
   free_block_t *prevBestFit = NULL;
   free_block_t *currBestFit = NULL;

   // Search for the best fit in segregated lists starting from the right index based on set from above
   for (int i = index; i < 10 && !currBestFit; i++) {
       free_block_t *current = segregated_free_lists[i];
       prevBestFit = NULL;
       
       // Iterate through the list
       while (current) {
           if (GET_SIZE(&current->header) >= asize) {
               currBestFit = current;
               break;
           }
           prevBestFit = current;
           current = current->next;
       }
   }
   if (currBestFit){
       if(prevBestFit){
           prevBestFit->next = currBestFit->next;
       }
       else{
           segregated_free_lists[get_list_index(GET_SIZE(&currBestFit->header))] = currBestFit->next;
       }
       split(asize, currBestFit);
       return (char *)currBestFit + 8;
   }

//! somehow take this line and swap it with coalescing alongside the bp initialization
   char *bp = mm_sbrk(asize);
   if (bp == (void *)-1) return NULL;
//this too   
   PUT(bp, PACK(asize, 1)); 






   return bp + 8;

   // while (curr != NULL && iterations < 500 ){
   //     if (asize <= GET_SIZE(&curr -> header)){
   //         if (currBestFit == NULL || GET_SIZE(&curr -> header) < GET_SIZE(&currBestFit -> header)){
   //             currBestFit = curr;
   //             prevBestFit = prevBestFit;
   //         }
   //     }
   //     prevBestFit = curr;
   //     curr = curr -> next;
   //     iterations += 1;
   // }




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

   PUT(HDRP(ptr), PACK(GET_SIZE(HDRP(ptr)), 0));
   int index = get_list_index(GET_SIZE(HDRP(ptr)));
   free_block_t * free = (free_block_t *) HDRP(ptr);
   free -> next = segregated_free_lists[index];
   segregated_free_lists[index] = free;    

   // free->next = head;

   // head = free;

   

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
    void *newptr = malloc(size);
    if(!newptr){
       return NULL;
    }
   size_t copy_size = GET_SIZE(HDRP(oldptr))- DSIZE;
   if (size < copy_size) {
       copy_size = size;
   }  
   memcpy(newptr, oldptr, copy_size);
   free(oldptr);
   


   //  // temp store data & copy to new block
   //  unsigned char buf[size];
   //  mm_memcpy(buf, oldptr, size);
   //  void * ptr = malloc(size);
   //  mm_memcpy(ptr, buf, size);


   return newptr;

}

/*
 * calloc
 * This function is not tested by mdriver, and has been implemented for you.
 */
void* calloc(size_t nmemb, size_t size)
{
    void* ptr;
    size_t total_size = nmemb * size;
    ptr = malloc(total_size);
    if (ptr) {
        memset(ptr, 0, total_size);
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