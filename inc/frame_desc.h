#ifndef FRAME_DESC_H
#define FRAME_DESC_H

struct frame_desc
{
	struct frame_desc_mapping
	{
		// page id of a valid frame
		uint64_t page_id;

		// memory contents of a valid frame pointed to by page_id
		void* frame;
	} map;

	// this frame_desc exists in unreferenced_frame_descs_lru_lists only if its reference_counter is 0
	uint64_t reference_counter;

	// -------------------------------------
	// --------- embedded nodes ------------
	// -------------------------------------

	bstnode embed_node_page_id_to_frame_desc;

	bstnode embed_node_frame_ptr_to_frame_desc;

	llnode embed_node_unreferenced_lru_list;
};

void initialize_empty_frame_desc(frame_desc* fd);

// utiiity functions for frame_desc hashmaps and linkedlists

cy_uint hash_frame_desc_by_page_id(const void* fd);

int compare_frame_desc_by_page_id(const void* fd1, const void* fd2);

cy_uint hash_frame_desc_by_frame_ptr(const void* fd);

int compare_frame_desc_by_frame_ptr(const void* fd1, const void* fd2);

#endif