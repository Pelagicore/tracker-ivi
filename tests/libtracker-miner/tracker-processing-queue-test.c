/*
 * Copyright (C) 2013, Pelagicore AB <jonatan.palsson@pelagicore.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <glib-object.h>
#include <libtracker-miner/tracker-processing-queue.h>
#include "gio/gio.h"

typedef struct {
	TrackerProcessingQueue *queue;
} HintFixture;

typedef struct {
	gchar **elements;
	guint numelements;
} HintParameters;

static HintParameters *init_parameters (gchar **elems, guint numelems)
{
	HintParameters *hp = g_malloc (sizeof (HintParameters));
	hp->elements = elems;
	hp->numelements = numelems;
	return hp;
}

static void fixture_add_elements (HintFixture *fix,
                                       gconstpointer user_data)
{
	fix->queue = tracker_processing_queue_new ();
	const HintParameters *params = user_data;
	int i = 0;
	for (i = 0; i < params->numelements; i++) {
		tracker_processing_queue_add (fix->queue, params->elements[i]);
	}
}

static void test_processing_queue_can_create ()
{
	TrackerProcessingQueue *queue = tracker_processing_queue_new ();
	g_assert (TRACKER_IS_PROCESSING_QUEUE (queue));
}

/*
 * Attempt popping a random item from an empty queue
 */
static void test_processing_queue_pop_empty_random ()
{
	gpointer elem = NULL;
	TrackerProcessingQueue *queue = tracker_processing_queue_new ();
	elem = tracker_processing_queue_pop (queue);
	g_assert (elem == NULL);
}

/*
 * Attempt popping a random item from a single element queue
 */
static void test_processing_queue_pop_one_elem_random ()
{
	gchar *added_element = "Hello world!";
	gpointer elem = NULL;
	TrackerProcessingQueue *queue = tracker_processing_queue_new ();
	tracker_processing_queue_add (queue, added_element);
	elem = tracker_processing_queue_pop (queue);
	g_assert (elem == added_element);
}

/*
 * Verify that peeking an item and then popping an item yields the same item
 */
static void test_processing_queue_peeking_and_popping_yields_the_same (HintFixture *fix,
                                                                       gconstpointer user_data)
{
	gpointer peeked = NULL;
	gpointer popped = NULL;
	peeked = tracker_processing_queue_peek (fix->queue);
	popped = tracker_processing_queue_pop  (fix->queue);
	g_assert (peeked == popped);
	g_assert (peeked != NULL);
}

/*
 * Verify that peek doesn't change the value returned
 */
static void test_processing_queue_peeking_doesnt_modify_queue (HintFixture *fix,
                                                               gconstpointer user_data)
{
	gpointer peeked1 = NULL;
	gpointer peeked2 = NULL;
	gpointer peeked3 = NULL;
	peeked1 = tracker_processing_queue_peek (fix->queue);
	peeked2 = tracker_processing_queue_peek (fix->queue);
	peeked3 = tracker_processing_queue_peek (fix->queue);
	g_assert (peeked1 == peeked2 && peeked1 == peeked3);
}

/*
 * Randomply popping from two identical queues shouldn't (with high
 * probability) give the same element.
 */
static void test_processing_queue_popping_is_random ()
{
	TrackerProcessingQueue *queue1 = tracker_processing_queue_new ();
	TrackerProcessingQueue *queue2 = tracker_processing_queue_new ();
	int i = 0;
	for (i = 0; i < 100000; i++) {
		gpointer elem = g_malloc0(1);
		tracker_processing_queue_add (queue1, elem);
		tracker_processing_queue_add (queue2, elem);
	}
	g_assert (tracker_processing_queue_pop (queue1) !=
	          tracker_processing_queue_pop (queue2));
}

/*
 * Changing the priority for a file should put it in the right place in the
 * queue, and also actually set the priority of this element
 */
static void test_processing_queue_can_prioritize_string (HintFixture *fix,
                                                         gconstpointer user_data)
{
	gpointer after  = NULL;
	tracker_processing_queue_peek (fix->queue);
	tracker_processing_queue_prioritize (fix->queue, "_C_");
	after  = tracker_processing_queue_peek (fix->queue);
	g_assert (g_strcmp0 (after, "_C_") == 0);
}

/* 
 * Verify that we can look up a GFile
 */
static gpointer keying_func (gpointer key) {
	return g_file_get_path (g_file_get_parent ( (GFile *)key));
}
static void test_processing_queue_can_prioritize_gfile ()
{
	GFile *f1 = g_file_new_for_uri ("file:///x/FILE1");
	GFile *f2 = g_file_new_for_uri ("file:///y/FILE2");
	GFile *f3 = g_file_new_for_uri ("file:///z/FILE3");
	TrackerProcessingQueue *queue = tracker_processing_queue_new_full (
	                                                       keying_func,
	                                                       g_str_equal);
	g_assert (queue != NULL);
	tracker_processing_queue_add (queue, f1);
	tracker_processing_queue_add (queue, f2);
	tracker_processing_queue_add (queue, f3);
	tracker_processing_queue_prioritize (queue, "/x");
	g_assert (tracker_processing_queue_pop (queue) == f1);
}

