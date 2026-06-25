// Simple Memory Allocator in C++
// Two strategies: Segregated Free List + Buddy System

#include <cstdio>
#include <cstring>
#include <new>
#include <ctime>

// Basic utility functions
static void* my_memset(void* ptr, int val, unsigned int len)
{
    unsigned char* p = (unsigned char*)ptr;
    while (len--) *p++ = (unsigned char)val;
    return ptr;
}

static void* my_memcpy(void* dst, const void* src, unsigned int len)
{
    unsigned char* d = (unsigned char*)dst;
    const unsigned char* s = (const unsigned char*)src;
    while (len--) *d++ = *s++;
    return dst;
}

// Find next power of 2
static unsigned int power_of_2(unsigned int n)
{
    if (n == 0) return 1;
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    return n + 1;
}

// Check if n is power of 2
static bool is_power2(unsigned int n)
{
    return n && !(n & (n - 1));
}

// Track memory allocations
struct AllocationLog
{
    const char* op;
    unsigned int size;
    void* addr;
};

class MemoryTracker
{
    AllocationLog logs[1024];
    int log_count;
    
public:
    unsigned int total_alloc;
    unsigned int total_free;
    unsigned int peak;
    unsigned int current;
    int alloc_count;
    int free_count;
    
    MemoryTracker() : log_count(0), total_alloc(0), total_free(0),
                      peak(0), current(0), alloc_count(0), free_count(0) {}
    
    void log_op(const char* op, unsigned int sz, void* addr)
    {
        if (log_count < 1024) {
            logs[log_count++] = {op, sz, addr};
        }
        
        if (op[0] == 'A') {
            total_alloc += sz;
            current += sz;
            alloc_count++;
            if (current > peak) peak = current;
        } else if (op[0] == 'F') {
            if (current >= sz) current -= sz;
            total_free += sz;
            free_count++;
        }
    }
    
    void show_stats()
    {
        printf("\n===== MEMORY STATS =====\n");
        printf("Total allocate: %d times\n", alloc_count);
        printf("Total free    : %d times\n", free_count);
        printf("Bytes alloc   : %u\n", total_alloc);
        printf("Bytes freed   : %u\n", total_free);
        printf("Peak usage    : %u bytes\n", peak);
        printf("Current usage : %u bytes\n", current);
        unsigned int leak = total_alloc > total_free ? total_alloc - total_free : 0;
        printf("Leak detected : %u bytes\n", leak);
        printf("        ****        \n");
    }
};

// Base allocator interface
class Allocator
{
public:
    virtual void* malloc(unsigned int size) = 0;
    virtual void* calloc(unsigned int count, unsigned int size) = 0;
    virtual void* realloc(void* ptr, unsigned int newsize) = 0;
    virtual void free(void* ptr) = 0;
    virtual void show_heap() const = 0;
    virtual unsigned int free_space() const = 0;
    virtual unsigned int used_space() const = 0;
    
    virtual ~Allocator() {}
    
    void show_fragmentation()
    {
        unsigned int free_mem = free_space();
        unsigned int used_mem = used_space();
        unsigned int total = free_mem + used_mem;
        float frag = total > 0 ? (1.0f - (float)free_mem / total) * 100.0f : 0.0f;
        printf("Free : %u bytes\n", free_mem);
        printf("Used : %u bytes\n", used_mem);
        printf("Fragmentation: %.1f%%\n", frag);
    }
};

// Segregated Free List Allocator

static const int HEAP_SIZE = 8192;

struct Block
{
    unsigned int size;
    int free;
    int magic;
    Block* next;
};

static const int MAGIC = 0xDEADBEEF;

class SegAllocator : public Allocator
{
    char heap[HEAP_SIZE];
    Block* head;
    Block* small_list;
    Block* med_list;
    Block* large_list;
    MemoryTracker tracker;
    
