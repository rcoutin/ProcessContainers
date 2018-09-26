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
    //void (*container_scheduler_ptr)(__u64)
};

void current_container_state(){
    mutex_lock(lock);

    struct container* cur = container_list;

    while(cur!=NULL){

        printk("C%llu:",cur->container_id);

        struct thread_node* cur_thread = cur->thread_head;
        mutex_lock(container->local_lock);
        while(cur_thread!=NULL){
            printk("\tT%d",cur_thread ->pid);
            cur_thread = cur_thread ->next;
        }
        printk("\n");
        mutex_unlock(container->local_lock);
        cur = cur->next;
    }
    mutex_unlock(lock);
}

int enqueue(struct container* container, struct thread_node* thread_item){
    int ret = -1;
    if(container ->thread_tail !=NULL){
        container ->thread_tail -> next = thread_item;
        //enqueued at tail
        ret = 1;
    }
    container ->thread_tail = thread_item;
    if(container ->thread_head == NULL){
        container ->thread_head = thread_item;
         //enqueued at head
        ret = 0;
    }
    return ret;
}

struct thread_node* dequeue(struct container* container){
    struct thread_node* to_return;
    // struct thread_node* head = container ->thread_head;
    // struct thread_node* tail = container ->thread_tail;
    if(container ->thread_head == NULL){
        return NULL;
    }
    to_return = container ->thread_head;
    container ->thread_head = container ->thread_head-> next;
    if(container ->thread_head == NULL){
        container ->thread_tail = NULL;
    }
    to_return -> next = NULL; // set the next pointer upon dequeue since we don't need it
    return to_return;
}

// Lookup container by id
struct container* lookup_container(__u64 cid){
    // take a lock on the global container list
    struct container* cur;
    struct container* return_container = NULL;
    printk("Looking up container;before lock %d", current->pid);
    printk("Look up container acquired lock %d", current->pid);
    cur = container_list;
    printk("Container ID : %llu",cid);
    while (cur != NULL){
        mutex_lock(lock);
        if(cur == NULL){
            mutex_unlock(lock);
            break;
        }
        if(cur -> container_id == cid){
            printk("Container ID : %llu",cur ->container_id);
            printk("Found container, unlocked %d", current->pid);
            return_container =  cur;
            mutex_unlock(lock);
            break;
        }else{
            cur = cur -> next;
        }
        mutex_unlock(lock);
    }
    mutex_unlock(lock);
    if(return_container==NULL){
        printk("Lookup complete, didnt unlock/find container! %d", current->pid);
    }
    return return_container;
}
// Thread Linked Lists
int add_thread(struct container* container){
    int pos;
    struct thread_node* current_thread_node = (struct thread_node*) kcalloc(1, sizeof(struct thread_node), GFP_KERNEL);
    printk("Adding a thread to the container %d", current->pid);
    // allocating memory and assigning current threads task_struct
    current_thread_node->thread_id = current->pid;
    current_thread_node->context = current;
    //take a lock using the containers local mutex
    mutex_lock(container->local_lock);
            pos = enqueue(container,current_thread_node);
    mutex_unlock(container->local_lock);
    if(pos == 0){
        //added to head
        set_current_state(TASK_RUNNING);
    }else if(pos == 1){
        //added to tail, so sleep
        set_current_state(TASK_INTERRUPTIBLE);
    }
    //printk("Added thread to the container %d", container->thread_tail->thread_id);
    printk("Added thread to the container ");
    schedule();
    return 0;
}


// Container Linked Lists
int add_container(struct container* lookup_cont, __u64 cid){
    
    if(lookup_cont!= NULL){
        //container exists; add threads    
        add_thread(lookup_cont);    
        printk("Added new thread %d", current->pid);
    }else{
        struct container* new_container = (struct container*) kcalloc(1, sizeof(struct container), GFP_KERNEL);   

        struct mutex* container_lock = (struct mutex*) kcalloc(1,sizeof(struct mutex),GFP_KERNEL);
        mutex_init(container_lock);
        new_container->local_lock = container_lock;
        new_container->container_id = cid;
        new_container-> next = NULL;

        printk("Adding container. Before lock %d", current->pid);
        //lock the global container list before adding

        mutex_lock(lock);
                if(container_list!=NULL){
                    container_list->prev = new_container;
                }
                new_container->next = container_list;
                container_list = new_container;
        mutex_unlock(lock);
        printk("Adding container; acquired lock %d", current->pid);

        printk("Adding a fresh container %d", current->pid);
        add_thread(new_container);
        printk("Added container, unlocked %d", current->pid);
     }     
    return 0;
}

