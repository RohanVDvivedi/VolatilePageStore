#include<frame_desc.h>

void initialize_empty_frame_desc(frame_desc* fd)
{
	fd->map = (frame_desc_mapping){};
	fd->reference_counter = 0;

	initialize_bstnode(&(fd->embed_node_page_id_to_frame_desc));
	initialize_bstnode(&(fd->embed_node_frame_ptr_to_frame_desc));
	initialize_llnode(&(fd->llnode embed_node_lru_lists));
}

#include<cutlery_math.h>

cy_uint hash_frame_desc_by_page_id(const void* pd_p)
{
	return ((const frame_desc*)pd_p)->map.page_id;
}

int compare_frame_desc_by_page_id(const void* pd1_p, const void* pd2_p)
{
	return compare_numbers(((const frame_desc*)pd1_p)->map.page_id, ((const frame_desc*)pd2_p)->map.page_id);
}

cy_uint hash_frame_desc_by_frame_ptr(const void* pd_p)
{
	return (cy_uint)(((const frame_desc*)pd_p)->map.frame);
}

int compare_frame_desc_by_frame_ptr(const void* pd1_p, const void* pd2_p)
{
	return compare_numbers(((const frame_desc*)pd1_p)->map.frame, ((const frame_desc*)pd2_p)->map.frame);
}