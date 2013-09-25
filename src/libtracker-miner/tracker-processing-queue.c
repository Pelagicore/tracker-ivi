#include "glib.h"
#include "libtracker-miner/tracker-processing-queue.h"
#include "gio/gio.h"

struct _TrackerProcessingQueuePrivate {
	int         next_random_idx;
	GPtrArray  *elem_array;
	GHashTable *elem_ht;
	GQueue     *hints;
	gpointer  (*key_func)   (gpointer);
	GEqualFunc  lookup_func;
};

enum {
	PROP_0,

	PROP_KEYING_FUNCTION,
	PROP_LOOKUP_FUNCTION,

	N_PROPERTIES
};

struct ElemPtr {
	gpointer ptr;
	gboolean removed;
};

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

static gpointer pop_random (TrackerProcessingQueue *queue, gboolean remove);
static gpointer pop_hinted (TrackerProcessingQueue *queue, gboolean remove);
static gpointer tracker_processing_queue_get (TrackerProcessingQueue *queue, gboolean remove_element);

static void tracker_processing_queue_set_property (
		GObject *queue, guint property_id, const GValue *value,
		GParamSpec *pspec);
static void tracker_processing_queue_get_property (
		GObject *queue, guint property_id, GValue *value,
		GParamSpec *pspec);

G_DEFINE_TYPE (TrackerProcessingQueue, tracker_processing_queue, G_TYPE_OBJECT);
#define TRACKER_PROCESSING_QUEUE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o),TRACKER_TYPE_PROCESSING_QUEUE, TrackerProcessingQueuePrivate))

static gpointer identity (gpointer key) {return key;}

static void
tracker_processing_queue_class_init (TrackerProcessingQueueClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = tracker_processing_queue_set_property;
	object_class->get_property = tracker_processing_queue_get_property;

	obj_properties[PROP_KEYING_FUNCTION] =
		g_param_spec_pointer ("keying-function",
		                      "Keying function",
		                      "Set keying function",
		                      G_PARAM_CONSTRUCT_ONLY |
		                      G_PARAM_READWRITE);

	obj_properties[PROP_LOOKUP_FUNCTION] =
		g_param_spec_pointer ("lookup-function",
		                      "Lookup function",
		                      "Set lookup function",
		                      G_PARAM_CONSTRUCT_ONLY |
		                      G_PARAM_READWRITE);

	g_type_class_add_private (object_class, sizeof (TrackerProcessingQueuePrivate));

	g_object_class_install_properties (object_class,
	                                   N_PROPERTIES,
	                                   obj_properties);
}

static void
tracker_processing_queue_init (TrackerProcessingQueue *self)
{
	self->priv              = TRACKER_PROCESSING_QUEUE_GET_PRIVATE (self);
	self->priv->elem_array  = g_ptr_array_new ();
	self->priv->elem_ht     = NULL;
	self->priv->hints       = g_queue_new();
	self->priv->key_func    = identity; /* This is really supposed to be set as a property */
	self->priv->lookup_func = NULL;
}

static void
tracker_processing_queue_set_property (GObject      *object,
                                       guint         property_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
	TrackerProcessingQueuePrivate *priv =
	      TRACKER_PROCESSING_QUEUE_GET_PRIVATE (object);
	switch (property_id) {
		case (PROP_KEYING_FUNCTION):
			priv->key_func = g_value_get_pointer (value);
			break;
		case (PROP_LOOKUP_FUNCTION):
			if (priv->elem_ht)
				g_free (priv->elem_ht);
			priv->lookup_func = g_value_get_pointer (value);
			priv->elem_ht =
			     g_hash_table_new (g_str_hash, priv->lookup_func);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object,
			                                   property_id,
			                                   pspec);
			break;
	}
}

static void
tracker_processing_queue_get_property (GObject      *object,
                                       guint         property_id,
                                       GValue       *value,
                                       GParamSpec   *pspec)
{}

void
tracker_processing_queue_add (TrackerProcessingQueue *queue,
                              gpointer                elem)
{
	struct ElemPtr   *eptr = g_malloc0 (sizeof (struct ElemPtr));
	       GPtrArray *dir  = NULL;
	       gpointer   key  = queue->priv->key_func (elem);
	eptr->ptr = elem;

	g_ptr_array_add (queue->priv->elem_array, eptr);
	dir = g_hash_table_lookup (queue->priv->elem_ht, key);
	if (dir){
		g_ptr_array_add (dir, eptr);
	} else {
		dir = g_ptr_array_new ();
		g_ptr_array_add (dir, eptr);
		g_hash_table_insert (queue->priv->elem_ht,
				     key,
				     dir);
	}
	queue->priv->next_random_idx =
                        g_random_int_range (0, queue->priv->elem_array->len);
}

gpointer
tracker_processing_queue_pop (TrackerProcessingQueue *queue)
{
	return tracker_processing_queue_get (queue, TRUE);
}

gpointer
tracker_processing_queue_peek (TrackerProcessingQueue *queue)
{
	return tracker_processing_queue_get (queue, FALSE);
}

void
tracker_processing_queue_prioritize (TrackerProcessingQueue *queue,
                                     gpointer                hint)
{
	g_assert (queue->priv->hints != NULL);
	g_queue_push_tail (queue->priv->hints, hint);
}

