#include<context.h>
#include<memory.h>
#include<lib.h>
#include<entry.h>
#include<file.h>
#include<tracer.h>


///////////////////////////////////////////////////////////////////////////
//// 		Start of Trace buffer functionality 		      /////
///////////////////////////////////////////////////////////////////////////

int is_valid_mem_range(unsigned long buff, u32 count, int access_bit) 
{
	struct exec_context*ctx=get_current_ctx();
	for(int i=0;i<MAX_MM_SEGS;i++){
	struct mm_segment s=ctx->mms[i];
	if(buff>=s.start && buff<=s.next_free-1 && buff+count-1>=s.start && buff+count-1<=s.end-1){
		if((i==MM_SEG_CODE) || (i==MM_SEG_RODATA)){
			if(access_bit==0)return 1;
			if(access_bit==1)return 0;
		}
		if(i==MM_SEG_DATA)return 1;
	}if(buff>=s.start && buff<=s.end-1){
		if(i==MM_SEG_STACK)return 1;
	}
	}
	struct vm_area*vm=ctx->vm_area;
	while(vm!=NULL){
		if(buff>=vm->vm_start && buff<=(vm->vm_end)-1 && buff+count-1>=vm->vm_start && buff+count-1<=(vm->vm_end)-1) {
			if(access_bit==0 && vm->access_flags&2){
				return 1;}
			if(access_bit==1 && vm->access_flags&1){
				return 1;}
		}vm=vm->vm_next;
	}
	return 0;
}



long trace_buffer_close(struct file *filep)
{
    if(filep==NULL|| filep->type!=TRACE_BUFFER||filep->trace_buffer==NULL ||filep->fops==NULL)return -EINVAL;
	if(filep->trace_buffer!=NULL)os_page_free(USER_REG,(void*)filep->trace_buffer);
	if(filep->fops!=NULL)os_free((void*)filep->fops,USER_REG);
	os_page_free(USER_REG,(void*)filep);
	return 0;
}



int trace_buffer_read(struct file *filep, char *buff, u32 count)
{
	if(!is_valid_mem_range((unsigned long)buff,count,1)){
     return -EBADMEM;
	}
    if(filep==NULL|| filep->type!=TRACE_BUFFER)return -EINVAL;
	if(filep->mode!=O_READ && filep->mode!=O_RDWR)return -EINVAL;
	struct trace_buffer_info* trace_buffer=filep->trace_buffer;
	u32 data_avl=(trace_buffer->write - trace_buffer->read );
	if(count<0)return -EINVAL;
	 if(count>data_avl)count=data_avl;
	if(count==0)return 0;
	for(u32 i=0;i<count;i++){
		buff[i]=(char)((trace_buffer->data)[((trace_buffer->read) + i)%(u32)TRACE_BUFFER_MAX_SIZE]);
	}
	trace_buffer->read=(trace_buffer->read+count)%TRACE_BUFFER_MAX_SIZE;
	return count;
}

int trace_buffer_write(struct file*filep, char*buff, u32 count)
{ 	
	if(!is_valid_mem_range((unsigned long)buff,count,1)){
     return -EBADMEM;
	}
	if(filep==NULL|| filep->type!=TRACE_BUFFER)return -EINVAL;
	if(filep->mode!=O_WRITE && filep->mode!=O_RDWR)return -EINVAL;
	struct trace_buffer_info* trace_buffer=filep->trace_buffer;
	u32 space_left = (TRACE_BUFFER_MAX_SIZE - (trace_buffer->write - trace_buffer->read));
	if(space_left!=TRACE_BUFFER_MAX_SIZE)space_left=space_left%(u32)TRACE_BUFFER_MAX_SIZE;
	if(space_left>TRACE_BUFFER_MAX_SIZE)space_left=(space_left+1)%(TRACE_BUFFER_MAX_SIZE);
	if(count<0)return -EINVAL;
	if(count>space_left)count=space_left;
	if(count==0)return 0;
	for(u32 i=0;i<count;i++){
		(trace_buffer->data)[((trace_buffer->write)+i)%(u32)TRACE_BUFFER_MAX_SIZE]=buff[i];	
	}	
	trace_buffer->write=((trace_buffer->write)+count)%TRACE_BUFFER_MAX_SIZE;
	if(count==TRACE_BUFFER_MAX_SIZE)trace_buffer->write=((trace_buffer->write)+count);
    return count;
}

