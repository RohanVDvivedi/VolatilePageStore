#include<volatile_page_store.h>

#include<string.h>
#include<stdio.h>

#include<callbacks_tupleindexer.h>
#include<test_tuple_infos.h>

volatile_page_store vps;

#define PAGE_SIZE 4096
#define PAGE_ID_WIDTH 4
#define TRUNCATOR_PERIOD_US (20 * 1000000)

void main1()
{

}

int main()
{
	if(!initialize_volatile_page_store(&vps, ".", PAGE_SIZE, PAGE_ID_WIDTH, TRUNCATOR_PERIOD_US))
	{
		printf("failed to initialize volatile page store\n");
		exit(-1);
	}
	init_pam_for_vps(&vps);
	init_pmm_for_vps();

	// seed random number generator
	srand(time(NULL));

	main1();
	//sleep((TRUNCATOR_PERIOD_US / 1000000) + 1);

	printf("total pages used = %"PRIu64"\n", vps.active_page_count);

	deinitialize_volatile_page_store(&vps);

	return 0;
}