guint
tracker_processing_queue_get_length (TrackerProcessingQueue *queue)
{
	GHashTableIter iter;
	gpointer key, value;
	guint length = 0;

	g_hash_table_iter_init (&iter, queue->priv->elem_ht);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		GPtrArray *arr = value;
		length += arr->len;
	}

	return length;
}

guint
tracker_processing_queue_get_length_fast (TrackerProcessingQueue *queue)
{
	return queue->priv->elem_array->len;
}

TrackerProcessingQueue *
tracker_processing_queue_new ()
{
	return tracker_processing_queue_new_full (identity, g_str_equal);
}

TrackerProcessingQueue *
tracker_processing_queue_new_full (gpointer (*keying_func) (gpointer),
                                   GEqualFunc lookup_func)
{
	return g_object_new (TRACKER_TYPE_PROCESSING_QUEUE,
	                     "keying-function", keying_func,
	                     "lookup-function", lookup_func,
	                     NULL);
}

gboolean
tracker_processing_queue_contains (TrackerProcessingQueue *queue,
                                   gpointer                elem)
{
	gpointer   key = queue->priv->key_func (elem);
	GPtrArray *arr = g_hash_table_lookup (queue->priv->elem_ht, key);
	guint      i   = 0;

	/* Didn't find any elements with this key */
	if (!arr)
		return FALSE;

	/* Do a linear search through the array indicated by the lookup */
	for (i = 0; i < arr->len; i++) {
		struct ElemPtr *e = g_ptr_array_index (arr, i);
		if (e->ptr == elem)
			return TRUE;
	}

	return FALSE;
}

/* Privates */
static gpointer pop_random (TrackerProcessingQueue *queue, gboolean remove)
{
	struct ElemPtr *elem         = NULL;
	int *num_elems = &queue->priv->elem_array->len;

	if (*num_elems == 0)
		return NULL;

	/* Loop through elements until we find one which has
	 * not been removed */
	while (*num_elems > 0) {
		elem = g_ptr_array_index (queue->priv->elem_array,
					  queue->priv->next_random_idx);
		/* If the element is flagged as removed, pick a
		 * new one */
		if (elem->removed) {
			/* Remove the flagged element from the array */
			g_ptr_array_remove_index_fast (
			      queue->priv->elem_array,
			      queue->priv->next_random_idx);

			if (*num_elems <= 1)
				queue->priv->next_random_idx = 0;
			else {
				queue->priv->next_random_idx =
				      g_random_int_range (0,
					*num_elems-1);
			}
		} else {
			/* If element if not removed, continue
			 */
			break;
		}
	}

	g_assert (elem != NULL);

	/* This is just a call to peek */
	if (remove)
		g_ptr_array_remove_index_fast (queue->priv->elem_array,
		                               queue->priv->next_random_idx);
	else {
		return elem->ptr;
	}

	/* Also remove the element from the HT */
	/* TODO: Ensure we remove the exact correct element, and not just ONE
	 * of the elements with this key! */
	g_hash_table_remove (queue->priv->elem_ht,
			     queue->priv->key_func (elem->ptr));

	/* Set the index for the next pop */
	if (*num_elems <= 1) {
		queue->priv->next_random_idx = 0;
	}
	else {
		queue->priv->next_random_idx =
		      g_random_int_range (0, *num_elems-1);
	}

	return elem->ptr;
}

static gpointer pop_hinted (TrackerProcessingQueue *queue, gboolean remove)
{
	gpointer hint        = g_queue_peek_head (queue->priv->hints);
	GPtrArray *arr       = g_hash_table_lookup (queue->priv->elem_ht, hint);
	gpointer innerElem   = NULL;
	if (arr) {
		struct ElemPtr *elem = NULL;
		elem = g_ptr_array_remove_index_fast (arr, 0);
		innerElem = elem->ptr;

		/* Remove this element  */
		if (arr->len == 0)
			g_hash_table_remove (queue->priv->elem_ht, hint);
		elem->removed = TRUE;
		/* Check if this was the last element, if so, pop it from the
		 * hints queue also */
		if (!g_hash_table_contains (queue->priv->elem_ht, hint))
			g_queue_pop_head (queue->priv->hints);
	} else {
		/* If there is no hit in the HT, remove this hint */
		g_queue_pop_head (queue->priv->hints);
	}
	return innerElem;
}

/* Helper for peeking and popping. remove_element = FALSE means peek */
static gpointer
tracker_processing_queue_get (TrackerProcessingQueue *queue,
                              gboolean remove_element)
{
	gpointer retval = NULL;
	/* Check size of HT. Size of array may be inaccurate */
	if (g_hash_table_size (queue->priv->elem_ht) == 0)
		return NULL;

	/* If there are hints, try popping a hinted elem */
	if (g_queue_get_length (queue->priv->hints) > 0) {
		retval = pop_hinted (queue, remove_element);

		/* There was no hit for the hint, grab a random elem instead */
		if (!retval)
			retval = pop_random (queue, remove_element);
	}
	else {
		/* Hint queue is empty, pop a random element */
		retval = pop_random (queue, remove_element);
	}

	return retval;
}
