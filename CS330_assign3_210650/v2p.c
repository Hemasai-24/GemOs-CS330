#include <types.h>
#include <mmap.h>
#include <fork.h>
#include <v2p.h>
#include <page.h>

/* 
 * You may define macros and other helper functions here
 * You must not declare and use any static/global variables 
 * */


/**
 * mprotect System call Implementation.
 */

long free_pfn1(struct exec_context*current,u64 addr,int access){

        u64*pgdi=osmap(current->pgd);
        u64*new_add;u64*new_add1;u64*new_add3;u64*new_add5;u64* new_add6;u64*new_add4;u64*new_add2;
        u64 offset=(((addr)>>39));
        if((pgdi[offset]!=0) && ((pgdi[offset]&(0x1))==1)){
             new_add1=osmap((pgdi[offset])>>12);
        }else{
           return 0;      
        } 
        u64 offset1=((addr>>30)&0x1ff);
        if((new_add1[offset1]!=0) && ((new_add1[offset1]&(0x1))==1)){
             new_add2=osmap((new_add1[offset1])>>12);
        }else{
            return 0;
        }    
        u64 offset2=((addr>>21)&0x1ff);  
        if(((new_add2[offset2])!=0) && (((new_add2[offset2])&(0x1))==1)){
             new_add3=osmap((new_add2[offset2])>>12);
        }else{
            return 0;
        }
        u64 offset3=((addr>>12)&0x1ff);
        if(((new_add3[offset3])!=0) && ((new_add3[offset3]&(0x1))==1)){
            u64 k=((new_add3[offset3])>>12);
             if(access==PROT_READ){
        new_add3[offset3]=((k<<12)+ 0x11);
       }else {
        new_add3[offset3]=(k<<12)+0x19;
       }
        asm volatile("invlpg (%0)"::"r" (addr));
            
        }else return 0;
      return 0;
}
long vm_area_mprotect(struct exec_context *current, u64 addr, int length, int prot)
{
    length=(length/4096)*4096+(length%4096!=0)*4096;
    struct vm_area*prev=current->vm_area;
    struct vm_area*curr=current->vm_area;
    int f=0;
    while(curr!=NULL && length!=0){
        if((addr<curr->vm_start) && addr<curr->vm_end)addr=curr->vm_start;
        if(addr>=curr->vm_start && addr<curr->vm_end){
            if(addr>curr->vm_start && addr+length<curr->vm_end){
                struct vm_area*temp1=os_alloc(sizeof(struct vm_area));
                struct vm_area*temp2=os_alloc(sizeof(struct vm_area));
                temp2->vm_end=curr->vm_end;
                curr->vm_end=addr;
                temp1->vm_start=addr;
                temp1->vm_end=(temp1->vm_start)+length;
                temp1->access_flags=prot;
                for(u64 i=addr;i<addr+length;i=i+4096){
                       long p=free_pfn1(current,i,prot);
                }
                temp1->vm_next=temp2;
                temp2->vm_start=addr+length;
                temp2->access_flags=curr->access_flags;
                temp2->vm_next=curr->vm_next;
                curr->vm_next=temp1;
                stats->num_vm_area+=2;
                break;
            }
            else if(addr==curr->vm_start){
                if(addr+length>=curr->vm_end){
                    curr->access_flags=prot;
                    u64 p=length-(curr->vm_end)+(curr->vm_start);
                    if(addr==prev->vm_end && prev->access_flags==prot){
                        prev->vm_end=curr->vm_end;
                        struct vm_area*temp=curr;
                        prev->vm_next=curr->vm_next;
                        curr=curr->vm_next;
                        os_free(temp,sizeof(struct vm_area));
                        stats->num_vm_area--;
                    }else if(curr->vm_next!=NULL && addr+length==curr->vm_next->vm_start && curr->vm_next->access_flags==prot){
                        curr->vm_next->vm_start=addr;
                        struct vm_area*temp=curr;
                        prev->vm_next=curr->vm_next;
                        curr=curr->vm_next;
                        os_free(temp,sizeof(struct vm_area));
                        stats->num_vm_area--;
                    }  for(u64 i=addr;i<curr->vm_end;i=i+4096){
                       long p1=free_pfn1(current,i,prot);
                    }
                    addr=addr+length;
                    length=p;                 
                }else if(addr+length<curr->vm_end){
                   // curr->vm_end=addr;
                    if(prev->access_flags==prot){
                        prev->vm_end=addr+length;
                        curr->vm_start=addr+length;
                    }else {
                     struct vm_area*temp=os_alloc(sizeof(struct vm_area));
                     temp->vm_start=addr;
                     temp->vm_end=temp->vm_start+length;
                     temp->access_flags=prot;
                     temp->vm_next=curr;
                     curr->vm_start=addr+length;
                     prev->vm_next=temp;
                     stats->num_vm_area++;
                    } for(u64 i=addr;i<addr+length;i=i+4096){
                       long p=free_pfn1(current,i,prot);
                    }
                    addr=addr+length;
                    length=length-(curr->vm_end)+(curr->vm_start);                
                }
            }else if(addr<curr->vm_end && addr+length>=curr->vm_end){
                u64 p=curr->vm_end;
                curr->vm_end=addr;
                if(prot==curr->vm_next->access_flags){
                    curr->vm_next->vm_start=addr;
                }else{
                     struct vm_area*temp=os_alloc(sizeof(struct vm_area));
                     temp->vm_start=addr;
                     temp->vm_end=temp->vm_start+length;
                     temp->access_flags=prot;
                     temp->vm_next=curr->vm_next;
                     curr->vm_next=temp;
                     stats->num_vm_area++;
                }for(u64 i=addr;i<curr->vm_end;i=(i+4096)){
                       long p=free_pfn1(current,i,prot);
                    }
                length=length-p+addr;
                addr=p;   
            }
        }
        prev=curr;
        curr=curr->vm_next;
    }
     prev=current->vm_area;
    curr=current->vm_area->vm_next;
    while(curr!=NULL){
        if(prev->vm_end==curr->vm_start && prev->access_flags==curr->access_flags){
            struct vm_area*temp=curr;
            prev->vm_end=curr->vm_end;
            prev->vm_next=curr->vm_next;
            os_free(temp,sizeof(struct vm_area));
            stats->num_vm_area--;
            curr=prev;
        }prev=curr;
        curr=curr->vm_next;
    }
    return 0;
}

