#include <stdio.h>
#include <string.h>
#include <ulib.h>
#include "test.h"

RESULT
test_queue_push ()
{
	UQueue *queue = u_queue_new ();

	u_queue_push_head (queue, "foo");
	u_queue_push_head (queue, "bar");
	u_queue_push_head (queue, "baz");

	if (queue->length != 3)
		return FAILED ("push failed");

	if (NULL != queue->head->prev)
		return FAILED ("HEAD: prev is wrong");
	if (strcmp ("baz", queue->head->data))
		return FAILED ("HEAD: First element is wrong");
	if (strcmp ("bar", queue->head->next->data))
		return FAILED ("HEAD: Second element is wrong");
	if (strcmp ("foo", queue->head->next->next->data))
		return FAILED ("HEAD: Third element is wrong");
	if (NULL != queue->head->next->next->next)
		return FAILED ("HEAD: End is wrong");

	if (NULL != queue->tail->next)
		return FAILED ("TAIL: next is wrong");
	if (strcmp ("foo", queue->tail->data))
		return FAILED ("TAIL: Third element is wrong");
	if (strcmp ("bar", queue->tail->prev->data))
		return FAILED ("TAIL: Second element is wrong");
	if (strcmp ("baz", queue->tail->prev->prev->data))
		return FAILED ("TAIL: First element is wrong");
	if (NULL != queue->tail->prev->prev->prev)
		return FAILED ("TAIL: End is wrong");

	u_queue_free (queue);
	return OK;
}

RESULT
test_queue_push_tail ()
{
	UQueue *queue = u_queue_new ();

	u_queue_push_tail (queue, "baz");
	u_queue_push_tail (queue, "bar");
	u_queue_push_tail (queue, "foo");

	if (queue->length != 3)
		return FAILED ("push failed");

	if (NULL != queue->head->prev)
		return FAILED ("HEAD: prev is wrong");
	if (strcmp ("baz", queue->head->data))
		return FAILED ("HEAD: First element is wrong");
	if (strcmp ("bar", queue->head->next->data))
		return FAILED ("HEAD: Second element is wrong");
	if (strcmp ("foo", queue->head->next->next->data))
		return FAILED ("HEAD: Third element is wrong");
	if (NULL != queue->head->next->next->next)
		return FAILED ("HEAD: End is wrong");

	if (NULL != queue->tail->next)
		return FAILED ("TAIL: next is wrong");
	if (strcmp ("foo", queue->tail->data))
		return FAILED ("TAIL: Third element is wrong");
	if (strcmp ("bar", queue->tail->prev->data))
		return FAILED ("TAIL: Second element is wrong");
	if (strcmp ("baz", queue->tail->prev->prev->data))
		return FAILED ("TAIL: First element is wrong");
	if (NULL != queue->tail->prev->prev->prev)
		return FAILED ("TAIL: End is wrong");

	u_queue_free (queue);
	return OK;
}

RESULT
test_queue_pop ()
{
	UQueue *queue = u_queue_new ();
	void * data;

	u_queue_push_head (queue, "foo");
	u_queue_push_head (queue, "bar");
	u_queue_push_head (queue, "baz");

	data = u_queue_pop_head (queue);
	if (strcmp ("baz", data))
		return FAILED ("expect baz.");

	data = u_queue_pop_head (queue);
	if (strcmp ("bar", data))
		return FAILED ("expect bar.");	

	data = u_queue_pop_head (queue);
	if (strcmp ("foo", data))
		return FAILED ("expect foo.");
	
	if (u_queue_is_empty (queue) == FALSE)
		return FAILED ("expect is_empty.");

	if (queue->length != 0)
		return FAILED ("expect 0 length .");

	u_queue_push_head (queue, "foo");
	u_queue_push_head (queue, "bar");
	u_queue_push_head (queue, "baz");

	u_queue_pop_head (queue);

	if (NULL != queue->head->prev)
		return FAILED ("HEAD: prev is wrong");
	if (strcmp ("bar", queue->head->data))
		return FAILED ("HEAD: Second element is wrong");
	if (strcmp ("foo", queue->head->next->data))
		return FAILED ("HEAD: Third element is wrong");
	if (NULL != queue->head->next->next)
		return FAILED ("HEAD: End is wrong");

	if (NULL != queue->tail->next)
		return FAILED ("TAIL: next is wrong");
	if (strcmp ("foo", queue->tail->data))
		return FAILED ("TAIL: Second element is wrong");
	if (strcmp ("bar", queue->tail->prev->data))
		return FAILED ("TAIL: First element is wrong");
	if (NULL != queue->tail->prev->prev)
		return FAILED ("TAIL: End is wrong");

	u_queue_free (queue);
	return OK;
}

RESULT
test_queue_new ()
{
	UQueue *queue = u_queue_new ();

	if (queue->length != 0)
		return FAILED ("expect length == 0");

	if (queue->head != NULL)
		return FAILED ("expect head == NULL");

	if (queue->tail != NULL)
		return FAILED ("expect tail == NULL");

	u_queue_free (queue);
	return OK;
}

RESULT
test_queue_is_empty ()
{
	UQueue *queue = u_queue_new ();

	if (u_queue_is_empty (queue) == FALSE)
		return FAILED ("new queue should be empty");

	u_queue_push_head (queue, "foo");

	if (u_queue_is_empty (queue) == TRUE)
		return FAILED ("expected TRUE");

	u_queue_free (queue);

	return OK;
}

static Test queue_tests [] = {
	{    "push", test_queue_push},
	{"push_tail", test_queue_push_tail},
	{     "pop", test_queue_pop},
	{     "new", test_queue_new},
	{"is_empty", test_queue_is_empty},
	{NULL, NULL}
};

DEFINE_TEST_GROUP_INIT(queue_tests_init, queue_tests)

