#include<frame_desc.h>

void initialize_empty_frame_desc(frame_desc* fd)
{
	fd->map = (frame_desc_mapping){};
	fd->reference_counter = 0;

	initialize_bstnode(&(fd->embed_node_page_id_to_frame_desc));
	initialize_bstnode(&(fd->embed_node_frame_ptr_to_frame_desc));
	initialize_llnode(&(fd->llnode embed_node_lru_lists));
}