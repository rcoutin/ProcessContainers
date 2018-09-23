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
// Global lock
struct mutex* lock = NULL;
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
    // Call this using kthread in process_container_create
}

// Lookup container by id
struct container* lookup_container(__u64 cid){
    // take a lock on the global container list
    struct container* cur;
    printk("Looking up container;before lock");
    mutex_lock(lock);
    printk("Look up container acquired lock");
    cur = container_list;

    while (cur != NULL){
        if(cur -> container_id == cid){
            return cur;
        }else{
            cur = cur -> next;
        }
    }
    mutex_unlock(lock);
    printk("Lookup complete, unlocked");
    return NULL;
}
// Thread Linked Lists
int add_thread(struct container* container){
    struct thread_node* temp = (struct thread_node*) kcalloc(1, sizeof(struct thread_node), GFP_KERNEL);
    printk("KKK Adding a thread to the container");
    // allocating memory and assigning current threads task_struct
    temp->context = (struct task_struct*) kcalloc(1, sizeof(struct task_struct), GFP_KERNEL);
    memcpy(temp->context, current, sizeof(struct task_struct));

    if(container->thread_tail != NULL){
        container->thread_tail->next = temp;
    }
    container->thread_tail = temp;    
    if(container-> thread_head == NULL){
        container -> thread_head = container -> thread_tail;
    }
    return 0;
}

// Container Linked Lists
int add_container(__u64 cid){
    struct container* lookup_cont;
    //lock the global container list before adding
    printk("Adding container. Before lock");
    mutex_lock(lock);
    printk("Adding container; acquired lock");

    lookup_cont = lookup_container(cid);
    if(lookup_cont!= NULL){
        //container exists; add threads    
        add_thread(lookup_cont);    
        printk("Need to add new threads");
    }else{
        struct container* new_head = (struct container*) kcalloc(1, sizeof(struct container), GFP_KERNEL);        
        new_head->next = container_list;
        container_list = new_head;
        printk("KKK Adding a fresh container");
        add_thread(container_list);
     }

     mutex_unlock(lock);
     printk("Added container, unlocked");
    return 0;
}

/**
 * Delete the task in the container.
 * 
 * external functions needed:
 * mutex_lock(), mutex_unlock(), wake_up_process(), 
 */


//assuming the current thread is at the head
// int lookup_thread(struct container* cont){

//     if(cont !=NULL){
//         struct thread_node* cur = cont -> thread_head;
//         while (cur != NULL){
//             if(cur -> context -> pid  == current -> pid){
//                 return cur;
//             }else{
//                 cur = cur -> next;
//             }
//     }

//     return NULL;
// }

int delete_thread(struct container* cont){

    if(cont !=NULL){
        struct thread_node* cur = cont -> thread_head;
        struct thread_node* prev = NULL;

        while (cur != NULL){
            printk("%d",cur ->context ->pid);
            printk("%d",current -> pid);
            if(cur -> context -> pid  == current -> pid){
                printk("In delete thread. Thread found!");
                if(prev == NULL){
                    cont->thread_head = cur->next;
                }else{
                    prev->next = cur -> next;
                }
                kfree(cur);
                return 0;
            }else{
                prev = cur;
                cur = cur -> next;
            }
        }  
    }
    return -1;
}

int processor_container_delete(struct processor_container_cmd __user *user_cmd)
{
    struct processor_container_cmd kprocessor_container_cmd;
    unsigned long ret = copy_from_user(&kprocessor_container_cmd, user_cmd, sizeof(struct processor_container_cmd));
    struct container* cont;
    if(ret!=0){
        printk("copy_from_user failed. Couldn't delete.");
        return -1;
    }
    // Lookup container
    cont = lookup_container(kprocessor_container_cmd.cid);
    
    // Find the thread and delete it
    if(delete_thread(cont) != 0){
        printk("could not find the current thread to delete (?!!)");
        return -1;
    }

    if(cont->thread_head==NULL){
        if(cont->prev != NULL){
            cont->prev->next = cont->next;
            cont->next->prev = cont->prev;
        }else{
            container_list = cont->next;
        }
        kfree(cont);
        printk("Freed container");
    }

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
    struct processor_container_cmd kprocessor_container_cmd;
    unsigned long ret;

    if(lock == NULL){
        lock = (struct mutex *) kcalloc(1, sizeof(struct mutex),GFP_KERNEL);
        mutex_init(lock);
        printk("Initialized lock");
    }
  
    // //1. copying CID from user space to kernel space
    ret = copy_from_user(&kprocessor_container_cmd, user_cmd, sizeof(struct processor_container_cmd));
    if(ret==0){
        printk("Rahul1 TID: %d CID: %llu", task->pid, kprocessor_container_cmd.cid);
        add_container(kprocessor_container_cmd.cid);
    }else{
        printk("Did not work");
    }
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
    //
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