    unsigned int align(unsigned int s)
    {
        return (s % 8 == 0) ? s : s + (8 - s % 8);
    }
    
    Block** get_list(unsigned int sz)
    {
        if (sz <= 64) return &small_list;
        if (sz <= 256) return &med_list;
        return &large_list;
    }
    
    void remove_block(Block* b)
    {
        Block** list = get_list(b->size);
        Block* cur = *list;
        Block* prev = nullptr;
        
        while (cur) {
            if (cur == b) {
                if (prev) prev->next = cur->next;
                else *list = cur->next;
                b->next = nullptr;
                return;
            }
            prev = cur;
            cur = cur->next;
        }
    }
    
    void add_free(Block* b)
    {
        Block** list = get_list(b->size);
        b->next = *list;
        *list = b;
    }
    
    void split(Block* b, unsigned int sz)
    {
        if (b->size >= sz + sizeof(Block) + 8) {
            Block* new_b = (Block*)((char*)b + sizeof(Block) + sz);
            new_b->size = b->size - sz - sizeof(Block);
            new_b->free = 1;
            new_b->magic = MAGIC;
            new_b->next = nullptr;
            b->size = sz;
            add_free(new_b);
        }
    }
    
    void coalesce()
    {
        bool changed = true;
        while (changed) {
            changed = false;
            Block* cur = head;
            
            while ((char*)cur < heap + HEAP_SIZE) {
                Block* nxt = (Block*)((char*)cur + sizeof(Block) + cur->size);
                if ((char*)nxt >= heap + HEAP_SIZE) break;
                
                if (cur->free && nxt->free) {
                    remove_block(nxt);
                    remove_block(cur);
                    cur->size += sizeof(Block) + nxt->size;
                    add_free(cur);
                    changed = true;
                } else {
                    cur = nxt;
                }
            }
        }
    }
    
public:
    SegAllocator() : head(nullptr), small_list(nullptr),
                     med_list(nullptr), large_list(nullptr)
    {
        head = (Block*)heap;
        head->size = HEAP_SIZE - sizeof(Block);
        head->free = 1;
        head->magic = MAGIC;
        head->next = nullptr;
        large_list = head;
    }
    
    void* malloc(unsigned int sz) override
    {
        sz = align(sz);
        if (sz == 0) return nullptr;
        
        Block* lists[3] = {small_list, med_list, large_list};
        
        for (int i = 0; i < 3; i++) {
            Block* cur = lists[i];
            Block* prev = nullptr;
            
            while (cur) {
                if (cur->free && cur->size >= sz) {
                    if (prev) prev->next = cur->next;
                    else {
                        if (i == 0) small_list = cur->next;
                        else if (i == 1) med_list = cur->next;
                        else large_list = cur->next;
                    }
                    cur->next = nullptr;
                    split(cur, sz);
                    cur->free = 0;
                    tracker.log_op("ALLOC", sz, (char*)cur + sizeof(Block));
                    return (char*)cur + sizeof(Block);
                }
                prev = cur;
                cur = cur->next;
            }
        }
        
        printf("SegAlloc Out of memory\n");
        return nullptr;
    }
    
    void* calloc(unsigned int count, unsigned int sz) override
    {
        unsigned int total = count * sz;
        void* ptr = malloc(total);
        if (ptr) my_memset(ptr, 0, total);
        return ptr;
    }
    
    void* realloc(void* ptr, unsigned int newsize) override
    {
        if (!ptr) return malloc(newsize);
        if (newsize == 0) { free(ptr); return nullptr; }
        
        Block* b = (Block*)((char*)ptr - sizeof(Block));
        unsigned int oldsize = b->size;
        newsize = align(newsize);
        
        if (newsize <= oldsize) return ptr;
        
        void* np = malloc(newsize);
        if (!np) return nullptr;
        my_memcpy(np, ptr, oldsize);
        free(ptr);
        return np;
    }
    
