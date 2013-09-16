/*
 * Copyright (C) 2011, Nokia <ivan.frade@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifndef __TRACKER_ENUMS_H__
#define __TRACKER_ENUMS_H__

G_BEGIN_DECLS

typedef enum {
	TRACKER_VERBOSITY_ERRORS,
	TRACKER_VERBOSITY_MINIMAL,
	TRACKER_VERBOSITY_DETAILED,
	TRACKER_VERBOSITY_DEBUG,
} TrackerVerbosity;

typedef enum {
	TRACKER_SCHED_IDLE_ALWAYS,
	TRACKER_SCHED_IDLE_FIRST_INDEX,
	TRACKER_SCHED_IDLE_NEVER,
} TrackerSchedIdle;

typedef enum {
	TRACKER_PROCESSING_QUEUE_RANDOM,
	TRACKER_PROCESSING_QUEUE_SEQUENTIAL
} TrackerProcessingQueueOrder;

G_END_DECLS

#endif /* __TRACKER_ENUMS_H__ */
