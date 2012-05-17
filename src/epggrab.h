#ifndef __EPGGRAB_H__
#define __EPGGRAB_H__

#include <pthread.h>
#include "cron.h"

/* **************************************************************************
 * Type definitions
 * *************************************************************************/

/*
 * Grabber base class
 */
typedef struct epggrab_module
{
  const char* (*name)    ( void );
  void        (*enable)  ( void );
  void        (*disable) ( void );
  htsmsg_t*   (*grab)    ( const char *opts );
  int         (*parse)   ( htsmsg_t *data );
} epggrab_module_t;

/*
 * Schedule specification
 */
typedef struct epggrab_sched
{ 
  LIST_ENTRY(epggrab_sched) es_link;
  cron_t                    cron;  ///< Cron definition
  epggrab_module_t          *mod;  ///< Module
  char                      *opts; ///< Extra (advanced) options
} epggrab_sched_t;

/*
 * Schedule list
 */
LIST_HEAD(epggrab_sched_list, epggrab_sched);
typedef struct epggrab_sched_list epggrab_sched_list_t;

/* **************************************************************************
 * Variables
 * *************************************************************************/

/*
 * Lock this if accessing configuration
 */
pthread_mutex_t epggrab_mutex;

/* **************************************************************************
 * Functions
 * *************************************************************************/

/*
 * Setup
 */
void epggrab_init ( void );

/*
 * Access the list of supported modules
 */
//epggrab_module_t** epggrab_module_list         ( void );
epggrab_module_t*  epggrab_module_find_by_name ( const char *name );

/*
 * Set configuration
 */
void epggrab_set_simple   ( uint32_t interval, epggrab_module_t* mod );
void epggrab_set_advanced ( uint32_t count, epggrab_sched_t* sched );
void epggrab_set_eit      ( uint32_t eit );

#endif /* __EPGGRAB_H__ */
