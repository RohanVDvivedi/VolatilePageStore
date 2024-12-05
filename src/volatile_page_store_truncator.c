#include<volatile_page_store_truncator.h>

#include<errno.h>
#include<stdlib.h>
#include<pthread.h>

#include<volatile_page_store.h>

void perform_truncation(volatile_page_store* vps)
{
	// TODO
}

void* truncator(void* vps_v_p)
{
	volatile_page_store* vps = vps_v_p;

	pthread_mutex_lock(&(vps->manager_lock));
	vps->is_truncator_running = 1;
	pthread_mutex_unlock(&(vps->manager_lock));

	while(1)
	{
		pthread_mutex_lock(&(vps->manager_lock));

		while(!vps->truncator_shutdown_called)
		{
			struct timespec now;
			clock_gettime(CLOCK_REALTIME, &now);
			struct timespec diff = {.tv_sec = (vps->truncator_period_in_microseconds / 1000000LL), .tv_nsec = (vps->truncator_period_in_microseconds % 1000000LL) * 1000LL};
			struct timespec stop_at = {.tv_sec = now.tv_sec + diff.tv_sec, .tv_nsec = now.tv_nsec + diff.tv_nsec};
			stop_at.tv_sec += stop_at.tv_nsec / 1000000000LL;
			stop_at.tv_nsec = stop_at.tv_nsec % 1000000000LL;
			if(ETIMEDOUT == pthread_cond_timedwait(&(vps->wait_for_truncator_period), &(vps->manager_lock), &stop_at))
				break;
		}

		if(vps->truncator_shutdown_called)
		{
			pthread_mutex_unlock(&(vps->manager_lock));
			break;
		}

		// perform truncation
		perform_truncation(vps);

		pthread_mutex_unlock(&(vps->manager_lock));
	}

	pthread_mutex_lock(&(vps->manager_lock));
	vps->is_truncator_running = 0;
	pthread_cond_broadcast(&(vps->wait_for_truncator_to_stop));
	pthread_mutex_unlock(&(vps->manager_lock));

	return NULL;
}