int sys_create_trace_buffer(struct exec_context *current, int mode)
{ 
	if(mode!=O_READ && mode!=O_WRITE && mode!=O_RDWR)return -EINVAL;
	u32 fd=0;
    for(fd=0;fd<MAX_OPEN_FILES;fd++){
		if(current->files[fd]==NULL){
			break;  
		}
	}
	if(fd>=MAX_OPEN_FILES)return -EINVAL;
	struct file*fobj=(struct file*)os_page_alloc((u32)USER_REG);
	struct trace_buffer_info*trace_buffer=(struct trace_buffer_info*)os_page_alloc(USER_REG);
	if(fobj==NULL)return -ENOMEM;
	trace_buffer->read=(u32)0;
	trace_buffer->write=(u32)0;
	trace_buffer->data=(char*)(os_page_alloc(USER_REG));
	fobj->type=TRACE_BUFFER;
	fobj->mode=mode;
	fobj->offp=0;
	fobj->ref_count++;
	fobj->inode=NULL;
	fobj->trace_buffer=trace_buffer;
	fobj->fops=(struct fileops*)os_alloc(USER_REG);
	if(fobj->fops==NULL){
		os_page_free(USER_REG,(void*)(fobj->trace_buffer));
		current->files[fd]=NULL;
		return -ENOMEM;
	}
    fobj->fops->read=trace_buffer_read;
	fobj->fops->write=trace_buffer_write;
	fobj->fops->lseek=NULL;
	fobj->fops->close=trace_buffer_close;
	current->files[fd]=fobj;
	return fd;
}

///////////////////////////////////////////////////////////////////////////
//// 		Start of strace functionality 		      	      /////
///////////////////////////////////////////////////////////////////////////

void itc(char*buff, u64 num) {
  for(int i = 0; i < 8; i++){ buff[i] =  (num >> ((i)*8)) & 0xFF;
  }
}
int perform_tracing(u64 syscall_num, u64 param1, u64 param2, u64 param3, u64 param4)
{   if(syscall_num==38 || syscall_num==37)return 0;
	struct exec_context*current=get_current_ctx();
	if(!current->st_md_base)return 0;
	if(!(current->st_md_base->is_traced)){return 0;}
	int flag=0;
	if((current->st_md_base)->tracing_mode==FILTERED_TRACING){
    struct strace_info*curr=current->st_md_base->next;
	while(curr!=NULL){
        if(curr->syscall_num==syscall_num){flag=1;}
		curr=curr->next;
	}
	}
	if((current->st_md_base)->tracing_mode==FULL_TRACING){flag=1;}
    int fd=current->st_md_base->strace_fd;
	if(flag==1){
    if(syscall_num==2||syscall_num==10||syscall_num==11||syscall_num==13||syscall_num==15||syscall_num==20||syscall_num==21||syscall_num==22||syscall_num==61){
	char buff[8];
	itc(buff,syscall_num);
    int cnt=trace_buffer_write(current->files[fd],buff,8);
	}
	else if(syscall_num==1||syscall_num==6||syscall_num==7||syscall_num==12||syscall_num==14||syscall_num==19||syscall_num==27||syscall_num==29||syscall_num==36){
	char buff[8];
	itc(buff,syscall_num);
    int cnt=trace_buffer_write(current->files[fd],buff,8);
	itc(buff,param1);
	cnt=trace_buffer_write(current->files[fd],buff,8);}
    else if(syscall_num==4||syscall_num==8||syscall_num==9||syscall_num==17||syscall_num==23||syscall_num==28||syscall_num==37||syscall_num==40){
	char buff[8];
	itc(buff,syscall_num);
    int cnt=trace_buffer_write(current->files[fd],buff,8);
	itc(buff,param1);
	cnt=trace_buffer_write(current->files[fd],buff,8);
	itc(buff,param2);
	cnt=trace_buffer_write(current->files[fd],buff,8);
	}
    else if(syscall_num==5||syscall_num==18||syscall_num==24||syscall_num==25||syscall_num==30||syscall_num==39||syscall_num==41){
	char buff[8];
	itc(buff,syscall_num);
    int cnt=trace_buffer_write(current->files[fd],buff,8);
    itc(buff,param1);
	cnt=trace_buffer_write(current->files[fd],buff,8);
	itc(buff,param2);
	cnt=trace_buffer_write(current->files[fd],buff,8);
	itc(buff,param3);
	cnt=trace_buffer_write(current->files[fd],buff,8);
	}
    else if(syscall_num==16||syscall_num==35){
	char buff[8];
	itc(buff,syscall_num);
    int cnt=trace_buffer_write(current->files[fd],buff,8);
	itc(buff,param1);
	cnt=trace_buffer_write(current->files[fd],buff,8);
	itc(buff,param2);
	cnt=trace_buffer_write(current->files[fd],buff,8);
	itc(buff,param3);
	cnt=trace_buffer_write(current->files[fd],buff,8);
	itc(buff,param4);
	cnt=trace_buffer_write(current->files[fd],buff,8);
	}
	   struct strace_info*sys_data=os_alloc(USER_REG);
	   //sys_data=current->st_md_base->next;
	   sys_data->syscall_num=syscall_num;
	   current->st_md_base->next=sys_data;
	   if(current->st_md_base->last) {
            current->st_md_base->last->next = sys_data ;
            current->st_md_base->last = sys_data;
        } else {
            current->st_md_base->next = sys_data;
            current->st_md_base->last = sys_data;
        }
        current->st_md_base->count++;}
    return 0;
}