/*
 * Verify that hinted popping keeps in sync with random popping also when
 * keys are identical
 */
static void test_processing_queue_can_handle_identical_keys ()
{
	GFile    *f1     = g_file_new_for_uri ("file:///x/FILE1");
	GFile    *f2     = g_file_new_for_uri ("file:///x/FILE2");
	GFile    *f3     = g_file_new_for_uri ("file:///x/FILE3");
	GFile    *f4     = g_file_new_for_uri ("file:///x/FILE4");
	gpointer  after  = NULL;
	gpointer  after2 = NULL;

	TrackerProcessingQueue *queue = tracker_processing_queue_new_full (
	                                                       keying_func,
	                                                       g_str_equal);
	g_assert (queue != NULL);
	tracker_processing_queue_add (queue, f1);
	tracker_processing_queue_add (queue, f2);
	tracker_processing_queue_add (queue, f3);
	tracker_processing_queue_add (queue, f4);
	tracker_processing_queue_prioritize (queue, "/x");
	after = tracker_processing_queue_pop (queue);

	tracker_processing_queue_prioritize (queue, "/NONEXISTANT");
	after2 = tracker_processing_queue_pop (queue);
	g_assert (after2 != NULL);
	g_assert (after2 != after);
	
	after2 = tracker_processing_queue_pop (queue);
	g_assert (after2 != NULL);
	g_assert (after2 != after);

	after2 = tracker_processing_queue_pop (queue);
	g_assert (after2 != NULL);
	g_assert (after2 != after);
}

/*
 * Verify that we can prioritize an empty queue
 */
static void test_processing_queue_can_prioritize_empty ()
{
	TrackerProcessingQueue *queue = tracker_processing_queue_new ();
	tracker_processing_queue_prioritize (queue, ".*C.*");
	g_assert (tracker_processing_queue_peek (queue) == NULL);
}

/*
 * Popping in hinted mode should eventually exhaust the elements
 */
static void test_processing_queue_hinted_popping_removes_element (HintFixture *fix,
                                                                  gconstpointer user_data)
{
	gpointer after = NULL;
	tracker_processing_queue_prioritize (fix->queue, "_C_");
	after  = tracker_processing_queue_pop (fix->queue);
	g_assert (g_strcmp0 (after, "_C_") == 0);

	tracker_processing_queue_prioritize (fix->queue, "_A_");
	after  = tracker_processing_queue_pop (fix->queue);
	g_assert (g_strcmp0 (after, "_A_") == 0);

	tracker_processing_queue_prioritize (fix->queue, "_B_");
	after  = tracker_processing_queue_pop (fix->queue);
	g_assert (g_strcmp0 (after, "_B_") == 0);

	tracker_processing_queue_prioritize (fix->queue, "_D_");
	after  = tracker_processing_queue_pop (fix->queue);
	g_assert (g_strcmp0 (after, "_D_") == 0);

	tracker_processing_queue_prioritize (fix->queue, "_AB_");
	after  = tracker_processing_queue_pop (fix->queue);
	g_assert (g_strcmp0 (after, "_AB_") == 0);

	after = tracker_processing_queue_pop (fix->queue);
	g_assert (after == NULL);
	
}

/*
 * Popping in random mode should eventually exhaust the elements
 */
static void test_processing_queue_random_pop_removes_element (HintFixture *fix,
                                                              gconstpointer user_data)
{
	gpointer after = NULL;
	after = tracker_processing_queue_pop (fix->queue);
	g_assert (after != NULL);
	after = tracker_processing_queue_pop (fix->queue);
	g_assert (after != NULL);
	after = tracker_processing_queue_pop (fix->queue);
	g_assert (after != NULL);
	after = tracker_processing_queue_pop (fix->queue);
	g_assert (after != NULL);
	after = tracker_processing_queue_pop (fix->queue);
	g_assert (after != NULL);
	after = tracker_processing_queue_pop (fix->queue);
	g_assert (after == NULL);
}

/*
 * Ensure a random pop also removes the popped element from the HT
 */
static void test_processing_queue_random_and_hinted_are_synced (HintFixture *fix,
                                                                gconstpointer user_data)
{
	gpointer after  = NULL;
	gpointer after2 = NULL;
	after = tracker_processing_queue_pop (fix->queue);

	tracker_processing_queue_prioritize (fix->queue, after);
	after2 = tracker_processing_queue_pop (fix->queue);
	g_assert (after != after2);
	g_assert (after2 != NULL);
}

/*
 * Ensure a hinted pop also removes the popped element from the array
 */
