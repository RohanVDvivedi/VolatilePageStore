#include<tuple.h>

data_type_info digits_type_info;
data_type_info num_in_words_type_info;
data_type_info value_string_type_info;
data_type_info* record_type_info;
tuple_def record_def;

#define RECORD_S_KEY_ELEMENT_COUNT 2
positional_accessor KEY_POS[2] = {STATIC_POSITION(0), STATIC_POSITION(2)};
compare_direction CMP_DIR[2] = {ASC, ASC};

void initialize_tuple_defs()
{
	record_type_info = malloc(sizeof_tuple_data_type_info(5));
	initialize_tuple_data_type_info(record_type_info, "record", 0, 900, 5);

	strcpy(record_type_info->containees[0].field_name, "num");
	record_type_info->containees[0].type_info = UINT_NON_NULLABLE[8];

	strcpy(record_type_info->containees[1].field_name, "order");
	record_type_info->containees[1].type_info = INT_NON_NULLABLE[1];

	num_in_words_type_info = get_variable_length_string_type("num_in_words", 70);
	strcpy(record_type_info->containees[2].field_name, "num_in_words");
	record_type_info->containees[2].type_info = &num_in_words_type_info;

	digits_type_info = get_variable_element_count_array_type("digits", 16, UINT_NON_NULLABLE[1]);
	strcpy(record_type_info->containees[3].field_name, "digits");
	record_type_info->containees[3].type_info = &digits_type_info;

	value_string_type_info = get_variable_length_string_type("value_in_string", 100);
	strcpy(record_type_info->containees[4].field_name, "value_in_string");
	record_type_info->containees[4].type_info = &value_string_type_info;

	initialize_tuple_def(&record_def, record_type_info);

	print_tuple_def(&record_def);
	printf("\n\n");
}

#define BUFFER_SIZE 300

const char *ones[] = {
  "zero", "one", "two", "three", "four", "five", "six", "seven",
  "eight", "nine", "ten", "eleven", "twelve", "thirteen", "fourteen",
  "fifteen", "sixteen", "seventeen", "eighteen", "nineteen"
};

const char *tens[] = {
  "", "", "twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety"
};

void num_in_words(char* output, uint16_t n) {
  if (n < 20) {
    strcpy(output, ones[n]);
  } else if (n < 100) {
    strcpy(output, tens[(n / 10) % 10]);
    if((n % 10) != 0) {
    	strcat(output, " ");
    	strcat(output, ones[n % 10]);
    }
  } else if (n < 1000) {
  	strcpy(output, ones[(n / 100) % 10]);
  	strcat(output, " hundred");
  	if(n % 100 != 0) {
  		strcat(output, " ");
			char temp[100];
			num_in_words(temp, n % 100);
  		strcat(output, temp);
  	}
  } else {
  	strcpy(output, "TOO00-BIG");
  }
}

uint16_t find_order(uint64_t num, int order)
{
	switch(order)
	{
		case 0:
			return (num / 1ULL) % 1000;
		case 1:
			return (num / 1000ULL) % 1000;
		case 2:
			return (num / 1000000ULL) % 1000;
		case 3:
			return (num / 1000000000ULL) % 1000;
		case 4:
		{
			printf("ORDER TOO BIG\n");
			exit(-1);
		}
	}
	return 0;
}

void construct_record(void* buffer, uint64_t num, int order, char* value)
{
	init_tuple(&record_def, buffer);

	set_element_in_tuple(&record_def, STATIC_POSITION(0), buffer, &(user_value){.uint_value = num}, UINT32_MAX);

	uint16_t o = find_order(num, order);
	set_element_in_tuple(&record_def, STATIC_POSITION(1), buffer, &(user_value){.int_value = order}, UINT32_MAX);

	char temp[100];
	num_in_words(temp, o);
	set_element_in_tuple(&record_def, STATIC_POSITION(2), buffer, &(user_value){.string_value = temp, .string_size = strlen(temp)}, UINT32_MAX);

	set_element_in_tuple(&record_def, STATIC_POSITION(3), buffer, EMPTY_USER_VALUE, UINT32_MAX);
	uint32_t size = 0;
	while(num > 0)
	{
		uint8_t d = num % 10;
		if(!expand_element_count_for_element_in_tuple(&record_def, STATIC_POSITION(3), buffer, size, 1, UINT32_MAX))
			break;
		size++;
		set_element_in_tuple(&record_def, STATIC_POSITION(3,(size-1)), buffer, &(user_value){.uint_value = d}, UINT32_MAX);
		num = num / 10;
	}

	if(value == NULL)
		set_element_in_tuple(&record_def, STATIC_POSITION(4), buffer, NULL_USER_VALUE, UINT32_MAX);
	else
		set_element_in_tuple(&record_def, STATIC_POSITION(4), buffer, &(user_value){.string_value = value, .string_size = strlen(value)}, UINT32_MAX);
}


void deinitialize_tuple_defs()
{
	free(record_type_info);
}