#include<tupleindexer/interface/page_access_methods.h>
#include<tupleindexer/interface/page_modification_methods.h>

//#define GENERATE_TRACE

void* get_new_page_with_write_lock_vps(void* context, const void* transaction_id, uint64_t* page_id_returned, int* abort_error)
{
	#ifdef GENERATE_TRACE
		printf("%p get_new_page_with_write_lock_vps\n", transaction_id);
	#endif
	return get_new_page_for_vps(context, page_id_returned);
}
void* acquire_page_with_reader_lock_vps(void* context, const void* transaction_id, uint64_t page_id, int* abort_error)
{
	#ifdef GENERATE_TRACE
		printf("%p acquire_page_with_reader_lock_vps\n", transaction_id);
	#endif
	return acquire_page_for_vps(context, page_id);
}
void* acquire_page_with_writer_lock_vps(void* context, const void* transaction_id, uint64_t page_id, int* abort_error)
{
	#ifdef GENERATE_TRACE
		printf("%p acquire_page_with_writer_lock_vps\n", transaction_id);
	#endif
	return acquire_page_for_vps(context, page_id);
}
int downgrade_writer_lock_to_reader_lock_on_page_vps(void* context, const void* transaction_id, void* pg_ptr, int opts, int* abort_error)
{
	#ifdef GENERATE_TRACE
		printf("%p downgrade_writer_lock_to_reader_lock_on_page_vps\n", transaction_id);
	#endif
	return 1;
}
int upgrade_reader_lock_to_writer_lock_on_page_vps(void* context, const void* transaction_id, void* pg_ptr, int* abort_error)
{
	#ifdef GENERATE_TRACE
		printf("%p upgrade_reader_lock_to_writer_lock_on_page_vps\n", transaction_id);
	#endif
	return 1;
}
int release_reader_lock_on_page_vps(void* context, const void* transaction_id, void* pg_ptr, int opts, int* abort_error)
{
	#ifdef GENERATE_TRACE
		printf("%p release_reader_lock_on_page_vps\n", transaction_id);
	#endif
	release_page_for_vps(context, pg_ptr, !!(opts & FREE_PAGE));
	return 1;
}
int release_writer_lock_on_page_vps(void* context, const void* transaction_id, void* pg_ptr, int opts, int* abort_error)
{
	#ifdef GENERATE_TRACE
		printf("%p release_writer_lock_on_page_vps\n", transaction_id);
	#endif
	release_page_for_vps(context, pg_ptr, !!(opts & FREE_PAGE));
	return 1;
}
int free_page_vps(void* context, const void* transaction_id, uint64_t page_id, int* abort_error)
{
	#ifdef GENERATE_TRACE
		printf("%p free_page_vps\n", transaction_id);
	#endif
	free_page_for_vps(context, page_id);
	return 1;
}

page_access_methods pam;

void init_pam_for_vps(volatile_page_store* vps)
{
	pam = (page_access_methods){
		.get_new_page_with_write_lock = get_new_page_with_write_lock_vps,
		.acquire_page_with_reader_lock = acquire_page_with_reader_lock_vps,
		.acquire_page_with_writer_lock = acquire_page_with_writer_lock_vps,
		.downgrade_writer_lock_to_reader_lock_on_page = downgrade_writer_lock_to_reader_lock_on_page_vps,
		.upgrade_reader_lock_to_writer_lock_on_page = upgrade_reader_lock_to_writer_lock_on_page_vps,
		.release_reader_lock_on_page = release_reader_lock_on_page_vps,
		.release_writer_lock_on_page = release_writer_lock_on_page_vps,
		.free_page = free_page_vps,
		.pas = (page_access_specs){},
		.context = vps,
	};
	if(!initialize_page_access_specs(&(pam.pas), vps->user_stats.page_id_width, vps->user_stats.page_size, vps->user_stats.NULL_PAGE_ID))
		exit(-1);
}

page_modification_methods pmm;

#include<tupleindexer/interface/unWALed_page_modification_methods.h>

void init_pmm_for_vps()
{
	page_modification_methods* pmm_p = get_new_unWALed_page_modification_methods();
	pmm = *pmm_p;
	delete_unWALed_page_modification_methods(pmm_p);
}