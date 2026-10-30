/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "filesys/file.h"
#include "lib/string.h"

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
hash_action_func * page_destroy(struct hash_elem *e, void *aux UNUSED);
unsigned page_hash(const struct hash_elem *p_, void *aux);
bool page_less(const struct hash_elem *a_, const struct hash_elem *b_, void *aux);
bool page_uninit_copy(struct page *src, struct page *dst);
bool page_frame_copy(struct page *src, struct page *dst);
bool is_stack_valid(uintptr_t stp , uint64_t addr);


/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		struct page *page = NULL;
		page = malloc(sizeof *page);
		if (page == NULL){
			return false;
		}
		switch (VM_TYPE(type))
		{
		case VM_ANON:
			uninit_new(page, upage, init, type, aux, anon_initializer);
			break;
		
		case VM_FILE:
			uninit_new(page, upage, init, type, aux, file_backed_initializer);
			break;

		default:
			free(page);
			return false;
			break;
		}
		page->writable = writable;
		/* TODO: Insert the page into the spt. */
		if (!spt_insert_page(spt, page)){
			return false;
		};
	}
	return true;
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	struct page *page = NULL;
	struct hash_elem *hash_temp = NULL;
	struct page key = {.va = pg_round_down(va)};
	
	/* TODO: Fill this function. */
	hash_temp = hash_find(&spt->hash_table, &key.hash_elem);
	// empty check 
	if (hash_temp == NULL){
		return NULL;
	}
	page = hash_entry(hash_temp, struct page, hash_elem);

	return page;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt,
		struct page *page) {
	bool succ = false;
	/* TODO: Fill this function. */
	struct hash_elem *hash_temp = hash_insert(&spt->hash_table, &page->hash_elem);

	// 1. insert page.. 
	if (hash_temp == NULL){
		succ = true;
	}

	return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page (page);
	return true;
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

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	struct frame *frame = NULL;
	/* TODO: Fill this function. */
	frame = malloc(sizeof(*frame));
	frame->kva = palloc_get_page(PAL_USER);
	if (frame->kva == NULL){
		free(frame);
		PANIC("todo");
	}
	frame->page = NULL;

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
	// alloc anon page
	vm_alloc_page(VM_ANON |  VM_MARKER_0, pg_round_down(addr), true);
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	// valid fault address 
	if (is_kernel_vaddr(addr) || (int) addr >= USER_STACK){
		return false;
	}
	page = spt_find_page(spt, addr);
	if (page == NULL){
		uintptr_t stp ;
		// stack validity 
		if (user){
			stp = f->rsp;
		}
		else {
			stp = thread_current()->rsp;
		}
		if (is_stack_valid(stp, (uintptr_t) addr)){
			vm_stack_growth(addr);
			return vm_claim_page(addr);
		}
		return false;
	}
	/* TODO: Your code goes here */

	return vm_do_claim_page (page);
}

bool
is_stack_valid(uintptr_t stp , uintptr_t addr) {
	if (addr >= stp - 8 ){
		return true;
	}
	
	if (addr < USER_STACK - (PGSIZE*256)) {
		return true;
	}

	return false;

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
vm_claim_page (void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function */
	page = spt_find_page(&thread_current()->spt, va);
	if (page == NULL){
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

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	if (!pml4_set_page(thread_current()->pml4, page->va, frame->kva, page->writable)){
		return false;
	};

	return swap_in (page, frame->kva);
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	// spt 구조체 hash table 초기화
	hash_init(&spt->hash_table, page_hash, page_less, NULL);

}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
	
	// dst 의 elem 을 src로 복사해주기.. vs spt_insert_page 쓰기 
	// hash table 순회
	struct hash_iterator i;
	hash_first(&i, &src->hash_table);
   	while (hash_next(&i)) {
   		struct page *src_page = hash_entry(hash_cur(&i), struct page, hash_elem);
		struct page *dst_page = malloc(sizeof *dst_page);

		if (dst_page == NULL){
			return false;
		}
		// copy member variables 
		*dst_page = *src_page;

		// if page type => UNINIT
		if (VM_TYPE(src_page->operations->type) == 0){
			dst_page->uninit.aux = NULL;
			if (!page_uninit_copy(src_page, dst_page)){
				vm_dealloc_page(dst_page);
				return false;
			}
		} // else page type => ANON, FILE
		else {
			if (!page_frame_copy(src_page, dst_page)){
				vm_dealloc_page(dst_page);
				return false;
			}
		}
		
		if ( (hash_insert(&dst->hash_table, &dst_page->hash_elem)) != NULL){
			vm_dealloc_page(dst_page);
			return false;
		}
   	
   	}
	return true;
}

bool
page_uninit_copy(struct page *src, struct page *dst){
	
	if (src->uninit.aux == NULL){
		return true;
	}
	
	struct binfo *aux = malloc(sizeof *aux); 
	if (aux == NULL){
		free(aux);
		return false;
	}
	memcpy(aux, src->uninit.aux, sizeof *aux);

	struct file *file = file_duplicate(aux->file);
	if (file == NULL){
		free(aux);
		return false;
	}
	aux->file = file;
	dst->uninit.aux = aux;
	return true;
}

bool 
page_frame_copy(struct page *src, struct page *dst){
	// 0. frame 할당 
	dst->frame = vm_get_frame();

	// 1. frame 내용 복사 
	memcpy(dst->frame->kva, src->frame->kva, PGSIZE);
	
	// 2. frame <-> page 링킹 
	dst->frame->page = dst;

	// 3. 페이지 테이블 매핑 
	if (!pml4_set_page(thread_current()->pml4, dst->va, dst->frame->kva, dst->writable)){
		return false;
	}

	return true;
	
}

// ALTER WAY -> process_exec() 재 초기화 필수 supplemental_page_table_init (&thread_current ()->spt);
// void supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
// 	/* TODO: Destroy all the supplemental_page_table hold by thread and
// 	 * TODO: writeback all the modified contents to the storage. */
// 	struct hash_iterator i;
// 	hash_first(&i, &spt->hash_table);
// 	struct hash_elem *e = (hash_next(&i));
//    	while ( e!= NULL ) {
//    		struct page *page = hash_entry(hash_cur(&i), struct page, hash_elem);
// 		e = hash_next(&i);
// 		vm_dealloc_page(page);
//    	}
// 	free(spt->hash_table.buckets);

// }
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	hash_clear(&spt->hash_table, page_destroy);

}

hash_action_func *
page_destroy(struct hash_elem *e, void *aux UNUSED){
	struct page *page = hash_entry(e, struct page, hash_elem);
    vm_dealloc_page(page);
}


unsigned
page_hash (const struct hash_elem *p_, void *aux){
	const struct page *p = hash_entry (p_, struct page, hash_elem);
  	return hash_bytes (&p->va, sizeof p->va);
}

bool
page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux) {
	const struct page *a = hash_entry (a_, struct page, hash_elem);
  	const struct page *b = hash_entry (b_, struct page, hash_elem);

  	return a->va < b->va;
}

