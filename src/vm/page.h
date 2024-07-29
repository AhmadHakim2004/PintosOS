#include <hash.h>
#include <stdbool.h>
#include "filesys/file.h"
#include "threads/thread.h"
#include "vm/frame.h"


/* Virtual Page Type */
enum vpt
	{
		ELF_FILE,							/* Exeutable file */
		MAPPED_FILE,                        /* Mapped file */
		SWAP                                /* Swap page */
	};


/* Structure representing an entry in the supplemental page table */
struct spt_entry
	{
		struct hash_elem hash_elem;				/* Hash elem for hash table */
		uint8_t *uaddr;										/* User virtual address for page */	
		struct frame *frame;							/* Pointer to frame struct */
		struct file *file;								/* Pointer to file */
		off_t file_offset;								/* File offset */
		size_t file_page_size;						/* Size of bytes to read into page */
		bool writable;										/* File is writable or not */
		bool in_memory;										/* If page is in memory */
		enum vpt vpt;											/* Page type */
		int swap_index;										/* Swap index */
	};	

void init_spt (struct thread *);
struct spt_entry * init_spt_entry (uint8_t *, struct frame *, struct file *, 
								    off_t, size_t, bool, bool, enum vpt);
struct spt_entry *get_spt_entry(void *uaddr);
bool load_file_page_to_mem (void *kpage, struct spt_entry *);
void spt_entry_destory (struct hash_elem *, void *);