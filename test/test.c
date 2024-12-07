#include<volatile_page_store.h>

#include<string.h>
#include<stdio.h>

#include<callbacks_tupleindexer.h>
#include<test_tuple_infos.h>

volatile_page_store vps;

#define PAGE_SIZE (4 * 4096)
#define PAGE_ID_WIDTH 4
#define TRUNCATOR_PERIOD_US (20 * 1000000)

#define TESTCASE_SIZE 1000000

//#define PRINT_TUPLE

uint32_t inputs[TESTCASE_SIZE];
void generate_random_inputs()
{
	for(uint32_t i = 0; i < TESTCASE_SIZE; i++)
		inputs[i] = i;
	for(uint32_t i = 0; i < TESTCASE_SIZE; i++)
		memory_swap(inputs + (((uint32_t)rand())  % TESTCASE_SIZE), inputs + (((uint32_t)rand()) % TESTCASE_SIZE), sizeof(uint32_t));
	/*for(uint32_t i = 0; i < TESTCASE_SIZE; i++)
		printf("%u\n", inputs[i]);*/
}

const void* transaction_id = NULL;
int abort_error = 0;

#include<sorter.h>
#include<linked_page_list.h>

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
	printf("\n\nPERFORMING INSERTS\n\n");
	for(int i = 0; i < TESTCASE_SIZE; i++)
	{
		char record[900];
		construct_record(record, inputs[i], 0, "Rohan Dvivedi");
		if(!insert_in_sorter(&sh, record, transaction_id, &abort_error))
		{
			printf("failed to insert to sorter\n");
			exit(-1);
		}
	}

	// sort the contents of the sorter
	printf("\n\nPERFORMING SORTING\n\n");
	external_sort_merge_sorter(&sh, 100, transaction_id, &abort_error);

	// destroy sorter and extract the sorted values
	uint64_t sorted_data;
	destroy_sorter(&sh, &sorted_data, transaction_id, &abort_error);

	// print all sorted data 
	printf("\n\nPRINTING RESULTS\n\n");
	if(sorted_data != pam.pas.NULL_PAGE_ID)
	{
		// print them all popping them one after the another
		int counter = 0;
		{
			linked_page_list_iterator* lpli_p = get_new_linked_page_list_iterator(sorted_data, &(stdef.lpltd), &pam, &pmm, transaction_id, &abort_error);

			while(1)
			{
				counter++;
				const void* record = get_tuple_linked_page_list_iterator(lpli_p);

#ifdef PRINT_TUPLE
				print_tuple(record, &record_def);
#endif

				// if the record is at tail, break out
				if(is_at_tail_tuple_linked_page_list_iterator(lpli_p))
					break;

				if(!next_linked_page_list_iterator(lpli_p, transaction_id, &abort_error))
					break;
			}

			delete_linked_page_list_iterator(lpli_p, transaction_id, &abort_error);
		}

		printf("PRINTED %d SORTED TUPLES\n", counter);

		// if sorted_data exists then we also need to destroy the list
		destroy_linked_page_list(sorted_data, &(stdef.lpltd), &pam, transaction_id, &abort_error);
	}

	deinit_sorter_tuple_definitions(&stdef);

	deinitialize_tuple_defs();
}

#include<bplus_tree.h>

void main2()
{
	initialize_tuple_defs();

	bplus_tree_tuple_defs bpttd;
	if(!init_bplus_tree_tuple_definitions(&bpttd, &(pam.pas), &record_def, KEY_POS, CMP_DIR, RECORD_S_KEY_ELEMENT_COUNT))
	{
		printf("failed to initialize bplus tree tuple definitions\n");
		exit(-1);
	}

	// perform random 100,000 inserts
	printf("\n\nPERFORMING INSERTS\n\n");
	uint64_t root_page_id = get_new_bplus_tree(&bpttd, &pam, &pmm, transaction_id, &abort_error);
	printf("root of the sorter bplus tree = %"PRIu64"\n", root_page_id);
	for(int i = 0; i < TESTCASE_SIZE; i++)
	{
		char record[900];
		construct_record(record, inputs[i], 0, "Rohan Dvivedi");
		if(!insert_in_bplus_tree(root_page_id, record, &bpttd, &pam, &pmm, transaction_id, &abort_error))
		{
			printf("failed to insert to sorter bplus tree\n");
			exit(-1);
		}
	}

	printf("\n\nPRINTING RESULTS\n\n");
	{
		bplus_tree_iterator* bpi = find_in_bplus_tree(root_page_id, NULL, KEY_ELEMENT_COUNT, MIN, 0, READ_LOCK, &bpttd, &pam, NULL, transaction_id, &abort_error);

		int counter = 0;
		const void* curr_tuple = get_tuple_bplus_tree_iterator(bpi);
		while(curr_tuple != NULL)
		{
			counter++;

#ifdef PRINT_TUPLE
			print_tuple(curr_tuple, &record_def);
#endif

			if(!next_bplus_tree_iterator(bpi, transaction_id, &abort_error))
				break;

			curr_tuple = get_tuple_bplus_tree_iterator(bpi);
		}

		printf("PRINTED %d SORTED TUPLES\n", counter);

		delete_bplus_tree_iterator(bpi, transaction_id, &abort_error);
	}

	// destroy the bplus tree
	destroy_bplus_tree(root_page_id, &bpttd, &pam, transaction_id, &abort_error);

	deinit_bplus_tree_tuple_definitions(&bpttd);

	deinitialize_tuple_defs();
}

int compare_t(const void* i1, const void* i2)
{
	return compare_numbers((*((const uint32_t *)i1)), (*((const uint32_t*)i2)));
}

void main3()
{
	// sort the contents of the sorter
	printf("\n\nPERFORMING SORTING\n\n");
	qsort(inputs, TESTCASE_SIZE, sizeof(inputs[0]), compare_t);

	printf("\n\nPRINTING RESULTS\n\n");
	int counter = 0;
	for(uint32_t i = 0; i < TESTCASE_SIZE; i++)
	{
#ifdef PRINT_TUPLE
			printf("%"PRIu32"\n", inputs[i]);
#endif

		counter++;
	}
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
	generate_random_inputs();

	main1();
	//main2();
	//main3();
	//sleep((TRUNCATOR_PERIOD_US / 1000000) + 1);

	printf("total pages used = %"PRIu64"\n", vps.active_page_count);

	deinitialize_volatile_page_store(&vps);

	return 0;
}