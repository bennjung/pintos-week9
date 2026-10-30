/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "userprog/syscall.h"
#include "userprog/process.h"
#include "threads/mmu.h"

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void
vm_file_init (void) {

}

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;
	return true;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page = &page->file;

	if (page->frame != NULL) {
		if (pml4_is_dirty(thread_current()->pml4, page->va)) {
			lock_acquire(&file_lock);
			file_write_at(page->file.file, page->frame->kva, page->file.length, page->file.ofs);
			lock_release(&file_lock);
		}
		pml4_clear_page(thread_current()->pml4, page->va);
		palloc_free_page(page->frame->kva);
		free(page->frame);
		page->frame = NULL;
	}
}

static bool
lazy_load_mmap (struct page *page, void *aux) {
	
	struct file_load *aux_mmap = (struct file_load *)aux; // 되는지 모름
	size_t pg_r_b = aux_mmap->page_read_bytes;

	page->file.file = aux_mmap->file;
	page->file.ofs = aux_mmap->ofs;
	page->file.length = aux_mmap->page_read_bytes;

	lock_acquire(&file_lock);
	off_t read_bytes =  file_read_at(aux_mmap->file, page->frame->kva, aux_mmap->page_read_bytes, aux_mmap->ofs);
	if (read_bytes == aux_mmap->page_read_bytes) {	// 똑디 읽었는지 검사
		uint8_t *address = page->frame->kva;
		memset(address + read_bytes, 0, aux_mmap->page_zero_bytes);	// 0채우기
	}
	lock_release(&file_lock);

	free(aux_mmap);	// 자유
	if (read_bytes == pg_r_b) {
		return true;
	}
	else {
		return false;
	}
}

static struct mmap_info *
m_info_func (void *addr, size_t length, struct file *file) {

	struct mmap_info *m_info = malloc(sizeof(struct mmap_info));
	if (m_info == NULL) {
		return NULL;
	}

	m_info->start = addr;
	m_info->page_count = (length + PGSIZE - 1) / PGSIZE;

	lock_acquire(&file_lock);
	m_info->file = file_reopen(file);
	lock_release(&file_lock);

	if (m_info->file == NULL) {
		file_close(m_info->file);
		free(m_info);
		return NULL;
	}
	
	return m_info;
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable, struct file *file, off_t offset) {

	if (length <= 0) {
		return NULL;
	}

	struct mmap_info *m_info = m_info_func (addr, length, file);
	if (m_info == NULL) {
		return NULL;
	}

	size_t remaining = length;
	uint8_t *page_addr = addr;
	off_t page_ofs = offset;
	
	while (remaining > 0) {
		struct file_load *mmap_aux = malloc(sizeof(struct file_load));
		if (mmap_aux == NULL) {
			file_close(m_info->file);
			free(m_info);
			free(mmap_aux);
			return NULL;
		}
		size_t page_bytes = remaining < PGSIZE ? remaining : PGSIZE;
		size_t page_read_bytes;
		size_t page_zero_bytes;
		lock_acquire(&file_lock);
		size_t file_size = file_length(m_info->file);
		lock_release(&file_lock);
		if (page_ofs > file_size) {
			page_read_bytes = 0;
			page_zero_bytes = PGSIZE - page_read_bytes;
		}
		else {
			page_read_bytes = (file_size - page_ofs) < page_bytes ? (file_size - page_ofs) : page_bytes;
			page_zero_bytes = PGSIZE - page_read_bytes;
		}
		mmap_aux->page_zero_bytes = page_zero_bytes;
		mmap_aux->page_read_bytes = page_read_bytes;
		mmap_aux->file = m_info->file;
		mmap_aux->ofs = page_ofs;

		if (!vm_alloc_page_with_initializer(VM_FILE, page_addr, writable, lazy_load_mmap, mmap_aux)) {
			free(m_info);
			return NULL;
		}

		page_addr += PGSIZE;
		page_ofs += PGSIZE;
		remaining -= page_bytes;
	}
	list_push_back(&thread_current()->mmap_list, &m_info->mmap_elem);
	return addr;
}

/* Do the munmap */
void
do_munmap (void *addr) {
	struct thread *curr = thread_current();
	struct list_elem *ptr;
	struct list *list = &curr->mmap_list;

	for(ptr = list_begin(list); ptr != list_end(list); ptr = list_next(ptr)){
		struct mmap_info *m_info = list_entry(ptr, struct mmap_info, mmap_elem);
		uint8_t *page_addr = m_info->start;
		if (m_info->start == addr) {
			for (size_t i = 0; i < m_info->page_count; i++) {
				struct page *p = spt_find_page(&thread_current()->spt, page_addr);

				if (p != NULL) {
					spt_remove_page(&thread_current()->spt, p);
				}

				page_addr += PGSIZE;
			}

			lock_acquire(&file_lock);
			file_close(m_info->file);
			lock_release(&file_lock);

			list_remove(&m_info->mmap_elem);
			free(m_info);
			return;
		}
	}

}