int sys_strace(struct exec_context *current, int syscall_num, int action){
printk("sys_strace start");
		if(action==ADD_STRACE){
         struct strace_info*temp=(struct strace_info*)os_page_alloc(USER_REG);
		 if(temp==NULL)return -EINVAL;
		 temp->syscall_num=syscall_num;
		 if(current->st_md_base->last){
		 current->st_md_base->last->next=temp;
		 current->st_md_base->last=temp;
		 }else {
			current->st_md_base->next=temp;
			current->st_md_base->last=temp;
		 }(current->st_md_base)->count++;
		}
		else if(action==REMOVE_STRACE){
            struct strace_info*prev=NULL;
			struct strace_info*curr=current->st_md_base->next;
			while(curr!=NULL){
				if(curr->syscall_num==syscall_num){
					if(prev){
						prev->next=curr->next;
					}else{
						current->st_md_base->next=curr->next;
					}
					if(current->st_md_base->last==curr)current->st_md_base->last==prev;
					os_page_free(USER_REG,(void*)curr);
					current->st_md_base->count--;
					break;
				}prev=curr;
				curr=curr->next;
			}
		}else return -EINVAL;
	return 0;
}

int sys_read_strace(struct file *filep, char *buff, u64 count)
{  
    int bytes=0;
    struct exec_context*current=get_current_ctx();
	if(filep==NULL ||filep->type!=TRACE_BUFFER ){return -EINVAL;}
	if(count<=0){return -EINVAL;}
	if(count>MAX_STRACE){count=MAX_STRACE;}
    struct trace_buffer_info*trace_buffer=filep->trace_buffer;
	if(trace_buffer==NULL){return -EINVAL;}
	while(count>0 && trace_buffer->read!=trace_buffer->write ){
	int cnt;
	int read=trace_buffer->read;u64 syscall_num=0;
	for(int i=0;i<8;i++){
     syscall_num=(syscall_num|((u64)(trace_buffer->data[read++])<<(i*8)));}
	//syscall_num=24;
    if(syscall_num==2||syscall_num==10||syscall_num==11||syscall_num==13||syscall_num==15||syscall_num==20||syscall_num==21||syscall_num==22||syscall_num==61){
	 cnt=trace_buffer_read(filep,buff,8);
	 buff=buff+8;bytes=bytes+8;

	}
	else if(syscall_num==1||syscall_num==6||syscall_num==7||syscall_num==12||syscall_num==14||syscall_num==19||syscall_num==27||syscall_num==29||syscall_num==36){
     cnt=trace_buffer_read(filep,buff,16);
	 buff=buff+16;bytes=bytes+16;
	 }
    else if(syscall_num==4||syscall_num==8||syscall_num==9||syscall_num==17||syscall_num==23||syscall_num==28||syscall_num==37||syscall_num==40){
     cnt=trace_buffer_read(filep,buff,24);
	 buff=buff+24;bytes=bytes+24;
	}
    else if(syscall_num==5||syscall_num==18||syscall_num==24||syscall_num==25||syscall_num==30||syscall_num==39||syscall_num==41){
     cnt=trace_buffer_read(filep,buff,32);
	 buff=buff+32;bytes=bytes+32;
	}
    else if(syscall_num==16||syscall_num==35){
     cnt=trace_buffer_read(filep,buff,40);
	 buff=buff+40;bytes=bytes+40;
	}
	 count--;
	}
	return  bytes;
}


int sys_start_strace(struct exec_context *current, int fd, int tracing_mode)
{   
	if(current->st_md_base)return -EINVAL;
	struct file*filep=(current->files)[fd];
	if(filep==NULL || filep->type!=TRACE_BUFFER)return -EINVAL;
	struct strace_head *strace_head=(struct strace_head*)os_page_alloc(USER_REG);
	if(!strace_head)return EINVAL;
	strace_head->count=0;
	strace_head->is_traced=1;
	strace_head->strace_fd=fd;
	strace_head->tracing_mode=tracing_mode;
	strace_head->next=NULL;
    strace_head->last=NULL;
	current->st_md_base=strace_head;
	if(tracing_mode!=FULL_TRACING && tracing_mode!=FILTERED_TRACING){
       return -EINVAL;    
	}
	return 0;
}

int sys_end_strace(struct exec_context *current)
{
		struct strace_head *head = current->st_md_base; 
        struct strace_info *curr = head->next;
        struct strace_info *next = curr;
        while (curr!=0) {
            next = curr->next;
            os_free(curr,sizeof(struct strace_info));
            curr=next;
        }
        os_free(current->st_md_base,sizeof(struct strace_head));
        current->st_md_base=NULL;
		return 0;
	}

///////////////////////////////////////////////////////////////////////////
//// 		Start of ftrace functionality 		      	      /////
///////////////////////////////////////////////////////////////////////////


long do_ftrace(struct exec_context *ctx, unsigned long faddr, long action, long nargs, int fd_trace_buffer)
{
    return 0;
}

//Fault handler
long handle_ftrace_fault(struct user_regs *regs)
{
        return 0;
}


int sys_read_ftrace(struct file *filep, char *buff, u64 count)
{
    return 0;
}


