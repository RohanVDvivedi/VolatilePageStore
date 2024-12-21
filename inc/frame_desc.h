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

	llnode embed_node_lru_lists;
};

#endif