    void free(void* ptr) override
    {
        if (!ptr) return;
        
        Block* b = (Block*)((char*)ptr - sizeof(Block));
        if (b->magic != MAGIC) {
            printf("SegAlloc Double free or corruption\n");
            return;
        }
        
        b->free = 1;
        tracker.log_op("FREE", b->size, ptr);
        add_free(b);
        coalesce();
    }
    
    void show_heap() const override
    {
        printf("\nSegregated Heap Layout\n");
        Block* cur = head;
        int count = 0;
        
        while ((char*)cur < heap + HEAP_SIZE && count < 20) {
            printf("  Block %d: addr=%p, size=%u, free=%d\n",
                   count, cur, cur->size, cur->free);
            cur = (Block*)((char*)cur + sizeof(Block) + cur->size);
            count++;
        }
    }
    
    unsigned int free_space() const override
    {
        unsigned int total = 0;
        Block* cur = head;
        
        while ((char*)cur < heap + HEAP_SIZE) {
            if (cur->free) total += cur->size;
            cur = (Block*)((char*)cur + sizeof(Block) + cur->size);
        }
        return total;
    }
    
    unsigned int used_space() const override
    {
        unsigned int total = 0;
        Block* cur = head;
        
        while ((char*)cur < heap + HEAP_SIZE) {
            if (!cur->free) total += cur->size;
            cur = (Block*)((char*)cur + sizeof(Block) + cur->size);
        }
        return total;
    }
};
// Buddy Allocator

static const int BUDDY_SIZE = 8192;
static const int MIN_SIZE = 32;

struct BuddyNode
{
    int free;
    int level;
    BuddyNode* left;
    BuddyNode* right;
};

class BuddyAllocator : public Allocator
{
    char heap[BUDDY_SIZE];
    BuddyNode* root;
    MemoryTracker tracker;
    
    BuddyNode* make_node()
    {
        static int node_count = 0;
        static char node_pool[8192];
        
        if (node_count >= 256) return nullptr;
        return (BuddyNode*)(node_pool + node_count++ * sizeof(BuddyNode));
    }
    
    BuddyNode* build_tree(int level, unsigned int sz)
    {
        BuddyNode* n = make_node();
        if (!n) return nullptr;
        
        n->free = 1;
        n->level = level;
        n->left = nullptr;
        n->right = nullptr;
        
        if (level > 0) {
            n->left = build_tree(level - 1, sz / 2);
            n->right = build_tree(level - 1, sz / 2);
        }
        
        return n;
    }
    
    BuddyNode* find_buddy(BuddyNode* n, int level)
    {
        if (level == 0) return nullptr;
        if (n->left && n->right) {
            BuddyNode* l = find_buddy(n->left, level - 1);
            if (l) return l;
            return find_buddy(n->right, level - 1);
        }
        return nullptr;
    }
    
    BuddyNode* allocate_buddy(BuddyNode* n, int level)
    {
        if (!n || !n->free) return nullptr;
        
        if (level == 0) {
            n->free = 0;
            return n;
        }
        
        BuddyNode* res = allocate_buddy(n->left, level - 1);
        if (res) return res;
        
        return allocate_buddy(n->right, level - 1);
    }
    
    void free_buddy(BuddyNode* n)
    {
        if (!n) return;
        n->free = 1;
    }
    
    int size_at_level(int level)
    {
        return MIN_SIZE << level;
    }
    
public:
    BuddyAllocator()
    {
        root = build_tree(8, BUDDY_SIZE);
    }
    
    void* malloc(unsigned int sz) override
    {
        if (sz == 0) return nullptr;
        
        int level = 0;
        while (size_at_level(level) < (int)sz && level < 8) level++;
        
        if (level > 8) return nullptr;
        
        BuddyNode* node = allocate_buddy(root, level);
        if (!node) {
            printf("[BuddyAlloc] Out of memory\n");
            return nullptr;
        }
        
        tracker.log_op("ALLOC", sz, (void*)node);
        return (void*)node;
    }
    
