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
struct mutex* lock = NULL;
// Structures for containers and threads
//Threads
struct thread_node {
    int thread_id;
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
    struct mutex * local_lock;
    // kthread_t * sched_thread;
    // Function pointer - Assign it when you create container.
    //void (*container_scheduler_ptr)(__u64);
};

// Function pointer
void container_scheduler(__u64 container_id){
    // Call this using kthread in process_container_create
}

// Lookup container by id
struct container* lookup_container(__u64 cid){
    // take a lock on the global container list
    struct container* cur;
    printk("Looking up container;before lock %d", current->pid);
    //mutex_lock(lock);
    printk("Look up container acquired lock %d", current->pid);
    cur = container_list;

    while (cur != NULL){
        printk("Container ID : %llu",cur ->container_id);
        if(cur -> container_id == cid){
            //mutex_unlock(lock);
            printk("Found container, unlocked %d", current->pid);
            return cur;
        }else{
            cur = cur -> next;
        }
    }
    
    printk("Lookup complete, didnt unlock/find container! %d", current->pid);
    return NULL;
}
// Thread Linked Lists
int add_thread(struct container* container){
    struct thread_node* current_thread_node = (struct thread_node*) kcalloc(1, sizeof(struct thread_node), GFP_KERNEL);
    printk("KKK Adding a thread to the container %d", current->pid);

    // allocating memory and assigning current threads task_struct
    current_thread_node->thread_id = current->pid;
    current_thread_node->context = current;
    // current_thread_node->context = (struct task_struct*) kcalloc(1, sizeof(struct task_struct), GFP_KERNEL);
    // memcpy(current_thread_node->context, current, sizeof(struct task_struct));

    //take a lock using the containers local mutex
    mutex_lock(container->local_lock);

    //add thread node to the back of the queue
    if(container->thread_tail != NULL){
        container->thread_tail->next = current_thread_node;
    }
    container->thread_tail = current_thread_node;

    //check the front to see if this is the first thread into the container or not
    if(container-> thread_head == NULL){
        container -> thread_head = current_thread_node;
        set_current_state(TASK_RUNNING);
    }else{
        // not the first thread, so sleep
        set_current_state(TASK_INTERRUPTIBLE);
    }

    printk(" KKK Added thread to the container %d", container->thread_tail->thread_id);
    mutex_unlock(container->local_lock);
    schedule();
    return 0;
}

