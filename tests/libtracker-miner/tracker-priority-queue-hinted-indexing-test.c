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

/*
 * These tests are to ensure that the re-prioritizing and random picks of the
 * indexing queue works. Things not related structly to re-prioritizing and
 * randomness are tested elsewhere
 */

#include <glib-object.h>
#include <libtracker-miner/tracker-priority-queue.h>

typedef struct {
	TrackerPriorityQueue *queue;
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
	fix->queue = tracker_priority_queue_new ();
	const HintParameters *params = user_data;
	int i = 0;
	for (i = 0; i < params->numelements; i++) {
		tracker_priority_queue_add (fix->queue, params->elements[i], i);
	}
}

/*
 * Attempt popping a random item from an empty queue
 */
static void test_priority_queue_hinted_empty_random ()
{
	gpointer elem = NULL;
	TrackerPriorityQueue *queue = tracker_priority_queue_new ();
	elem = tracker_priority_queue_pop_random (queue);
	g_assert (elem == NULL);
}

/*
 * Attempt popping a random item from a single element queue
 */
static void test_priority_queue_hinted_one_elem_random ()
{
	gchar *added_element = "Hello world!";
	gpointer elem = NULL;
	TrackerPriorityQueue *queue = tracker_priority_queue_new ();
	tracker_priority_queue_add (queue, added_element, 0);
	elem = tracker_priority_queue_pop_random (queue);
	g_assert (elem == added_element);
}

/*
 * Randomply popping from two identical queues shouldn't (with high
 * probability) give the same element.
 */
static void test_priority_queue_hinted_popping_is_random ()
{
	TrackerPriorityQueue *queue1 = tracker_priority_queue_new ();
	TrackerPriorityQueue *queue2 = tracker_priority_queue_new ();
	int i = 0;
	for (i = 0; i < 100000; i++) {
		gpointer elem = g_malloc0(1);
		tracker_priority_queue_add (queue1, elem, 0);
		tracker_priority_queue_add (queue2, elem, 0);
	}
	g_assert (tracker_priority_queue_pop_random (queue1) !=
	          tracker_priority_queue_pop_random (queue2));
}

/*
 * Changing the priority for a file should put it in the right place in the
 * queue, and also actually set the priority of this element
 */
static gboolean stringmatcher (TrackerPriorityQueueCriteria *criteria, 
                               gpointer user_data) {
	return g_regex_match_simple (user_data, criteria->node->data, 0,0);
}
static void test_priority_queue_hinted_can_prioritize_string (HintFixture *fix,
                                                              gconstpointer user_data)
{
	gpointer before = NULL;
	gpointer after  = NULL;
	int prio;
	before = tracker_priority_queue_peek (fix->queue, &prio);
	tracker_priority_queue_prioritize (fix->queue, stringmatcher, -1, ".*C.*");
	after  = tracker_priority_queue_peek (fix->queue, &prio);
	g_assert (prio == -1);
	g_assert (g_strcmp0 (after, "_C_") == 0);
	g_assert (before != after);
}

static void test_priority_queue_hinted_can_prioritize_empty ()
{
	int prio;
	TrackerPriorityQueue *queue = tracker_priority_queue_new ();
	tracker_priority_queue_prioritize (queue, stringmatcher, 0, ".*C.*");
	g_assert (tracker_priority_queue_peek (queue, &prio) == NULL);
}

static void test_priority_queue_hinted_correctly_counts_matches (HintFixture *fix,
                                                                 gconstpointer user_data)
{
	int hits = tracker_priority_queue_prioritize (fix->queue,
	                                              stringmatcher,
                                                      -1,
                                                      "_A.*");
	int hits2 = tracker_priority_queue_get_num_remaining (fix->queue);
	int hits3 = tracker_priority_queue_dec_num_remaining (fix->queue);
	int i = 100;
	for (; i != 0; i--) {
		g_assert (tracker_priority_queue_dec_num_remaining (fix->queue) >= 0);
	}

	g_assert (tracker_priority_queue_get_num_remaining (fix->queue) == 0);
	g_assert (hits == 2);
	g_assert (hits == hits2);
	g_assert (hits-1 == hits3);
}

int
main (int    argc,
      char **argv)
{
	g_test_init (&argc, &argv, NULL);
	gchar *testelements[] = {"_A_", "_B_", "_C_", "_D_", "_AB_"};

	g_test_add_func ("/libtracker-miner/tracker-priority-queue-hinted-indexing/empty-random",
	                 test_priority_queue_hinted_empty_random);
	g_test_add_func ("/libtracker-miner/tracker-priority-queue-hinted-indexing/one-elem-random",
	                 test_priority_queue_hinted_one_elem_random);
	g_test_add_func ("/libtracker-miner/tracker-priority-queue-hinted-indexing/popping-is-random",
	                 test_priority_queue_hinted_popping_is_random);
	g_test_add_func ("/libtracker-miner/tracker-priority-queue-hinted-indexing/can-prioritize-empty",
	                 test_priority_queue_hinted_can_prioritize_empty);
	g_test_add ("/libtracker-miner/tracker-priority-queue-hinted-indexing/can-prioritize-string",
	            HintFixture, init_parameters (testelements, 5), fixture_add_elements,
	            test_priority_queue_hinted_can_prioritize_string, NULL);
	g_test_add ("/libtracker-miner/tracker-priority-queue-hinted-indexing/correctly-counts-matches",
	            HintFixture, init_parameters (testelements, 5), fixture_add_elements,
	            test_priority_queue_hinted_correctly_counts_matches, NULL);

	return g_test_run ();
}