/**
 * mmap system call implementation.
 */

long vm_area_map(struct exec_context *current, u64 addr, int length, int prot, int flags){
    if(addr!=0 && (addr<MMAP_AREA_START||addr>MMAP_AREA_END))return -1;
    if(current->vm_area==NULL){
        struct vm_area*temp=(struct vm_area*)os_alloc(sizeof(struct vm_area));
        stats->num_vm_area++;
        temp->vm_start=MMAP_AREA_START;
        temp->vm_end=MMAP_AREA_START+4096;
        temp->access_flags=0;
        temp->vm_next=NULL;
        current->vm_area=temp;
    }
    length=(length/4096)*4096+(length%4096!=0)*4096;

    if(flags==MAP_FIXED){
        if(addr==0)return -1;
        else {
            struct vm_area*prev=current->vm_area;
            struct vm_area*curr=current->vm_area;
            while(curr!=NULL){
                //address is partially or fully mapped
                if(addr>=curr->vm_start && addr<=((curr->vm_end)-1))return -1;
                if(prev!=NULL && prev->vm_end<=addr && curr->vm_start>addr){
                    //insert the vm_area here;
                    if(addr==prev->vm_end && prev->access_flags==prot){
                        prev->vm_end==addr+length;
                        return addr;
                    }else if(addr+length==curr->vm_start && curr->access_flags==prot){
                        curr->vm_start==addr;
                        return addr;
                    }else {
                        //insert new_vma;
                        struct vm_area*vma=(struct vm_area*)os_alloc(sizeof(struct vm_area));                         
                        stats->num_vm_area++;
                        vma->vm_start=addr;
                        vma->vm_end=vma->vm_start+length;
                        vma->access_flags=prot;
                        prev->vm_next=vma;
                        vma->vm_next=curr;
                        return addr;
                    }

                }
                prev=curr;
                curr=curr->vm_next;
                if(curr==NULL && addr==prev->vm_end && prev->access_flags==prot){
                prev->vm_end=prev->vm_end+length;
                return addr;  
                }
                else if(curr==NULL && addr>prev->vm_end){
                    struct vm_area*vma=(struct vm_area*)os_alloc(sizeof(struct vm_area));                         
                        stats->num_vm_area++;
                        vma->vm_start=addr;
                        vma->vm_end=vma->vm_start+length;
                        vma->access_flags=prot;
                        prev->vm_next=vma;
                        vma->vm_next=curr;
                        return addr;
                }
            }
        }
    }int p=0;
    if(addr!=0 && flags==0){
            struct vm_area*prev=current->vm_area;
            struct vm_area*curr=current->vm_area;
            while(curr!=NULL){
                if(curr->vm_start<=addr && curr->vm_end-1>=addr)p=1;break;
                if(prev!=NULL && prev->vm_end<=addr && curr->vm_start>addr){
                    if(addr==prev->vm_end && prev->access_flags==prot){
                        prev->vm_end==addr+length;
                        return addr;
                    }else if(addr+length==curr->vm_start && curr->access_flags==prot){
                        curr->vm_start==addr;
                        return addr;
                    }else {
                        struct vm_area*vma=(struct vm_area*)os_alloc(sizeof(struct vm_area));                         
                        stats->num_vm_area++;
                        vma->vm_start=addr;
                        vma->vm_end=vma->vm_start+length;
                        vma->access_flags=prot;
                        prev->vm_next=vma;
                        vma->vm_next=curr;
                        return addr;
                    }

                }
                prev=curr;
                curr=curr->vm_next;
                  if(curr==NULL && addr==prev->vm_end && prev->access_flags==prot){
                prev->vm_end=prev->vm_end+length;
                return addr;  
                }
                else if(curr==NULL && addr>prev->vm_end){
                    struct vm_area*vma=(struct vm_area*)os_alloc(sizeof(struct vm_area));                         
                        stats->num_vm_area++;
                        vma->vm_start=addr;
                        vma->vm_end=vma->vm_start+length;
                        vma->access_flags=prot;
                        prev->vm_next=vma;
                        vma->vm_next=curr;
                        return addr;
                }   
            }
            }
    if(addr==0 || p==1){
            struct vm_area*prev=current->vm_area;
            struct vm_area*curr=current->vm_area;
            while(curr!=NULL){
                if(prev!=NULL && curr->vm_start>prev->vm_end){
                    if(prev->access_flags==prot){
                        prev->vm_end==prev->vm_end+length;
                        return prev->vm_end-length;
                    }else if(curr->access_flags==prot){                  
                        curr->vm_start=curr->vm_start-length;
                        return curr->vm_start;
                    }else {
                        struct vm_area*vma=(struct vm_area*)os_alloc(sizeof(struct vm_area));                         
                        stats->num_vm_area++;
                        vma->vm_start=prev->vm_end;
                        vma->vm_end=vma->vm_start+length;
                        vma->access_flags=prot;
                        prev->vm_next=vma;
                        vma->vm_next=curr;
                        return vma->vm_start;
                    }
                }
                prev=curr;
                curr=curr->vm_next;
                if(curr==NULL ){
                    if(prev->access_flags==prot){
                     prev->vm_end=prev->vm_end+length;
                     return prev->vm_end-length;}
                    else{
                     struct vm_area*vma=(struct vm_area*)os_alloc(sizeof(struct vm_area));                         
                        stats->num_vm_area++;
                        vma->vm_start=prev->vm_end;
                        vma->vm_end=vma->vm_start+length;
                        vma->access_flags=prot;
                        prev->vm_next=vma;
                        vma->vm_next=curr;
                        return vma->vm_start;
                    }
                }
            }
    }  
    }
