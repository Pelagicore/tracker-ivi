#include "glib.h"
#include "glib-object.h"


G_BEGIN_DECLS

#define TRACKER_TYPE_PROCESSING_QUEUE         (tracker_processing_queue_get_type())
#define TRACKER_PROCESSING_QUEUE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_PROCESSING_QUEUE, TrackerProcessingQueue))
#define TRACKER_PROCESSING_QUEUE_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    TRACKER_TYPE_PROCESSING_QUEUE, TrackerProcessingQueueClass))
#define TRACKER_IS_PROCESSING_QUEUE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_PROCESSING_QUEUE))
#define TRACKER_IS_PROCESSING_QUEUE_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),    TRACKER_TYPE_PROCESSING_QUEUE))
#define TRACKER_PROCESSING_QUEUE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  TRACKER_TYPE_PROCESSING_QUEUE, TrackerProcessingQueueClass))

typedef struct _TrackerProcessingQueue TrackerProcessingQueue;
typedef struct _TrackerProcessingQueuePrivate TrackerProcessingQueuePrivate;
typedef struct _TrackerProcessingQueueCriteria TrackerProcessingQueueCriteria;

struct _TrackerProcessingQueue {
	GObject parent_instance;
	TrackerProcessingQueuePrivate *priv;
};

typedef struct {
	GObjectClass parent_class;
} 
TrackerProcessingQueueClass;

struct _TrackerProcessingQueueCriteria
{
	guint  queue_position;
	guint  seg_position;
	guint  queue_length;
	GList *node;
};

GType                   tracker_processing_queue_get_type        (void) G_GNUC_CONST;

TrackerProcessingQueue *tracker_processing_queue_new             (void);
TrackerProcessingQueue *tracker_processing_queue_new_full        (gpointer (*keying_func) (gpointer),
                                                                  GEqualFunc lookup_func);
gpointer                tracker_processing_queue_pop             (TrackerProcessingQueue *queue);
gpointer                tracker_processing_queue_peek            (TrackerProcessingQueue *queue);
void                    tracker_processing_queue_add             (TrackerProcessingQueue *queue,
                                                                  gpointer                elem);
void                    tracker_processing_queue_prioritize      (TrackerProcessingQueue *queue,
                                                                  gpointer                hint);
guint                   tracker_processing_queue_get_length      (TrackerProcessingQueue *queue);
guint                   tracker_processing_queue_get_length_fast (TrackerProcessingQueue *queue);
gboolean                tracker_processing_queue_contains        (TrackerProcessingQueue *queue,
                                                                  gpointer                elem);
gboolean                tracker_processing_queue_foreach_remove (TrackerProcessingQueue *queue,
                                                                 GEqualFunc              compare_func,
                                                                 gpointer                compare_user_data,
                                                                 GDestroyNotify          destroy_notify);

G_END_DECLS
