//////////////////////////////////////////////////////////////////////
//                      North Carolina State University
//
//
//
//                             Copyright 2016
//
////////////////////////////////////////////////////////////////////////
//
// This program is free software; you can redistribute it and/or modify it
// under the terms and conditions of the GNU General Public License,
// version 2, as published by the Free Software Foundation.
//
// This program is distributed in the hope it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
// more details.
//
// You should have received a copy of the GNU General Public License along with
// this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
//
////////////////////////////////////////////////////////////////////////
//
//   Author:  Hung-Wei Tseng, Yu-Chia Liu
//
//   Description:
//     Core of Kernel Module for Processor Container
//
////////////////////////////////////////////////////////////////////////

#include "processor_container.h"

#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/poll.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/kthread.h>

// Global container list
struct container* container_list = NULL;

// Structures for containers and threads
//Threads
struct thread_node {
    struct thread_node* next;
    struct task_struct *context;
};
// Containers
struct container{
    __u64 container_id;
    struct container* next;
    struct container* prev;
    struct thread_node* thread_head;
    struct thread_node* thread_tail;

    // Function pointer - Assign it when you create container.
    void (*container_scheduler_ptr)(__u64);
};

// Function pointer
void container_scheduler(__u64 container_id){
    // Call this using pthread_create in process_container_create
}

// Lookup

// Thread Linked Lists
int add_thread(struct container* list){
    struct thread_node* temp = (struct thread_node*) kcalloc(1, sizeof(struct thread_node), GFP_KERNEL);

    // allocating memory and assigning current threads task_struct
    temp->context = (struct task_struct*) kcalloc(1, sizeof(struct task_struct), GFP_KERNEL);
    memcpy(&(temp->context), current, sizeof(struct task_struct));

    if(list->thread_tail != NULL){a
        list->thread_tail->next = temp;
    }
    list->thread_tail = temp;    
    return 0;
}

// Container Linked Lists
int add_container(__u64 cid){
    if(1!=1){
        
    }else{
        struct container* temp = (struct container*) kcalloc(1, sizeof(struct container), GFP_KERNEL);        
        temp->next = container_list;
        container_list = temp;
        add_thread(container_list);
    }
    return 0;
}

/**
 * Delete the task in the container.
 * 
 * external functions needed:
 * mutex_lock(), mutex_unlock(), wake_up_process(), 
 */
int processor_container_delete(struct processor_container_cmd __user *user_cmd)
{
    return 0;
}

/**
 * Create a task in the corresponding container.
 * external functions needed:
 * copy_from_user(), mutex_lock(), mutex_unlock(), set_current_state(), schedule()
 * 
 * external variables needed:
 * struct task_struct* current  
 */
int processor_container_create(struct processor_container_cmd __user *user_cmd)
{
    struct task_struct* task = current;
    // struct container* assigned_container;
    // struct thread_node* current_thread = (struct thread_node*) kcalloc(1,sizeof(struct thread_node),GFP_KERNEL);
    // struct container* assigned_container = ( struct container* ) kcalloc(1,sizeof(struct container),GFP_KERNEL);
    
    // //1. copying CID from user space to kernel space
    struct processor_container_cmd kprocessor_container_cmd;
    // __u64 buf;
    unsigned long ret = copy_from_user(&kprocessor_container_cmd, user_cmd, sizeof(struct processor_container_cmd));
    if(ret==0){
        printk("Rahul1 TID: %d CID: %llu", task->pid, kprocessor_container_cmd.cid);
        add_container(kprocessor_container_cmd.cid);
    }else{
        printk("Did not work");
    }
    // // 2. associate the thread to a container with ID=cid using current
        
    // // thread struct

    // current_thread-> task_id = task ->pid;
    // current_thread-> context = current;

    // //container struct

    // assigned_container -> container_id = buf;
    // assigned_container -> thread_head = current_thread; //gotta fix



    // // link them
    // append_to_container_list(buf);
    return 0;
}

/**
 * switch to the next task within the same container
 * 
 * external functions needed:
 * mutex_lock(), mutex_unlock(), wake_up_process(), set_current_state(), schedule()
 */
int processor_container_switch(struct processor_container_cmd __user *user_cmd)
{
    return 0;
}

/**
 * control function that receive the command in user space and pass arguments to
 * corresponding functions.
 */
int processor_container_ioctl(struct file *filp, unsigned int cmd,
                              unsigned long arg)
{
    switch (cmd)
    {
    case PCONTAINER_IOCTL_CSWITCH:
        return processor_container_switch((void __user *)arg);
    case PCONTAINER_IOCTL_CREATE:
        return processor_container_create((void __user *)arg);
    case PCONTAINER_IOCTL_DELETE:
        return processor_container_delete((void __user *)arg);
    default:
        return -ENOTTY;
    }
}