/**
 * munmap system call implemenations
 */

long free_pfn(struct exec_context*current,u64 addr){
        u64*pgdi=osmap(current->pgd);
        u64*new_add;u64*new_add1;u64*new_add3;u64*new_add5;u64* new_add6;u64*new_add4;u64*new_add2;
        u64 offset=(((addr)>>39));
        if((pgdi[offset]!=0) && ((pgdi[offset]&(0x1))==1)){
             new_add1=osmap((pgdi[offset])>>12);
        }else{
           return 0;      
        } printk("v");
        u64 offset1=((addr>>30)&0x1ff);
        if((new_add1[offset1]!=0) && ((new_add1[offset1]&(0x1))==1)){
             new_add2=osmap((new_add1[offset1])>>12);
        }else{
            return 0;
        }     printk("u");
        u64 offset2=((addr>>21)&0x1ff);  
        if(((new_add2[offset2])!=0) && (((new_add2[offset2])&(0x1))==1)){
             new_add3=osmap((new_add2[offset2])>>12);
        }else{
            return 0;
        }printk("w");
        u64 offset3=((addr>>12)&0x1ff);
        if(new_add3[offset3]!=0 && ((((new_add3[offset3])&(0x1))==1))){
            printk("9");
            put_pfn((new_add3[offset3])>>12);
            //present bit=0
            new_add3[offset3]=0;
            if((get_pfn_refcount(new_add3[offset3]>>12))==0){
             os_pfn_free(USER_REG,new_add3[offset3]>>12);
            } asm volatile("invlpg (%0)"::"r" (addr));
        }else return 1;
      return 1;
}
long vm_area_unmap(struct exec_context *current, u64 addr, int length)
{        
    length=(length/4096)*4096+(length%4096!=0)*4096;
    struct vm_area*prev=current->vm_area;
    struct vm_area*curr=current->vm_area;

    while(curr!=NULL && length!=0){
        if(addr<curr->vm_start && addr<curr->vm_end && addr+length>curr->vm_start){
            addr=curr->vm_start;
        }
        if(addr>=curr->vm_start && addr<curr->vm_end){
            if(addr>curr->vm_start && addr+length<curr->vm_end){
                struct vm_area*temp=os_alloc(sizeof(struct vm_area));
                curr->vm_end=addr;
                temp->vm_start=addr;
                temp->vm_end=temp->vm_start+length;
                temp->access_flags=curr->access_flags;
                temp->vm_next=curr->vm_next;
                curr->vm_next=temp;
                stats->num_vm_area++;
                
                for(u64 i=addr;i<addr+length;i=i+4096){
                  long p=free_pfn(current,i);
                }
                return 0;
            }
            if(addr==curr->vm_start){
                if(addr+length>=curr->vm_end){
                    struct vm_area*temp=curr;
                    prev->vm_next=curr->vm_next;
                    curr=curr->vm_next;
                    os_free(temp,sizeof(struct vm_area));
                    stats->num_vm_area--;
                    for(u64 i=addr;i<curr->vm_end;i=i+4096){
                       long p=free_pfn(current,i);
                    }
                    addr=curr->vm_end;
                    length=length-curr->vm_end+curr->vm_start;
                    
                }else if(addr+length<curr->vm_end){
                    for(u64 i=addr;i<addr+length;i=i+4096){
                       long p=free_pfn(current,i);
                    }
                    curr->vm_start=addr+length;
                    return 0;
                }
            }else if(addr<curr->vm_end && addr+length>=curr->vm_end){
                u64 p=curr->vm_end;
                curr->vm_end=addr;length=length-p+addr;
                addr=p;
               for(u64 i=addr;i<curr->vm_end;i=i+4096){
                       long p=free_pfn(current,i);
                    }
            }
        }prev=curr;
        curr=curr->vm_next;
    }
    
    return 0;
}