    void* calloc(unsigned int count, unsigned int sz) override
    {
        unsigned int total = count * sz;
        void* ptr = malloc(total);
        if (ptr) my_memset(ptr, 0, total);
        return ptr;
    }
    
    void* realloc(void* ptr, unsigned int newsize) override
    {
        if (!ptr) return malloc(newsize);
        if (newsize == 0) { free(ptr); return nullptr; }
        
        void* np = malloc(newsize);
        if (!np) return nullptr;
        my_memcpy(np, ptr, newsize);
        free(ptr);
        return np;
    }
    
    void free(void* ptr) override
    {
        if (!ptr) return;
        
        BuddyNode* node = (BuddyNode*)ptr;
        free_buddy(node);
        tracker.log_op("FREE", node->level, ptr);
    }
    
    void show_heap() const override
    {
        printf("\n[Buddy Heap Layout]\n");
        printf("  Root level: 8, size: %u bytes\n", BUDDY_SIZE);
        printf("  Min block: %u bytes\n", MIN_SIZE);
    }
    
    unsigned int free_space() const override
    {
        return BUDDY_SIZE / 2;
    }
    
    unsigned int used_space() const override
    {
        return BUDDY_SIZE / 2;
    }
};

// Random number generator

static unsigned int rand_state = 12345;

static unsigned int my_rand()
{
    rand_state = rand_state * 1664525u + 1013904223u;
    return rand_state;
}

// Benchmark

static void run_test(Allocator& alloc, const char* name, int cycles, unsigned int max_sz)
{
    static const int SLOTS = 64;
    void* ptrs[SLOTS];
    unsigned int sizes[SLOTS];
    
    for (int i = 0; i < SLOTS; i++) { ptrs[i] = nullptr; sizes[i] = 0; }
    
    clock_t start = clock();
    
    for (int i = 0; i < cycles; i++) {
        int slot = my_rand() % SLOTS;
        
        if (ptrs[slot]) {
            alloc.free(ptrs[slot]);
            ptrs[slot] = nullptr;
            sizes[slot] = 0;
        } else {
            unsigned int sz = (my_rand() % max_sz) + 8;
            ptrs[slot] = alloc.malloc(sz);
            sizes[slot] = sz;
        }
    }
    
    for (int i = 0; i < SLOTS; i++)
        if (ptrs[i]) alloc.free(ptrs[i]);
    
    clock_t end = clock();
    double secs = (double)(end - start) / CLOCKS_PER_SEC;
    
    printf("\n  [%s]\n", name);
    printf("  Cycles: %d\n", cycles);
    printf("  Time  : %.4f s\n", secs);
    printf("  Free  : %u bytes\n", alloc.free_space());
    printf("  Used  : %u bytes\n", alloc.used_space());
}

// Menu

static void show_menu()
{
    printf("\n              *******         \n");
    printf("  MEMORY ALLOCATOR - Windows Version\n");
    printf("                *******         \n");
    printf("Allocators:\n");
    printf("  1. Segregated Free List\n");
    printf("  2. Buddy System\n");
    printf("        *******         \n");
    printf("Operations:\n");
    printf("  A. malloc\n");
    printf("  B. calloc\n");
    printf("  C. realloc\n");
    printf("  D. free\n");
    printf("  E. show heap\n");
    printf("  F. fragmentation report\n");
    printf("  G. run benchmark\n");
    printf("  H. switch allocator\n");
    printf("  Q. quit\n");
    printf("               ***              \n");
}

// Main

