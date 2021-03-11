#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"
#include "fcntl.h"


#define min(a, b) ((a) < (b) ? (a) : (b))


static struct mmap_node* getfreemmnode(struct proc *p)
{
    for(int i=0; i<MMNODES; ++i) {
        if(p->mmap_nodes[i].startva == 0) 
            return &p->mmap_nodes[i];
    }

    panic("getfreemmnode");
    return 0;
}

// free mmap_node 
uint64 mmap(int length, int prot, int flags, struct file *f, int offset)
{
    printf("mmap\n");
    
    int pteprot = 0;
    if(prot & PROT_READ) {
        if(f->readable == 0) return -1;
        pteprot |= PTE_R;
    }
    if(prot & PROT_WRITE) {
        if(f->writable == 0 && (flags & MAP_SHARED)) return -1;
        pteprot |= PTE_W;
    }
    if(prot & PROT_EXEC) pteprot |= PTE_X;

    struct proc *p = myproc();
    int sz = PGROUNDUP(length);

    // acquire(&p->lock);
    // if(mappages(p->pagetable, p->mmap_addr - sz, sz,
    //           (uint64)p->ofile[fd], PTE_R) < 0){
    // uvmfree(pagetable, 0);

    // release(&p->lock);
    acquire(&p->lock);
    struct mmap_node* mmn = getfreemmnode(p);
    p->mmap_addr = mmn->startva = p->mmap_addr - sz;


    release(&p->lock);
    mmn->length = length;
    mmn->prot = pteprot;
    mmn->flags = flags;
    mmn->f = f;
    mmn->offset = offset;
    filedup(mmn->f);
    
    return p->mmap_addr;
}

uint64 munmap(uint64 addr, int length)
{
    if((addr % PGSIZE) != 0)
        panic("munmap: not aligned");

    struct proc *p = myproc();
    struct mmap_node *n = 0;
    struct mmap_node *fn = 0;
    // uint64 leastva = TRAPFRAME;
    for(int i=0; i<MMNODES; ++i) {
        n = &p->mmap_nodes[i];
        if(n->startva == 0) continue;

        if(addr >= n->startva && addr < n->startva + n->length) 
        {
            fn = n;
            break;
        }
        // if(n->startva < leastva) leastva = n->startva;
    }

    uint64 va;
    uint64 npages;
    length = PGROUNDUP(length);
    if(addr == fn->startva && length == fn->length)
    {
        printf("munmap case 1\n");
        va = addr;
        npages = (uint64)length / PGSIZE;

        fn->startva = 0;
        fn->length = 0;
    } else if(addr == fn->startva && length < fn->length)
    {
        printf("munmap case 2\n");
        va = addr;
        npages = (uint64)length / PGSIZE;

        fn->startva = addr + length;   
        fn->length -= length;
        fn->offset += length;
    } else if(addr > n->startva && addr + length == fn->startva + fn->length)
    {
        panic("munmap 3");
    } else 
    {
        panic("munmap 4");
    }

    // acquire(&p->lock);
    // if(fn->startva == p->mmap_addr) p->mmap_addr = leastva;
    // release(&p->lock);

    // free pagetable, write back to file
    uint64 a;
    pte_t *pte;
    for(a = va; a < va + npages*PGSIZE; a += PGSIZE){
        if((pte = walk(p->pagetable, a, 0)) == 0)
        continue;
        
        if((*pte & PTE_V) == 0)
        continue;
        
        if(PTE_FLAGS(*pte) == PTE_V)
            panic("uvmunmap: not a leaf");
        
        uint64 off = (a - fn->startva) + (uint64)(fn->offset);
        uint64 pa = PTE2PA(*pte);
        if(fn->flags & MAP_SHARED) {
            begin_op();
            ilock(fn->f->ip);
            int r = 0;
            if ((r = writei(fn->f->ip, 0, pa, off, PGSIZE)) < 0)
                panic("uvmunmap.writei");
            iunlock(fn->f->ip);
            end_op();
        }

        kfree((void*)pa);
        *pte = 0;
    }

    if(fn->startva == 0) {
        fileclose(fn->f);
        fn->f = 0;
    }

    return 0;
}

int mmap_load(uint64 addr)
{
    printf("mmap_load: %p\n", addr);
    struct proc *p = myproc();
    int found = 0;

    struct mmap_node *n = 0;
    for(int i=0; i<MMNODES; ++i) {
        n = &p->mmap_nodes[i];
        if(addr >= n->startva && addr < n->startva + n->length) 
        {
            found = 1;
            break;
        }
    }
    if(found == 0) return -1;

    uint64 va = PGROUNDDOWN(addr);
    char* pa = kalloc();
    if(pa == 0)
      panic("mmap_load.kalloc");
    memset(pa, 0, PGSIZE);

    ilock(n->f->ip);
    uint off = (uint) (va - n->startva + n->offset);
    uint sz = min(PGSIZE, (n->length - off));
    int r;
    if((r =readi(n->f->ip, 0, (uint64)pa, off, sz)) < 0) 
        panic("mmap_load.readi");
    iunlock(n->f->ip);

    if(mappages(p->pagetable, va, PGSIZE,
              (uint64)pa, PTE_U | n->prot) != 0) {
        panic("mmap_load.mappages");
    }

    return 0;
}

