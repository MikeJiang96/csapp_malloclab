/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
 #include <stdio.h>
 #include <stdlib.h>
 #include <assert.h>
 #include <unistd.h>
 #include <string.h>

 #include "mm.h"
 #include "memlib.h"

 /*********************************************************
  * NOTE TO STUDENTS: Before you do anything else, please
  * provide your team information in the following struct.
  ********************************************************/
 team_t team = {
     /* Team name */
     "ateam",
     /* First member's full name */
     "Mike Jiang",
     /* First member's email address */
     "jyt19960208@gmail.com",
     /* Second member's full name (leave blank if none) */
     "",
     /* Second member's email address (leave blank if none) */
     ""
 };

 /*
  * If NEXT_FIT defined use next fit search, else use first-fit search
  */
 #define NEXT_FIT

 /* $begin mallocmacros */
 /* Basic constants and macros */
 #define WSIZE       4       /* Word and header/footer size (bytes) */ //line:vm:mm:beginconst
 #define DSIZE       8       /* Double word size (bytes) */
 #define CHUNKSIZE  (1<<12)  /* Extend heap by this amount (bytes) */  //line:vm:mm:endconst

 #define MAX(x, y) ((x) > (y)? (x) : (y))

 /* Pack a size and allocated bit into a word */
 #define PACK(size, alloc)  ((size) | (alloc)) //line:vm:mm:pack

 /* Read and write a word at address p */
 #define GET(p)       (*(unsigned int *)(p))            //line:vm:mm:get
 #define PUT(p, val)  (*(unsigned int *)(p) = (val))    //line:vm:mm:put

 #define PREV_ALLOC_BIT       0x2
 #define SET_PREV_ALLOC(p)    (*(unsigned int *)(p) |= PREV_ALLOC_BIT)
 #define CLEAR_PREV_ALLOC(p)  (*(unsigned int *)(p) &= ~PREV_ALLOC_BIT)

 /* Read the size and allocated fields from address p */
 #define GET_SIZE(p)  (GET(p) & ~0x7)                   //line:vm:mm:getsize
 #define GET_ALLOC(p) (GET(p) & 0x1)                    //line:vm:mm:getalloc
 #define GET_PREV_ALLOC(p)  (GET(p) & PREV_ALLOC_BIT)

 /* Given block ptr bp, compute address of its header and footer */
 #define HDRP(bp)       ((char *)(bp) - WSIZE)                      //line:vm:mm:hdrp
 #define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) //line:vm:mm:ftrp

 /* Given block ptr bp, compute address of next and previous blocks */
 #define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE))) //line:vm:mm:nextblkp
 #define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE))) //line:vm:mm:prevblkp
 /* $end mallocmacros */

 /* Global variables */
 static char *heap_listp = 0;  /* Pointer to first block */
 #ifdef NEXT_FIT
 static char *rover;           /* Next fit rover */
 #endif

 /* Function prototypes for internal helper routines */
 static void *extend_heap(size_t words);
 static void place(void *bp, size_t asize);
 static void *find_fit(size_t asize);
 static void *coalesce(void *bp);
 static void printblock(void *bp);
 static void checkheap(int verbose);
 static void checkblock(void *bp);

 /*
  * mm_init - Initialize the memory manager
  */
 /* $begin mminit */
 int mm_init(void)
 {
     /* Create the initial empty heap */
     if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1) //line:vm:mm:begininit
         return -1;
     PUT(heap_listp, 0);                          /* Alignment padding */
     PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1 | PREV_ALLOC_BIT)); /* Prologue header */
     PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1 | PREV_ALLOC_BIT)); /* Prologue footer */
     PUT(heap_listp + (3*WSIZE), PACK(0, 1 | PREV_ALLOC_BIT));     /* Epilogue header */
     heap_listp += (2*WSIZE);                     //line:vm:mm:endinit
     /* $end mminit */

 #ifdef NEXT_FIT
     rover = heap_listp;
 #endif
     /* $begin mminit */

     /* Extend the empty heap with a free block of CHUNKSIZE bytes */
     if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
         return -1;
     return 0;
 }
 /* $end mminit */

 /*
  * mm_malloc - Allocate a block with at least size bytes of payload
  */
 /* $begin mmmalloc */
 void *mm_malloc(size_t size)
 {
     size_t asize;      /* Adjusted block size */
     size_t extendsize; /* Amount to extend heap if no fit */
     char *bp;

     /* $end mmmalloc */
     if (heap_listp == 0){
         mm_init();
     }
     /* $begin mmmalloc */
     /* Ignore spurious requests */
     if (size == 0)
         return NULL;

     /* Adjust block size to include overhead and alignment reqs. */
     if (size <= DSIZE)                                          //line:vm:mm:sizeadjust1
         asize = 2*DSIZE;                                        //line:vm:mm:sizeadjust2
     else
         asize = DSIZE * ((size + (WSIZE) + (DSIZE-1)) / DSIZE); //line:vm:mm:sizeadjust3

     /* Search the free list for a fit */
     if ((bp = find_fit(asize)) != NULL) {  //line:vm:mm:findfitcall
         place(bp, asize);                  //line:vm:mm:findfitplace
         return bp;
     }

     /* No fit found. Get more memory and place the block */
     extendsize = MAX(asize,CHUNKSIZE);                 //line:vm:mm:growheap1
     if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
         return NULL;                                  //line:vm:mm:growheap2
     place(bp, asize);                                 //line:vm:mm:growheap3
     return bp;
 }
 /* $end mmmalloc */

 /*
  * mm_free - Free a block
  */
 /* $begin mmfree */
 void mm_free(void *bp)
 {
     size_t prevAlloc;

     /* $end mmfree */
     if (bp == 0)
         return;

     /* $begin mmfree */
     size_t size = GET_SIZE(HDRP(bp));
     /* $end mmfree */
     if (heap_listp == 0){
         mm_init();
     }
     /* $begin mmfree */
     prevAlloc = GET_PREV_ALLOC(HDRP(bp));

     PUT(HDRP(bp), PACK(size, prevAlloc));
     PUT(FTRP(bp), PACK(size, 0));

     CLEAR_PREV_ALLOC(HDRP(NEXT_BLKP(bp)));

     coalesce(bp);
 }

 /* $end mmfree */
 /*
  * coalesce - Boundary tag coalescing. Return ptr to coalesced block
  */
 /* $begin mmfree */
 static void *coalesce(void *bp)
 {
     size_t prev_alloc = GET_PREV_ALLOC(HDRP(bp));
     size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
     size_t size = GET_SIZE(HDRP(bp));

     if (prev_alloc && next_alloc) {            /* Case 1 */
         return bp;
     }

     else if (prev_alloc && !next_alloc) {      /* Case 2 */
         size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
         PUT(HDRP(bp), PACK(size, PREV_ALLOC_BIT));
         PUT(FTRP(bp), PACK(size, 0));
     }

     else if (!prev_alloc && next_alloc) {      /* Case 3 */
         size += GET_SIZE(HDRP(PREV_BLKP(bp)));
         PUT(FTRP(bp), PACK(size, 0));
         PUT(HDRP(PREV_BLKP(bp)), PACK(size, PREV_ALLOC_BIT));
         bp = PREV_BLKP(bp);
     }

     else {                                     /* Case 4 */
         size += GET_SIZE(HDRP(PREV_BLKP(bp))) +
             GET_SIZE(FTRP(NEXT_BLKP(bp)));
         PUT(HDRP(PREV_BLKP(bp)), PACK(size, PREV_ALLOC_BIT));
         PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
         bp = PREV_BLKP(bp);
     }
     /* $end mmfree */
 #ifdef NEXT_FIT
     /* Make sure the rover isn't pointing into the free block */
     /* that we just coalesced */
     if ((rover > (char *)bp) && (rover < NEXT_BLKP(bp)))
         rover = bp;
 #endif
     /* $begin mmfree */
     return bp;
 }
 /* $end mmfree */

 /*
  * mm_realloc - Naive implementation of realloc
  */
 void *mm_realloc(void *ptr, size_t size)
 {
     size_t oldsize;
     void *newptr;

     /* If size == 0 then this is just free, and we return NULL. */
     if(size == 0) {
         mm_free(ptr);
         return 0;
     }

     /* If oldptr is NULL, then this is just malloc. */
     if(ptr == NULL) {
         return mm_malloc(size);
     }

     newptr = mm_malloc(size);

     /* If realloc() fails the original block is left untouched  */
     if(!newptr) {
         return 0;
     }

     /* Copy the old data. */
     oldsize = GET_SIZE(HDRP(ptr));
     if(size < oldsize) oldsize = size;
     memcpy(newptr, ptr, oldsize);

     /* Free the old block. */
     mm_free(ptr);

     return newptr;
 }

 /*
  * mm_checkheap - Check the heap for correctness
  */
 void mm_checkheap(int verbose)
 {
     checkheap(verbose);
 }

 /*
  * The remaining routines are internal helper routines
  */

 /*
  * extend_heap - Extend heap with free block and return its block pointer
  */
 /* $begin mmextendheap */
 static void *extend_heap(size_t words)
 {
     char *bp;
     size_t size;
     size_t prevAlloc;

     /* Allocate an even number of words to maintain alignment */
     size = (words % 2) ? (words+1) * WSIZE : words * WSIZE; //line:vm:mm:beginextend
     if ((long)(bp = mem_sbrk(size)) == -1)
         return NULL;                                        //line:vm:mm:endextend

     /* Initialize free block header/footer and the epilogue header */
     prevAlloc = GET_PREV_ALLOC(HDRP(bp));

     PUT(HDRP(bp), PACK(size, prevAlloc)); /* Free block header */   //line:vm:mm:freeblockhdr
     PUT(FTRP(bp), PACK(size, 0));         /* Free block footer */   //line:vm:mm:freeblockftr
     PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */ //line:vm:mm:newepihdr

     /* Coalesce if the previous block was free */
     return coalesce(bp);                                          //line:vm:mm:returnblock
 }
 /* $end mmextendheap */

 /*
  * place - Place block of asize bytes at start of free block bp
  *         and split if remainder would be at least minimum block size
  */
 /* $begin mmplace */
 /* $begin mmplace-proto */
 static void place(void *bp, size_t asize)
 /* $end mmplace-proto */
 {
     size_t csize = GET_SIZE(HDRP(bp));
     size_t prevAlloc = GET_PREV_ALLOC(HDRP(bp));

     if ((csize - asize) >= (2*DSIZE)) {
         PUT(HDRP(bp), PACK(asize, 1 | prevAlloc));

         bp = NEXT_BLKP(bp);
         PUT(HDRP(bp), PACK(csize-asize, PREV_ALLOC_BIT));
         PUT(FTRP(bp), PACK(csize-asize, 0));
     }
     else {
         PUT(HDRP(bp), PACK(csize, 1 | prevAlloc));

         SET_PREV_ALLOC(HDRP(NEXT_BLKP(bp)));
     }
 }
 /* $end mmplace */

 /*
  * find_fit - Find a fit for a block with asize bytes
  */
 /* $begin mmfirstfit */
 /* $begin mmfirstfit-proto */
 static void *find_fit(size_t asize)
 /* $end mmfirstfit-proto */
 {
     /* $end mmfirstfit */

 #ifdef NEXT_FIT
     /* Next fit search */
     char *oldrover = rover;

     /* Search from the rover to the end of list */
     for ( ; GET_SIZE(HDRP(rover)) > 0; rover = NEXT_BLKP(rover))
         if (!GET_ALLOC(HDRP(rover)) && (asize <= GET_SIZE(HDRP(rover))))
             return rover;

     /* search from start of list to old rover */
     for (rover = heap_listp; rover < oldrover; rover = NEXT_BLKP(rover))
         if (!GET_ALLOC(HDRP(rover)) && (asize <= GET_SIZE(HDRP(rover))))
             return rover;

     return NULL;  /* no fit found */
 #else
     /* $begin mmfirstfit */
     /* First-fit search */
     void *bp;

     for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
         if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
             return bp;
         }
     }
     return NULL; /* No fit */
 #endif
 }
 /* $end mmfirstfit */

 static void printblock(void *bp)
 {
     size_t hsize, halloc, fsize, falloc;

     checkheap(0);
     hsize = GET_SIZE(HDRP(bp));
     halloc = GET_ALLOC(HDRP(bp));
     fsize = GET_SIZE(FTRP(bp));
     falloc = GET_ALLOC(FTRP(bp));

     if (hsize == 0) {
         printf("%p: EOL\n", bp);
         return;
     }

     printf("%p: header: [%ld:%c] footer: [%ld:%c]\n", bp,
            hsize, (halloc ? 'a' : 'f'),
            fsize, (falloc ? 'a' : 'f'));
 }

 static void checkblock(void *bp)
 {
     if ((size_t)bp % 8)
         printf("Error: %p is not doubleword aligned\n", bp);
     if (!GET_ALLOC(HDRP(bp)) && (GET(HDRP(bp)) & ~PREV_ALLOC_BIT) != GET(FTRP(bp)))
         printf("Error: header does not match footer\n");
 }

 /*
  * checkheap - Minimal check of the heap for consistency
  */
 void checkheap(int verbose)
 {
     char *bp = heap_listp;

     if (verbose)
         printf("Heap (%p):\n", heap_listp);

     if ((GET_SIZE(HDRP(heap_listp)) != DSIZE) || !GET_ALLOC(HDRP(heap_listp)))
         printf("Bad prologue header\n");
     checkblock(heap_listp);

     for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
         if (verbose)
             printblock(bp);
         checkblock(bp);
     }

     if (verbose)
         printblock(bp);
     if ((GET_SIZE(HDRP(bp)) != 0) || !(GET_ALLOC(HDRP(bp))))
         printf("Bad epilogue header\n");
 }
