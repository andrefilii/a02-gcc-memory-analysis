// tests/verify_obstack.c
#include <stdio.h>
#include <obstack.h>
#include <stdlib.h>

// Macros required by the obstack library
#define obstack_chunk_alloc malloc
#define obstack_chunk_free free

int main() {
    struct obstack my_obstack;
    
    // 1. Initialize the obstack (mimics gcc_obstack_init)
    obstack_init(&my_obstack);
    printf("Obstack initialized. Base Chunk Address: %p\n", my_obstack.chunk);

    // 2. Start a "Scope" (Mark the current spot)
    void *scope_mark = obstack_alloc(&my_obstack, 0); 
    printf("Scope Marker (Rewind Point): %p\n", scope_mark);

    // 3. Allocate "Temporary" compiler objects (e.g., local variables)
    void *obj1 = obstack_alloc(&my_obstack, 64);
    void *obj2 = obstack_alloc(&my_obstack, 128);
    
    printf("Allocated Obj1 at: %p\n", obj1);
    printf("Allocated Obj2 at: %p\n", obj2);
    printf("Current next_free pointer: %p\n", my_obstack.next_free);

    // 4. Verification Moment: Free the scope
    // This should reset 'next_free' directly to 'scope_mark'
    printf("Freeing to Scope Marker...\n");
    obstack_free(&my_obstack, scope_mark);

    // 5. Check results
    printf("New next_free pointer:     %p\n", my_obstack.next_free);
    
    if (my_obstack.next_free == scope_mark) {
        printf("VERIFICATION SUCCESS: Pointer reset to mark.\n");
    } else {
        printf("VERIFICATION FAILED.\n");
    }

    return 0;
}