/*
 * Forking and cloning threads, processes
 *
 * Copyright (C) 2008 Bahadir Balban
 */
#include <syscalls.h>
#include <vm_area.h>
#include <task.h>


/*
 * Copy all vmas from the given task and populate each with
 * links to every object that the original vma is linked to.
 * Note, that we don't copy vm objects but just the links to
 * them, because vm objects are not per-process data.
 */
int copy_vmas(struct tcb *to, struct tcb *from)
{
	struct vm_area *vma, new_vma;
	struct vm_obj_link *vmo_link, *new_link;

	list_for_each_entry(vma, from->vm_area_list, list) {

		/* Create a new vma */
		new_vma = vma_new(vma->pfn_start, vma->pfn_end - vma->pfn_start,
				  vma->flags, vma->file_offset);

		/* Get the first object on the vma */
		BUG_ON(list_empty(&vma->vm_obj_list));
		vmo_link = list_entry(vma->vm_obj_list.next,
				      struct vm_obj_link, list);
		do {
			/* Create a new link */
			new_link = vm_objlink_create();

			/* Copy object field from original link. */
			new_link->obj = vmo_link->obj;

			/* Add the new link to vma in object order */
			list_add_tail(&new_link->list, &new_vma->vm_obj_list);

		/* Continue traversing links, doing the same copying */
		} while((vmo_link = vma_next_link(&vmo_link->list,
						  &vma->vm_obj_list)));

		/* All link copying is finished, now add the new vma to task */
		list_add_tail(&vma_new->list, &to->vm_area_list);
	}

	return 0;
}

int copy_tcb(struct tcb *to, struct tcb *from)
{
	/* Copy program segment boundary information */
	to->start = from->start;
	to->end = from->end;
	to->text_start = from->text_start;
	to->text_end = from->text_end;
	to->data_start = from->data_start;
	to->data_end = from->data_end;
	to->bss_start = from->bss_start;
	to->bss_end = from->bss_end;
	to->stack_start = from->stack_start;
	to->stack_end = from->stack_end;
	to->heap_start = from->heap_start;
	to->heap_end = from->heap_end;
	to->env_start = from->env_start;
	to->env_end = from->env_end;
	to->args_start = from->args_start;
	to->args_end = from->args_end;
	to->map_start = from->map_start;
	to->map_end = from->map_end;

	/* Copy all vm areas */
	copy_vmas(to, from);

	/* Copy all file descriptors */
	memcpy(to->fd, from->fd,
	       TASK_FILES_MAX * sizeof(struct file_descriptor));
}

/*
 * Sends vfs task information about forked child, and its utcb
 */
void vfs_notify_fork(struct tcb *child, struct tcb *parent)
{
	int err;
	struct task_data_head *tdata_head;
	struct tcb *vfs;

	printf("%s/%s\n", __TASKNAME__, __FUNCTION__);

	l4_save_ipcregs();

	/* Write parent and child information */
	write_mr(L4SYS_ARG0, parent->tid);
	write_mr(L4SYS_ARG1, child->tid);
	write_mr(L4SYS_ARG2, child->utcb);

	if ((err = l4_sendrecv(VFS_TID, VFS_TID,
			       L4_IPC_TAG_NOTIFY_FORK)) < 0) {
		printf("%s: L4 IPC Error: %d.\n", __FUNCTION__, err);
		return err;
	}

	/* Check if syscall was successful */
	if ((err = l4_get_retval()) < 0) {
		printf("%s: Pager from VFS read error: %d.\n",
		       __FUNCTION__, err);
		return err;
	}
	l4_restore_ipcregs();

	return err;
}

int do_fork(struct tcb *parent)
{
	struct tcb *child;
	struct task_ids ids = {
		.tid = TASK_ID_INVALID,
		.spid = parent->spid,
	};

	/* Make all shadows in this task read-only */
	vm_freeze_shadows(parent);

	/*
	 * Create a new L4 thread with parent's page tables
	 * kernel stack and kernel-side tcb copied
	 */
	child = task_create(&ids, THREAD_CREATE_COPYSPACE);

	/* Copy parent tcb to child */
	copy_tcb(child, parent);

	/* Create new utcb for child since it can't use its parent's */
	child->utcb = utcb_vaddr_new();

	/*
	 * Create the utcb shared memory segment
	 * available for child to shmat()
	 */
	if (IS_ERR(shm = shm_new((key_t)child->utcb,
				 __pfn(DEFAULT_UTCB_SIZE)))) {
		l4_ipc_return((int)shm);
		return 0;
	}
	/* FIXME: We should munmap() parent's utcb page from child */

	/* Notify fs0 about forked process */
	vfs_notify_fork(child, parent);

	/* Start forked child. */
	l4_thread_control(THREAD_RUN, ids);

	/* Return back to parent */
	l4_ipc_return(child->tid);

	return 0;
}

