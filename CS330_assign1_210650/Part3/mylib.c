#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>

struct fchunk {
    size_t size;
    struct fchunk* prev;
    struct fchunk* next;
};
struct fchunk* freelist = NULL;

void split(struct fchunk* chunk, long unsigned size,size_t totalsize) { 
    struct fchunk* newchunk = (struct fchunk*)((char*)chunk + size);
    newchunk->size = totalsize - size;
    newchunk->next = freelist;
    newchunk->prev = NULL;
    chunk->size =size;
    freelist=newchunk;
}

void* memalloc(unsigned long size) {
    printf("memalloc() called\n");
    if (size == 0) return NULL;
    size=((size+8+7)/8)*8;
    if(size<24)size=24;
    struct fchunk* chunk = freelist;
    struct fchunk* pchunk = NULL;
    while (chunk != NULL) {
        if (chunk->size >= size) {
            size_t b = chunk->size - size;
            if (b > 0) {
                struct fchunk* newchunk = (struct fchunk*)((void*)chunk + size);
                if (pchunk != NULL) {
                    pchunk->next = chunk->next;
                }
                else {
                freelist=(struct fchunk*)((void*)chunk+size);
                }
                freelist->size = b;
                freelist->next = chunk->next;
                freelist->prev = NULL;
                chunk->size = size;}   
                return ((void*)chunk) + 8;
            } else {
                if (pchunk != NULL) {
                    pchunk->next = chunk->next;
                } else {
                    freelist = chunk->next;
                }
                return ((void*)chunk) + 8;
            }

        pchunk = chunk;
        chunk = chunk->next;
    }
    size_t totalsize = (size/(4*1024*1024))*(4*1024*1024)+(size%(4*1024*1024)!=0)*(4*1024*1024) +(4*1024*1024) ;
    void* newchunk = (struct fchunk*)mmap(NULL, totalsize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    struct fchunk*nchunk=(struct fchunk*)newchunk;
    if (newchunk == MAP_FAILED) {
        return NULL;
    }
    nchunk->size = size;
    nchunk->next = NULL;
    nchunk->prev = NULL;
    
    if (totalsize > size) {
        split(nchunk, size , totalsize);
    }
    
    return ((void*)nchunk) + 8;
}

int memfree(void* ptr) {
    printf("memfree() called\n");
     if (ptr == NULL) {
      return -1;
     }
   void* lchunk = NULL;
   void* rchunk = NULL;
   void* c = ptr-8;
   size_t size = *((unsigned long*)(ptr-8));
   
   struct fchunk*chunk=freelist;
   
   while(chunk != NULL){
   size_t s = *((unsigned long*)chunk);
   if(chunk + s + 1 == c) {
   lchunk = chunk;
   break;
   }
   else chunk=chunk->next;
   }
   chunk = freelist;
   while(chunk != NULL){
   size_t s = *((unsigned long*)chunk);
   if(chunk == c + size +1) {
   rchunk = chunk;
   break;
   }
   else chunk=chunk->next;
   }
   if( lchunk != NULL && rchunk != NULL){
   struct fchunk* temp = c;
   struct fchunk* l = lchunk;
   struct fchunk* r = rchunk;
   temp->size= size + *((unsigned long*)l) + *((unsigned long*)r);
   temp->next = freelist;
   temp->prev = NULL;
   freelist = temp;
   }
   else if( lchunk == NULL && rchunk == NULL){
   struct fchunk* temp = c;
   temp->next = freelist;
   temp->prev = NULL;
   freelist = temp;
   } 
   else if( lchunk != NULL && rchunk == NULL){
   struct fchunk* temp = c;
   struct fchunk* l = lchunk;
   temp->size= size + *((unsigned long*)l);
   temp->next = freelist;
   temp->prev = NULL;
   freelist = temp;
   } 
   else if( lchunk == NULL && rchunk != NULL){
   struct fchunk* temp = c;
   struct fchunk* r = rchunk;
   temp->size= size + *((unsigned long*)r); 
   temp->next = freelist;
   temp->prev = NULL;
   freelist = temp;
   }   
  return 0;
}
