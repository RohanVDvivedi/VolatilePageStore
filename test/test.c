#include<volatile_page_store.h>

#include<string.h>
#include<stdio.h>

#include<callbacks_tupleindexer.h>
#include<test_tuple_infos.h>

volatile_page_store vps;

#define PAGE_SIZE 4096
#define PAGE_ID_WIDTH 4
#define TRUNCATOR_PERIOD_US (20 * 1000000)

#define TESTCASE_SIZE 1000000

#include<sorter.h>

const void* transaction_id = NULL;
int abort_error = 0;

void main1()
{
	initialize_tuple_defs();

	sorter_tuple_defs stdef;
	if(!init_sorter_tuple_definitions(&stdef, &(pam.pas), &record_def, KEY_POS, CMP_DIR, RECORD_S_KEY_ELEMENT_COUNT))
	{
		printf("failed to initialize sorter tuple defs\n");
		exit(-1);
	}

	sorter_handle sh = get_new_sorter(&stdef, &pam, &pmm, transaction_id, &abort_error);

	// perform random 100,000 inserts
	for(int i = 0; i < TESTCASE_SIZE; i++)
	{
		char record[900];
		construct_record(record, rand() % TESTCASE_SIZE, 0, "Rohan Dvivedi");
		if(!insert_in_sorter(&sh, record, transaction_id, &abort_error))
		{
			printf("failed to insert to sorter\n");
			exit(-1);
		}
	}

	deinit_sorter_tuple_definitions(&stdef);

	deinitialize_tuple_defs();
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