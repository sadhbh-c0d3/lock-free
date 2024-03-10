#define MEAN_AND_LEAN
#include <Windows.h>
#include <process.h>
#include <stdio.h>

#include "ntringb.h"


typedef struct tagFOO {
	int x;
	int y;
} FOO, *PFOO;

typedef struct tagFOOTHREAD {
	int max_i;
	int max_j;
	int sleep_ms;
} FOOTHREAD, *PFOOTHREAD;

#define FOO_COUNT 8

FOO g_foo[FOO_COUNT];
volatile NTRINGB g_ringb;

void foo_producer(PFOOTHREAD p_foo_thread) {
	NTRINGB_POS ringb_pos;
	LONG pos;
	FOO current;
	int last_x = 1;
	int i;
	int j;

	ntringb_pos_init(&g_ringb, &ringb_pos);

	for (j = 0; j != p_foo_thread->max_j; ++j)
	{
		for (i = 0; i != p_foo_thread->max_i; ++i)
		{
			current.x = last_x;
			current.y = ++last_x;

			pos = ntringb_begin_write(&ringb_pos);

			memcpy(&g_foo[pos], &current, sizeof(FOO));
			
			ntringb_commit_write(&ringb_pos);
		}

		printf("Last_X = %d\n", last_x);
		Sleep(p_foo_thread->sleep_ms);
	}
}

void foo_consumer(PFOOTHREAD p_foo_thread) {
	NTRINGB_POS ringb_pos;
	LONG pos;
	FOO current;
	int i;
	
	ntringb_pos_init(&g_ringb, &ringb_pos);

	for (i = 0; i != (p_foo_thread->max_i * p_foo_thread->max_j); ++i)
	{
		pos = ntringb_begin_read(&ringb_pos);
		
		memcpy(&current, &g_foo[pos], sizeof(FOO));

		ntringb_commit_read(&ringb_pos);

		printf("Received: x = %d, y = %d\n", current.x, current.y);
	}
}

DWORD WINAPI foo_producer_thread(LPVOID p_param) {
	foo_producer((PFOOTHREAD)p_param);
	return 0;
}

DWORD WINAPI foo_consumer_thread(PVOID p_param) {
	foo_consumer((PFOOTHREAD)p_param);
	return 0;
}

void main_st() {
	FOOTHREAD foo_thread = { 4, 2, 0 };
	
	printf("Running Single-Threaded Example\n");

	ntringb_init(&g_ringb, FOO_COUNT);
	memset(g_foo, 0, sizeof(FOO) * FOO_COUNT);

	foo_producer(&foo_thread);
	foo_consumer(&foo_thread);
}

void main_mt(sleep_ms) {
	FOOTHREAD foo_thread = { 3, 4, sleep_ms };
	HANDLE h_producer_thread;
	HANDLE h_consumer_thread;

	printf("Running Multi-Threaded Example with Sleep %d milliseconds\n", sleep_ms);

	ntringb_init(&g_ringb, FOO_COUNT);
	memset(g_foo, 0, sizeof(FOO) * FOO_COUNT);

	h_consumer_thread = CreateThread(NULL, 0, &foo_consumer_thread, &foo_thread, 0, NULL);
	h_producer_thread = CreateThread(NULL, 0, &foo_producer_thread, &foo_thread, 0, NULL);

	WaitForSingleObject(h_consumer_thread, INFINITE);
	WaitForSingleObject(h_producer_thread, INFINITE);

	CloseHandle(h_consumer_thread);
	CloseHandle(h_producer_thread);
}

int main(int argc, const char** argv) {
	// Single threaded version will just send all elements at once, and then
	// will consume them all at once.
	main_st();
	
	// Multi-threaded version will send elements in bursts, and consume at they come.
	// The time delay parameter controls the race-conditions simulation.
	main_mt(1000);
	main_mt(100);
	main_mt(10);
	main_mt(1);
}