int main()
{
    SegAllocator seg;
    BuddyAllocator buddy;
    
    Allocator* allocs[2] = {&seg, &buddy};
    const char* names[2] = {"Segregated", "Buddy"};
    
    int cur = 0;
    
    static const int MAX_PTRS = 32;
    void* ptrs[MAX_PTRS];
    unsigned int ptrsz[MAX_PTRS];
    int ptr_count = 0;
    
    for (int i = 0; i < MAX_PTRS; i++) { ptrs[i] = nullptr; ptrsz[i] = 0; }
    
    printf("\nActive allocator: %s\n", names[cur]);
    
    char op;
    while (true) {
        show_menu();
        printf("Current: %s | Enter op: ", names[cur]);
        scanf(" %c", &op);
        
        if (op == 'Q' || op == 'q') break;
        
        Allocator* alloc = allocs[cur];
        
        if (op == 'H' || op == 'h') {
            printf("Choose (1=Seg, 2=Buddy): ");
            int ch;
            scanf("%d", &ch);
            if (ch >= 1 && ch <= 2) { cur = ch - 1; ptr_count = 0; }
            printf("Switched to: %s\n", names[cur]);
        }
        
        else if (op == 'A' || op == 'a') {
            if (ptr_count >= MAX_PTRS) { printf("Slot full\n"); continue; }
            unsigned int sz;
            printf("malloc size: ");
            scanf("%u", &sz);
            int i = ptr_count++;
            ptrs[i] = alloc->malloc(sz);
            ptrsz[i] = sz;
            if (ptrs[i])
                printf("[%d] malloc(%u) -> %p\n", i, sz, ptrs[i]);
            else
                printf("malloc failed\n");
        }
        
        else if (op == 'B' || op == 'b') {
            if (ptr_count >= MAX_PTRS) { printf("Slot full\n"); continue; }
            unsigned int cnt, sz;
            printf("calloc count: "); scanf("%u", &cnt);
            printf("calloc size : "); scanf("%u", &sz);
            int i = ptr_count++;
            ptrs[i] = alloc->calloc(cnt, sz);
            ptrsz[i] = cnt * sz;
            if (ptrs[i])
                printf("[%d] calloc(%u, %u) -> %p\n", i, cnt, sz, ptrs[i]);
            else
                printf("calloc failed\n");
        }
        
        else if (op == 'C' || op == 'c') {
            int idx;
            unsigned int new_sz;
            printf("realloc index [0-%d]: ", ptr_count - 1);
            scanf("%d", &idx);
            printf("new size: ");
            scanf("%u", &new_sz);
            if (idx < 0 || idx >= ptr_count) { printf("Invalid index\n"); continue; }
            
            void* newp = alloc->realloc(ptrs[idx], new_sz);
            if (newp) {
                ptrs[idx] = newp;
                ptrsz[idx] = new_sz;
                printf("[%d] realloc -> %p (size %u)\n", idx, newp, new_sz);
            } else {
                printf("realloc failed\n");
            }
        }
        
        else if (op == 'D' || op == 'd') {
            int idx;
            printf("free index [0-%d]: ", ptr_count - 1);
            scanf("%d", &idx);
            if (idx < 0 || idx >= ptr_count || !ptrs[idx]) {
                printf("Invalid index or already freed\n");
                continue;
            }
            alloc->free(ptrs[idx]);
            printf("[%d] freed %p\n", idx, ptrs[idx]);
            ptrs[idx] = nullptr;
            ptrsz[idx] = 0;
        }
        
        else if (op == 'E' || op == 'e') {
            alloc->show_heap();
        }
        
        else if (op == 'F' || op == 'f') {
            printf("\n--- Fragmentation: %s ---\n", names[cur]);
            alloc->show_fragmentation();
        }
        
        else if (op == 'G' || op == 'g') {
            printf("\nRunning benchmark (1000 cycles, max 128B)...\n");
            run_test(seg, "Segregated", 1000, 128);
            run_test(buddy, "Buddy", 1000, 128);
        }
        
        else {
            printf("Unknown op\n");
        }
    }
    
    printf("\nFinal state:\n");
    seg.show_heap();
    buddy.show_heap();
    
    return 0;
}