/**
 * Function will invoked whenever there is page fault for an address in the vm area region
 * created using mmap
 */

long vm_area_pagefault(struct exec_context *current, u64 addr, int error_code)
{   
    if((error_code!=6) && (error_code!=7) && (error_code!=7))return -1;
    struct vm_area*curr=current->vm_area;
    u64 vm_exist=0;u64 access_flag=0;
    while(curr!=NULL){
        if(addr>=curr->vm_start && addr<curr->vm_end){
           vm_exist=1;
           access_flag=curr->access_flags;
           break;
        }curr=curr->vm_next;
    }
    if(vm_exist==0)return -1;

    if(vm_exist==1){
        if((error_code==0x7 || error_code==0x6) && curr->access_flags==PROT_READ)return -1;
        if(error_code==0x7  && curr->access_flags==PROT_READ|PROT_WRITE){
            handle_cow_fault(current,addr,curr->access_flags);
        }
        if(error_code==0x4 || error_code==0x6){
        u64 flag=0;     
        if(error_code==0x4)flag=PROT_READ;
        else flag=(PROT_READ|PROT_WRITE);
       
        u64*pgdi=osmap(current->pgd);
        u64*new_add;u64*new_add1;u64*new_add3;u64*new_add5;u64* new_add6;u64*new_add4;u64*new_add2;
        u64 offset=(((addr)>>39)&0x1ff);

        if((pgdi[offset]!=0) && (pgdi[offset]&0x1==1)){
             new_add1=osmap(pgdi[offset]>>12);
        }else{
            u64 k=os_pfn_alloc(OS_PT_REG);
            pgdi[offset]=((k)<<12) + 0x19;
            new_add1=osmap(k);       
        } 
        u64 offset1=((addr>>30)&0x1ff);
        if((new_add1[offset1]!=0) && (new_add1[offset1]&0x1==1)){
             new_add2=osmap(new_add1[offset1]>>12);
        }else{
            u64 k=os_pfn_alloc(OS_PT_REG);
            new_add1[offset1]=((k)<<12) + 0x19;
            new_add2=osmap(k);       
        }      
        u64 offset2=((addr>>21)&0x1ff);
        if((new_add2[offset2]!=0) && (new_add2[offset2]& 0x1==1)){
             new_add3=osmap(new_add2[offset2]>>12);
        }else{
            u64 k=os_pfn_alloc(OS_PT_REG);
            new_add2[offset2]=((k)<<12) + 0x19;
            new_add3=osmap(k);       
        }

       u64 offset3=((addr>>12)& 0x1ff);
       u64 k=os_pfn_alloc(USER_REG);
       if(flag==PROT_READ){
        new_add3[offset3]=(k<<12)+0x11;
       }else {
        new_add3[offset3]=(k<<12)+0x19;
       }
       return 1;
    }
    }return -1;
}
 


/**
 * cfork system call implemenations
 * The parent returns the pid of child process. The return path of
 * the child process is handled separately through the calls at the 
 * end of this function (e.g., setup_child_context etc.)
 */

long do_cfork(){
    u32 pid;
    struct exec_context *new_ctx = get_new_ctx();
    struct exec_context *ctx = get_current_ctx();
     /* Do not modify above lines
     * 
     * */   
     /*--------------------- Your code [start]---------------*/
     

     /*--------------------- Your code [end] ----------------*/
    
     /*
     * The remaining part must not be changed
     */
    copy_os_pts(ctx->pgd, new_ctx->pgd);
    do_file_fork(new_ctx);
    setup_child_context(new_ctx);
    return pid;
}



/* Cow fault handling, for the entire user address space
 * For address belonging to memory segments (i.e., stack, data) 
 * it is called when there is a CoW violation in these areas. 
 *
 * For vm areas, your fault handler 'vm_area_pagefault'
 * should invoke this function
 * */

long handle_cow_fault(struct exec_context *current, u64 vaddr, int access_flags)
{
  return -1;
}