// Container Linked Lists
int add_container(struct container* lookup_cont, __u64 cid){
    
    //lock the global container list before adding
    printk("Adding container. Before lock %d", current->pid);
    
    printk("Adding container; acquired lock %d", current->pid);

    if(lookup_cont!= NULL){
        //container exists; add threads    
        add_thread(lookup_cont);    
        printk("Added new thread %d", current->pid);
    }else{
        struct container* new_head = (struct container*) kcalloc(1, sizeof(struct container), GFP_KERNEL);   

        struct mutex* container_lock = (struct mutex*) kcalloc(1,sizeof(struct mutex),GFP_KERNEL);
        mutex_init(container_lock);
        new_head->local_lock = container_lock;
        new_head->container_id = cid;
        new_head->next = container_list;

        container_list = new_head;

        printk("KKK Adding a fresh container %d", current->pid);
        add_thread(container_list);
        printk("Added container, unlocked %d", current->pid);
     }

    //  mutex_unlock(lock);
     
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
            printk("Stored Thread %d.",cur -> thread_id);
            if(cur -> thread_id  == current -> pid){
                printk("In delete thread. Thread found! %d", current->pid);
                if(prev == NULL){
                    cont->thread_head = cur->next;
                }else{
                    prev->next = cur -> next;
                }
                kfree(cur);
                cur=NULL;
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
    struct container* cont;
    unsigned long ret;
    int delete_ret;
    ret = copy_from_user(&kprocessor_container_cmd, user_cmd, sizeof(struct processor_container_cmd));
    if(ret!=0){
        printk("copy_from_user failed. Couldn't delete. %d", current->pid);
        return -1;
    }

    // Lookup container
    printk("In delete lookup");
    cont = lookup_container(kprocessor_container_cmd.cid);
  
    // remove the container
    if(cont!=NULL){
        // Find the thread and delete it
        mutex_lock(cont->local_lock);
        delete_ret = delete_thread(cont);
        mutex_unlock(cont->local_lock);
        if(delete_ret != 0){
            printk("could not find the current thread to delete (?!!) %d", current->pid);
            return -1;
        }

        //delete the container
        mutex_lock(lock);
        if(cont->thread_head==NULL){
            if(cont->prev != NULL){
                cont->prev->next = cont->next;
                cont->next->prev = cont->prev;
            }else{
                container_list = cont->next;
            }
            if(container_list == NULL){
                printk("No more containers!");
            }else{
                printk("Thread Head ID: %d", container_list->thread_head->thread_id);
            }
            kfree(cont);
            cont=NULL;
            printk("Freed container %d", current->pid);
        }
        mutex_unlock(lock);
    }

    //delete the lock
    // if(container_list==NULL){
    //     kfree(lock);
    //     lock=NULL;
    // }
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
    struct container* lookup_cont;
    unsigned long ret;

    if(lock == NULL){
        lock = (struct mutex *) kcalloc(1, sizeof(struct mutex),GFP_KERNEL);
        mutex_init(lock);
        printk("Initialized lock %d", current->pid);
    }
  
    // //1. copying CID from user space to kernel space
    
    mutex_lock(lock);
    ret = copy_from_user(&kprocessor_container_cmd, user_cmd, sizeof(struct processor_container_cmd));
    if(ret==0){
        printk("Thread ID: %d CID: %llu", task->pid, kprocessor_container_cmd.cid);

        //lock here
        printk("Create: Obtaining lock");
        // lookup container
        lookup_cont = lookup_container(kprocessor_container_cmd.cid);
        // add
        add_container(lookup_cont, kprocessor_container_cmd.cid);
        //
    }else{
        printk("Did not work");
    }
    mutex_unlock(lock);
    return 0;
}

struct container* find_container(pid_t pid){
    struct container* cur = container_list;
    while(cur!=NULL){
        if(cur->thread_head != NULL && cur->thread_head->thread_id == pid){
            return cur;
        }else{
            cur = cur -> next;
        }
    }
    return NULL;
}

/**
 * switch to the next task within the same container
 * 
 * external functions needed:
 * mutex_lock(), mutex_unlock(), wake_up_process(), set_current_state(), schedule()
 */
int processor_container_switch(struct processor_container_cmd __user *user_cmd)
{
    struct container* context_switch_container;
    struct thread_node* top;
    int woken_up;
    printk("Context switching for thread %d",current->pid);
    // Find container having current thread
    mutex_lock(lock);
    context_switch_container = find_container(current->pid);
    mutex_unlock(lock);
    // Suspend current thread and Move thread to the back of the queue and change thread head
    if(context_switch_container != NULL){
        mutex_lock(context_switch_container->local_lock);
        printk("This thread %d belongs to container %llu",current->pid, context_switch_container->container_id);
        if(context_switch_container->thread_head != context_switch_container->thread_tail){
            top = context_switch_container->thread_head;
            context_switch_container->thread_head = top->next;
            context_switch_container->thread_tail->next=top;
            top->next = NULL;
            // Activate new thread head
            // memcpy(context_switch_container->thread_tail->context, current, sizeof(struct task_struct));
            set_current_state(TASK_INTERRUPTIBLE);
            woken_up = wake_up_process(context_switch_container->thread_head->context);
            if(woken_up == 1){
                printk("This thread %d has been woken up",current->pid);
            }else{
                printk("This thread %d was already running",current->pid);
            }
        }else{
            set_current_state(TASK_RUNNING);
        }
        mutex_unlock(context_switch_container->local_lock);
        schedule();
        printk("This thread %d has been put to sleep",current->pid);
    }
    else{
        printk("Could not find container for context switching for thread %d",current->pid);
    }

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


void enqueue(struct thread_node * head, struct thread_node * tail , struct thread_node* thread_item){
    if(tail !=NULL){
        tail -> next = thread_item;
    }
    tail = thread_item;
    if(head == NULL){
        head = tail;
    }
}

struct thread_node* dequeue(struct thread_node* head){
    if(head == NULL){
        return NULL;
    }
    struct thread_node* to_return = head;
    head = head-> next;
    if(head == NULL){
        tail = NULL;
    }
    return to_return;
}