void delete_container(struct container* container){
    mutex_lock(lock);
    if(container!=NULL){
        printk("Try to delete container %llu by %d",container->container_id, current->pid);
        if(container -> next !=NULL){
                container->next->prev = container->prev;
        }
        if(container->prev != NULL){
            container->prev->next = container->next;
           
        }else{
            container_list = container->next;
        }
        printk("Freed container %llu by %d",container->container_id, current->pid);

        //mutex_destroy(container->local_lock);
        //kfree(container->local_lock);
        //container->next = NULL;
        //container-> prev = NULL;
        //kfree(container);
        //container=NULL;
        // if(container_list == NULL){
        //     printk("No more containers!");
        // }else{
        //     printk("Thread Head ID: %d", container_list->thread_head->thread_id);
        // }
        // printk("Freed container %llu by %d",container->container_id, current->pid);
    }
    mutex_unlock(lock);
   
}

/**
 * Delete the task in the container.
 * 
 * external functions needed:
 * mutex_lock(), mutex_unlock(), wake_up_process(), 
 */

int delete_thread(struct container* container){
    struct thread_node* to_delete;
    struct thread_node* new_head;
    int ret_code = 1;
    mutex_lock(container->local_lock);
    if(container !=NULL){
                to_delete = dequeue(container);
                if(to_delete!=NULL){
                    //free memory of this thread_node
                    kfree(to_delete);
                    to_delete=NULL;
                    //get the next thread to schedule, if it exists
                    new_head = dequeue(container);
                    if(new_head == NULL){
                        delete_container(container);
                        //no more threads in this container, delete it
                    }else{
                        wake_up_process(new_head->context);
                    }
                }else{
                    ret_code = -1;
                    //somehow the current thread wasn't the head of the queue
                }
    }else{
        ret_code = -1;
        //somehow the container was null;
    }
    mutex_unlock(container->local_lock);
    return ret_code;
}

int processor_container_delete(struct processor_container_cmd __user *user_cmd)
{

    struct processor_container_cmd kprocessor_container_cmd;
    struct container* cont;
    unsigned long ret; 
    //int delete_ret;
    ret = copy_from_user(&kprocessor_container_cmd, user_cmd, sizeof(struct processor_container_cmd));
    if(ret!=0){
        printk("copy_from_user failed. Couldn't delete. %d", current->pid);
        return -1;
    }
    // Lookup container
    printk("In delete lookup");
    cont = lookup_container(kprocessor_container_cmd.cid);
    if(cont!=NULL){
        delete_thread(cont);
    }

    mutex_lock(lock);
        if(container_list==NULL){
            mutex_destroy(lock);
            kfree(lock);
        }
    mutex_unlock(lock);
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
  
    //copying CID from user space to kernel space
    ret = copy_from_user(&kprocessor_container_cmd, user_cmd, sizeof(struct processor_container_cmd));
    if(ret==0){
        printk("Thread ID: %d CID: %llu", task->pid, kprocessor_container_cmd.cid);

        lookup_cont = lookup_container(kprocessor_container_cmd.cid);
        add_container(lookup_cont, kprocessor_container_cmd.cid);
        
    }else{
        printk("Did not work");
    }
    
    return 0;
}

struct container* find_container_by_thread(pid_t pid){
    struct container* cur = container_list;
    struct container* ret = NULL;

    while(cur!=NULL){
        mutex_lock(lock);
        if(cur == NULL){
            mutex_unlock(lock);
            break;
        }
        // take a local lock to prevent
        //mutex_lock(cur-> local_lock);

        if(cur->thread_head != NULL && cur->thread_head->thread_id == pid){
            ret = cur;
            //mutex_unlock(cur-> local_lock);
            mutex_unlock(lock);
            break;
        }else{
            cur = cur -> next;
        }
        //mutex_unlock(cur-> local_lock);

        mutex_unlock(lock);
    }
    return ret;
}

/**
 * switch to the next task within the same container
 * 
 * external functions needed:
 * mutex_lock(), mutex_unlock(), wake_up_process(), set_current_state(), schedule()
 */
int processor_container_switch(struct processor_container_cmd __user *user_cmd)
{
    current_container_state();
    struct container* context_switch_container;
    struct thread_node* top;
    int woken_up;
    printk("Context switching for thread %d",current->pid);
    // Find container having current thread
    mutex_lock(lock);
    context_switch_container = find_container_by_thread(current->pid);
    mutex_unlock(lock);
    // Suspend current thread and Move thread to the back of the queue and change thread head
    if(context_switch_container != NULL){
        mutex_lock(context_switch_container->local_lock);
        printk("This thread %d belongs to container %llu",current->pid, context_switch_container->container_id);

        if(context_switch_container->thread_head != context_switch_container->thread_tail){
                
            top = dequeue(context_switch_container);
            enqueue(context_switch_container,top);
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