static void test_processing_queue_random_and_hinted_are_synced2 (HintFixture *fix,
                                                                gconstpointer user_data)
{
	gpointer after  = NULL;
	tracker_processing_queue_prioritize (fix->queue, "_A_");
	tracker_processing_queue_pop (fix->queue);

	tracker_processing_queue_prioritize (fix->queue, "_B_");
	tracker_processing_queue_pop (fix->queue);

	tracker_processing_queue_prioritize (fix->queue, "_C_");
	tracker_processing_queue_pop (fix->queue);

	tracker_processing_queue_prioritize (fix->queue, "_D_");
	tracker_processing_queue_pop (fix->queue);

	after = tracker_processing_queue_pop (fix->queue);
	g_assert (g_strcmp0 (after, "_AB_") == 0);
}
//static void test_processing_queue_correctly_counts_matches (HintFixture *fix,
//                                                                 gconstpointer user_data)
//{
//	int hits = tracker_processing_queue_prioritize (fix->queue,
//	                                              stringmatcher,
//                                                      -1,
//                                                      "_A.*");
//	int hits2 = tracker_priority_queue_get_num_remaining (fix->queue);
//	int hits3 = tracker_priority_queue_dec_num_remaining (fix->queue);
//	int i = 100;
//	for (; i != 0; i--) {
//		g_assert (tracker_priority_queue_dec_num_remaining (fix->queue) >= 0);
//	}
//
//	g_assert (tracker_priority_queue_get_num_remaining (fix->queue) == 0);
//	g_assert (hits == 2);
//	g_assert (hits == hits2);
//	g_assert (hits-1 == hits3);
//}

int
main (int    argc,
      char **argv)
{
	g_test_init (&argc, &argv, NULL);
	gchar *testelements[] = {"_A_", "_B_", "_C_", "_D_", "_AB_"};

	g_test_add_func ("/libtracker-miner/tracker-processing-queue/can-create",
	                 test_processing_queue_can_create);
	g_test_add_func ("/libtracker-miner/tracker-processing-queue/empty-random",
	                 test_processing_queue_pop_empty_random);
	g_test_add_func ("/libtracker-miner/tracker-processing-queue/one-elem-random",
	                 test_processing_queue_pop_one_elem_random);
	g_test_add_func ("/libtracker-miner/tracker-processing-queue/can-prioritize-gfile",
	                 test_processing_queue_can_prioritize_gfile);
	g_test_add_func ("/libtracker-miner/tracker-processing-queue/can-prioritize-empty",
	                 test_processing_queue_can_prioritize_empty);
	g_test_add_func ("/libtracker-miner/tracker-processing-queue/popping-is-random",
	                 test_processing_queue_popping_is_random);
	g_test_add_func ("/libtracker-miner/tracker-processing-queue/can-handle-identical-keys",
	                 test_processing_queue_can_handle_identical_keys);
	g_test_add ("/libtracker-miner/tracker-processing-queue/peeking-and-popping-yields-same-elem",
	            HintFixture, init_parameters (testelements, 5), fixture_add_elements,
	            test_processing_queue_peeking_and_popping_yields_the_same, NULL);
	g_test_add ("/libtracker-miner/tracker-processing-qyeye/peeking-doesnt-modify-queue",
	            HintFixture, init_parameters (testelements, 5), fixture_add_elements,
	            test_processing_queue_peeking_doesnt_modify_queue, NULL);
	g_test_add ("/libtracker-miner/tracker-processing-queue/can-prioritize-string",
	            HintFixture, init_parameters (testelements, 5), fixture_add_elements,
	            test_processing_queue_can_prioritize_string, NULL);
	g_test_add ("/libtracker-miner/tracker-processing-queue/hinted-popping-removes-element",
	            HintFixture, init_parameters (testelements, 5), fixture_add_elements,
	            test_processing_queue_hinted_popping_removes_element, NULL);
	g_test_add ("/libtracker-miner/tracker-processing-queue/random-pop-removes-element",
	            HintFixture, init_parameters (testelements, 5), fixture_add_elements,
	            test_processing_queue_random_pop_removes_element, NULL);
	g_test_add ("/libtracker-miner/tracker-processing-queue/random-and-hinted-are-synced",
	            HintFixture, init_parameters (testelements, 5), fixture_add_elements,
	            test_processing_queue_random_and_hinted_are_synced, NULL);
	g_test_add ("/libtracker-miner/tracker-processing-queue/random-and-hinted-are-synced2",
	            HintFixture, init_parameters (testelements, 5), fixture_add_elements,
	            test_processing_queue_random_and_hinted_are_synced2, NULL);
//	g_test_add ("/libtracker-miner/tracker-priority-queue-hinted-indexing/correctly-counts-matches",
//	            HintFixture, init_parameters (testelements, 5), fixture_add_elements,
//	            test_processing_queue_correctly_counts_matches, NULL);

	return g_test_run ();
}
