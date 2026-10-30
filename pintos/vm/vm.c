/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "kernel/hash.h"
#include "threads/vaddr.h"
#include "threads/mmu.h"
#include "userprog/process.h"
#include "lib/string.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "userprog/syscall.h"

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* 초기화 함수를 사용하여 보류 중인(pending) 페이지 객체를 생성합니다. 
* 페이지를 생성해야 할 경우, 직접 생성하지 말고 이 함수나
* `vm_alloc_page`를 통해 생성하십시오. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT);

	struct supplemental_page_table *spt = &thread_current ()->spt;
	
	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: 페이지를 생성하고, VM 유형에 따라 초기화자(initializer)를 가져온 뒤,
		* TODO: uninit_new를 호출하여 'uninit' 페이지 구조체를 생성하십시오. 
		* TODO: uninit_new를 호출한 후에 해당 필드를 수정해야 합니다. */
		struct page *p = malloc(sizeof(struct page));
		if (p == NULL) {
			return false;
		}

		if (VM_TYPE(type) == VM_ANON) {
			uninit_new(p, upage, init, type, aux, anon_initializer);
		}
		else if (VM_TYPE(type) == VM_FILE) {
			uninit_new(p, upage, init, type, aux, file_backed_initializer);
		}
		else {
			free(p);
			return false;
		}

		p->writable = writable;
		/* TODO: 페이지를 spt(보조 페이지 테이블)에 삽입하십시오. */
		if (spt_insert_page(spt, p) == false) {
			free(p);
			return false;
		}
		else {
			return true;
		}
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt, void *va) {
    struct page *page = NULL;
    /* TODO: Fill this function. */
    /* TODO: 이 함수를 구현하세요. */
    struct page key = { .va = pg_round_down(va) };

    struct hash_elem *find_hash_elem = hash_find(&spt->hash_table, &key.hash_elem);
    if(find_hash_elem == NULL)
    {
        return NULL;
    }
    page = hash_entry(find_hash_elem, struct page, hash_elem);

    return page;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt, struct page *page) {
	if (hash_insert(&spt->hash_table, &page->hash_elem) == NULL) {
		return true;
	}
	return false;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	hash_delete(&spt->hash_table, &page->hash_elem);
	vm_dealloc_page (page);
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

/* palloc()을 통해 프레임을 할당받는다. 사용 가능한 페이지가 없다면, 기존
 * 페이지를 내보내고(evict) 해당 페이지를 반환한다. 이 함수는 항상 유효한 주
 * 소를 반환한다. 즉, 사용자 풀(user pool) 메모리가 가득 찼을 경우, 이 함수는
 * 프레임을 내보내어 사용 가능한 메모리 공간을 확보한다. */
struct frame *
vm_get_frame (void) {
	struct frame *frame = NULL;
	/* TODO: Fill this function. */
	frame = malloc(sizeof(struct frame));
	if (frame == NULL) {
		PANIC ("could'n allocate frame");
	}
	frame->kva = palloc_get_page(PAL_USER);
	frame->page = NULL;

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* Growing the stack. */
void
vm_stack_growth (void *addr) {
	if (!vm_alloc_page(VM_ANON, pg_round_down(addr), true)) {
		process_exit();
	}
	vm_claim_page(addr);
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f, void *addr,
		bool user, bool write, bool not_present) {
	struct supplemental_page_table *spt = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */

	uintptr_t cmp_stack = USER_STACK;
	uintptr_t cmp_addr = addr;
	struct page *f_page = spt_find_page(spt, addr);
	if (not_present == true && is_user_vaddr(addr) && addr >= (f->rsp - 8)) {
		if(cmp_addr >= (cmp_stack-(1 << 20)) && addr < USER_STACK && spt_find_page(spt, addr) == NULL){
			vm_stack_growth(addr);
			return true;
		}
	}

	page = f_page;
	if (page == NULL) {
		return false;
	}
	return vm_do_claim_page (page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va) {
	struct page *page = NULL;
	/* TODO: Fill this function */
	struct thread *t = thread_current();
	page = spt_find_page(&t->spt, va);
	if (page == NULL) {
		return false;
	}
	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	struct thread *t = thread_current();
	/* TODO: 페이지의 가상 주소(VA)를 프레임의 물리 주소(PA)에 매핑하기 위해 페이지 테이블 항목을 삽입한다. */
	pml4_set_page(t->pml4, page->va, frame->kva, true);
	return swap_in (page, frame->kva);
}

unsigned
init_hash_func (const struct hash_elem *he, void *aux UNUSED) {
	struct page *p = hash_entry(he, struct page, hash_elem);
	return hash_bytes(&p->va, sizeof(p->va));
}

bool
less_hash_func (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
	struct page *p1 = hash_entry(a, struct page, hash_elem);
	struct page *p2 = hash_entry(b, struct page, hash_elem);
	if (p1->va < p2->va) {
		return true;
	}
	else {
		return false;
	}
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt) {
	hash_init(&spt->hash_table, init_hash_func, less_hash_func, NULL);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst, struct supplemental_page_table *src) {
	struct page *p = NULL;
	struct hash_iterator iter;
	hash_first(&iter, &src->hash_table);
	
	while(hash_next(&iter)) {
		struct file_load *child_aux = NULL;
		p = hash_entry(hash_cur(&iter), struct page, hash_elem);
		if (p->operations->type == VM_UNINIT) {
			if (p->uninit.aux != NULL) {
				child_aux = malloc(sizeof(struct file_load));
				if (child_aux == NULL) {
					return false;
				}
				memcpy(child_aux, p->uninit.aux, sizeof *child_aux);

				lock_acquire(&file_lock);
				child_aux->file = file_duplicate(child_aux->file);
				lock_release(&file_lock);
				if (child_aux->file == NULL) {
					free(child_aux);
					return false;
				}
			}
			if (!vm_alloc_page_with_initializer(p->uninit.type, p->va, p->writable, p->uninit.init, (void *)child_aux)){
				if (child_aux != NULL) {
					lock_acquire(&file_lock);
					file_close(child_aux->file);
					lock_release(&file_lock);
					free(child_aux);
				}
				return false;
			}
		}
		else if (p->operations->type == VM_ANON) {
			if (p->frame == NULL) {
				return false;
			}
			if (!vm_alloc_page(VM_ANON, p->va, p->writable)) {
				return false;
			}
			if (!vm_claim_page(p->va)) {
				return false;
			}

			struct page *child_page = spt_find_page(dst, p->va);
			if (child_page == NULL || child_page->frame == NULL) {
				return false;
			}
			memcpy(child_page->frame->kva, p->frame->kva, PGSIZE);
		}
		else if (p->operations->type == VM_FILE) {

		}
	}
}

void
action_hash_func(struct hash_elem *e, void *aux UNUSED) {
	struct page *p = hash_entry(e, struct page, hash_elem);
	vm_dealloc_page(p);
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt) {
	/* TODO: 스레드가 보유한 모든 supplemental_page_table을 파괴하고,
	 * TODO: 수정된 모든 내용을 저장소에 다시 기록(writeback)한다. */
	//hash_destroy(&spt->hash_table, action_hash_func);
}
