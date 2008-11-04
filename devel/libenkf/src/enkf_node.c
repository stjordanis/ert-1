#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <enkf_node.h>
#include <enkf_config_node.h>
#include <util.h>
#include <multz.h>
#include <relperm.h>
#include <multflt.h>
#include <equil.h>
#include <field.h>
#include <well.h>
#include <summary.h>
#include <ecl_static_kw.h>
#include <gen_kw.h>
#include <path_fmt.h>
#include <pilot_point.h>
#include <havana_fault.h>
#include <gen_data.h>
#include <enkf_serialize.h>

/**
   A small illustration (says more than thousand words ...) of how the
   enkf_node, enkf_config_node, field[1] and field_config[1] objects
   are linked.


     ================
     |              |   o-----------
     |  ================           |                =====================
     |  |              |   o--------                |                   |
     |	|  ================        |------------->  |                   |
     |	|  |		  |        |                |  enkf_config_node |
     |	|  |		  |        |                |                   |
     ===|  |  enkf_node   |  o------                |                   |
      o	|  |		  |                         |                   |
      |	===|		  |                         =====================
      |	 o |		  |                                   o
      |	 | ================                                   |
      |  |        o                                           |
      |  \        |					      |
      |   \       | 					      |
      |    |      |					      |
      |    |  	  | 					      |
      |    |  	  |  					      |
      |    |      |   					      |
     \|/   |	  |    					      |
     ======|======|==                       		     \|/
     |    \|/     | |   o-----------
     |  ==========|=====           |                =====================
     |  |        \|/   |   o--------                |                   |
     |	|  ================        |------------->  |                   |
     |	|  |		  |        |                |  field_config     |
     |	|  |		  |        |                |                   |
     ===|  |  field       |  o------                |                   |
       	|  |		  |                         |                   |
     	===|		  |                         =====================
     	   |		  |
     	   ================


   To summarize in words:

   * The enkf_node object is an abstract object, which again contains
     a spesific enkf_object, like e.g. the field objects shown
     here. In general we have an ensemble of enkf_node objects.

   * The enkf_node objects contain a pointer to a enkf_config_node
     object.

   * The enkf_config_node object contains a pointer to the spesific
     config object, i.e. field_config in this case.

   * All the field objects contain a pointer to a field_config object.


   [1]: field is just an example, and could be replaced with any of
        the enkf object types.
*/

/*-----------------------------------------------------------------*/

/**
   A note on memory 
   ================ 

   The enkf_nodes can consume large amounts of memory, and for large
   models/ensembles we have a situation where not all the
   members/fields can be in memory simultanouesly - such low-memory
   situations are not really supported at the moment, but we have
   implemented some support for such problems:

    o All enkf objects should have a xxx_realloc_data() function. This
      function should be implemented in such a way that it is always
      safe to call, i.e. if the object already has allocated data the
      function should just return.

    o All enkf objects should implement a xxx_free_data()
      function. This function free the data of the object, and set the
      data pointer to NULL.


   The following 'rules' apply to the memory treatment:
   ----------------------------------------------------

    o Functions writing to memory can always be called, and it is their
      responsibility to allocate memory before actually writing on it. The
      writer functions are:

        enkf_node_initialize()
        enkf_node_fread()
        enkf_node_ecl_load()  

      These functions should all start with a call to
      enkf_node_ensure_memory(). The (re)allocation of data is done at
      the enkf_node level, and **NOT** in the low level object
      (altough that is where it is eventually done of course).

    o When it comes to functions reading memory it is a bit more
      tricky. It could be that if the functions are called without
      memory, that just means that the object is not active or
      something (and the function should just return). On the other
      hand trying to read a NULL pointer does indicate that program
      logic is not fully up to it? And should therefor maybe be
      punished?

    o The only memory operation which is exported to 'user-space'
      (i.e. the enkf_state object) is enkf_node_free_data(). 

*/

/**
   Keeeping track of node state.
   =============================

   To keep track of the state of the node's data (actually the data of
   the contained enkf_object, i.e. a field) we have three higly
   internal variables __state, __modified and __report_step. These
   three variables are used/updated in the following manner:

    1. The nodes are created with (modified, report_step, state) ==
       (true , -1 , undefined).

    2. After initialization we set report_step -> 0 , state ->
       analyzed, modified -> true

    3. After load (both from ensemble and ECLIPSE). We set modified ->
       false, and report_step and state according to the load
       arguments.
      
    4. After deserialize (i.e. update) we set modified -> true.
      
    5. After write (to ensemble) we set in the same way as after load.
  
    6. After free_data we invalidate according to the newly allocated
       status.

    7. In the ens_load routine we check if modified == false and the
       report_step and state arguments agree with the current
       values. IN THAT CASE WE JUST RETURN WITHOUT ACTUALLY HITTING
       THE FILESYSTEM. This performance gain is the main point of the
       whole excercise.
*/











struct enkf_node_struct {
  alloc_ftype         *alloc;
  ecl_write_ftype     *ecl_write;
  ecl_load_ftype      *ecl_load;
  fread_ftype         *fread_f;
  fwrite_ftype        *fwrite_f;
  realloc_data_ftype  *realloc_data;
  free_data_ftype     *free_data;

  serialize_ftype     *serialize;
  deserialize_ftype   *deserialize;

  initialize_ftype   		 * initialize;
  free_ftype         		 * freef;
  clear_ftype        		 * clear;
  copyc_ftype        		 * copyc;
  scale_ftype        		 * scale;
  iadd_ftype         		 * iadd;
  imul_ftype         		 * imul;
  isqrt_ftype        		 * isqrt;
  iaddsqr_ftype      		 * iaddsqr;
  ensemble_fprintf_results_ftype * fprintf_results;
  iget_ftype                     * iget;               /* The two last are a pair - the very last is interactive and used first. */
  get_index_ftype                * get_index;
  
  
  /******************************************************************/
  char               *node_key;       	    /* The (hash)key this node is identified with. */
  void               *data;                 /* A pointer to the underlying enkf_object, i.e. multflt_type instance, or a field_type instance or ... */
  const enkf_config_node_type *config;      /* A pointer to a enkf_config_node instance (which again cointans a pointer to the config object of data). */
  
  serial_state_type  *serial_state;   	    /* A very internal object - containg information about the seralization of the this node .*/
  
  bool                __memory_allocated;   /* Whether the underlying object (the one pointed to by data) has memory allocated or not. */
  bool                __modified;           /* __modified, __report_step and __state are internal variables trying  */
  int                 __report_step;        /* to record the state of the in-memory reporesentation of the node->data. See */ 
  state_enum          __state;              /* the documentation with heading "Keeping track of node state". */
};





const enkf_config_node_type * enkf_node_get_config(const enkf_node_type * node) {
  return node->config;
}


/*
  const char     *  enkf_node_get_eclfile_ref(const enkf_node_type * node ) { return enkf_config_node_get_outfile_ref(node->config); }
*/





/*****************************************************************/

/*
  All the function pointers REALLY should be in the config object ...
*/


#define FUNC_ASSERT(func) \
   if (func == NULL) {      \
      fprintf(stderr,"%s: function handler: %s not registered for node:%s - aborting\n",__func__ , #func , enkf_node->node_key); \
      abort(); \
   }



static void enkf_node_ensure_memory(enkf_node_type * enkf_node) {
  FUNC_ASSERT(enkf_node->realloc_data);
  if (!enkf_node->__memory_allocated) {
    enkf_node->realloc_data(enkf_node->data);
    enkf_node->__memory_allocated = true;
  }
}



void enkf_node_alloc_domain_object(enkf_node_type * node) {
  if (node->data != NULL)
    node->freef(node->data);
  node->data = node->alloc(enkf_config_node_get_ref(node->config));
  node->__memory_allocated = true;
}




void enkf_node_clear_serial_state(enkf_node_type * node) {
  serial_state_clear(node->serial_state);
}



enkf_node_type * enkf_node_copyc(const enkf_node_type * src) {
  if (src->copyc == NULL) {
    printf("Har ikke copyc funksjon\n");
    abort();
  }
  {
    enkf_node_type * new;
    new = enkf_node_alloc(src->config);

    printf("%s: not properly implemented ... \n",__func__);
    abort();
    return new;
  }
}



bool enkf_node_include_type(const enkf_node_type * enkf_node, int mask) {
  return enkf_config_node_include_type(enkf_node->config , mask);
}


enkf_impl_type enkf_node_get_impl_type(const enkf_node_type * enkf_node) {
  return enkf_config_node_get_impl_type(enkf_node->config);
}


enkf_var_type enkf_node_get_var_type(const enkf_node_type * enkf_node) {
  return enkf_config_node_get_var_type(enkf_node->config);
}





void * enkf_node_value_ptr(const enkf_node_type * enkf_node) {
  return enkf_node->data;
}



/**
   This function calls the node spesific ecl_write function. IF the
   ecl_file of the (node == NULL) *ONLY* the path is sent to the node
   spesific file.

   This means that it is the responsibility of the node code to know
   wether a full file name is required, or only a path.
*/

void enkf_node_ecl_write(const enkf_node_type *enkf_node , const char *path , fortio_type * restart_fortio) {
  if (enkf_node->ecl_write != NULL) {
    const char * node_eclfile = enkf_config_node_get_outfile_ref(enkf_node->config);
    if (node_eclfile != NULL) {
      char * target_file = util_alloc_full_path(path , node_eclfile);
      enkf_node->ecl_write(enkf_node->data , target_file , restart_fortio);
      free(target_file);
    } else  
      enkf_node->ecl_write(enkf_node->data , path , restart_fortio);
    /*
      This code path is followed by:
      
      1. Restart-file elements which use the fortio pointer
      2. GEN_DATA instances which allocate the filename interanally
      	 (without useing the enkf_node layer). That implementation is
      	 to flexible for it's own good....
    */
  }
}



/**
   This function loads (internalizes) ECLIPSE results, the ecl_block
   instance with restart data, and the ecl_sum instance with summary
   data must already be loaded by the calling function.

   IFF the enkf_node has registered a filename to load from, that is
   passed to the specifi load function, otherwise the run_path is sent
   to the load function.

   If the node does not have a ecl_load function, the function just
   returns.
*/


void enkf_node_ecl_load(enkf_node_type *enkf_node , const char * run_path , const ecl_sum_type * ecl_sum, const ecl_block_type * restart_block , int report_step) {
  FUNC_ASSERT(enkf_node->ecl_load);
  enkf_node_ensure_memory(enkf_node);
  {
    const char * input_file = enkf_config_node_get_infile(enkf_node->config);
    char * file = NULL;
    if (input_file != NULL) {
      file = util_alloc_full_path( run_path , input_file);
      enkf_node->ecl_load(enkf_node->data , file  , ecl_sum , restart_block , report_step);
      free(file);
    } else
      enkf_node->ecl_load(enkf_node->data , run_path , ecl_sum , restart_block , report_step);
  }
  enkf_node->__report_step = report_step;
  enkf_node->__state       = forecast;
  enkf_node->__modified    = false;
}



void enkf_node_ecl_load_static(enkf_node_type * enkf_node , const ecl_kw_type * ecl_kw, int report_step) {
  ecl_static_kw_init(enkf_node_value_ptr(enkf_node) , ecl_kw);
  enkf_node->__memory_allocated = true;
  enkf_node->__report_step = report_step;
  enkf_node->__state       = forecast;
  enkf_node->__modified    = false;
}



/**
   Returns true if data is written to disk. If no data is written to
   disk, and the function returns false, the calling scope is free
   to skip storage (i.e. unlink an empty file).
*/

bool enkf_node_fwrite(enkf_node_type *enkf_node , FILE *stream , int report_step , state_enum state) {
  if (!enkf_node->__memory_allocated)
    util_abort("%s: fatal internal error: tried to save node:%s - memory is not allocated - aborting.\n",__func__ , enkf_node->node_key);
  {
    FUNC_ASSERT(enkf_node->fwrite_f);
    bool data_written = enkf_node->fwrite_f(enkf_node->data , stream);

    enkf_node->__report_step = report_step;
    enkf_node->__state       = state;
    enkf_node->__modified    = false;

    return data_written;
  }
}



void enkf_node_ensemble_fprintf_results(const enkf_node_type ** ensemble , int ens_size , int report_step , const char * path) {
  /*
    FUNC_ASSERT(ensemble[0]->fprintf_results);
  */
  {
    void ** data_pointers = util_malloc(ens_size * sizeof * data_pointers , __func__);
    char * filename       = util_alloc_full_path(path , ensemble[0]->node_key);
    int iens;
    for (iens=0; iens < ens_size; iens++)
      data_pointers[iens] = ensemble[iens]->data;

    ensemble[0]->fprintf_results((const void **) data_pointers , ens_size , filename);
    free(filename);
    free(data_pointers);
  }
}


bool enkf_node___memory_allocated(const enkf_node_type * node) { return node->__memory_allocated; }

static void enkf_node_assert_memory(const enkf_node_type * enkf_node , const char * caller) {
  if (!enkf_node___memory_allocated(enkf_node)) {
    printf("Fatal error - no memory ?? \n");
    util_abort("%s:  tried to call:%s without allocated memory for node:%s - internal ERROR - aborting.\n",__func__ , caller , enkf_node->node_key);
  }
}


void enkf_node_fread(enkf_node_type *enkf_node , FILE * stream , int report_step , state_enum state) {
  if ((report_step == enkf_node->__report_step) && (state == enkf_node->__state) && !enkf_node->__modified)
    return;  /* The in memory representation agrees with the disk image */
  {
    FUNC_ASSERT(enkf_node->fread_f);
    enkf_node_ensure_memory(enkf_node);
    enkf_node->fread_f(enkf_node->data , stream);
    enkf_node->__modified    = false;
    enkf_node->__report_step = report_step;
    enkf_node->__state       = state;
  }
}



void enkf_node_ens_clear(enkf_node_type *enkf_node) {
  FUNC_ASSERT(enkf_node->clear);
  enkf_node->clear(enkf_node->data);
}


int enkf_node_serialize(enkf_node_type *enkf_node , size_t current_serial_offset ,serial_vector_type * serial_vector, bool * complete) {
  FUNC_ASSERT(enkf_node->serialize);
  enkf_node_assert_memory(enkf_node , __func__);
  { 
    int elements_added = 0;
    elements_added = enkf_node->serialize(enkf_node->data , enkf_node->serial_state , current_serial_offset , serial_vector );
    *complete = serial_state_complete( enkf_node->serial_state );
    return elements_added;
  } 
}



void enkf_node_deserialize(enkf_node_type *enkf_node , const serial_vector_type * serial_vector) {
  FUNC_ASSERT(enkf_node->serialize);
  enkf_node->deserialize(enkf_node->data , enkf_node->serial_state , serial_vector);
  enkf_node->__modified = true;
}


void enkf_node_sqrt(enkf_node_type *enkf_node) {
  FUNC_ASSERT(enkf_node->isqrt);
  enkf_node->isqrt(enkf_node->data);
}

void enkf_node_scale(enkf_node_type *enkf_node , double scale_factor) {
  FUNC_ASSERT(enkf_node->scale);
  enkf_node->scale(enkf_node->data , scale_factor);
}

void enkf_node_iadd(enkf_node_type *enkf_node , const enkf_node_type * delta_node) {
  FUNC_ASSERT(enkf_node->iadd);
  enkf_node->iadd(enkf_node->data , delta_node->data);
}

void enkf_node_iaddsqr(enkf_node_type *enkf_node , const enkf_node_type * delta_node) {
  FUNC_ASSERT(enkf_node->iaddsqr);
  enkf_node->iaddsqr(enkf_node->data , delta_node->data);
}

void enkf_node_imul(enkf_node_type *enkf_node , const enkf_node_type * delta_node) {
  FUNC_ASSERT(enkf_node->imul);
  enkf_node->imul(enkf_node->data , delta_node->data);
}


void enkf_node_initialize(enkf_node_type *enkf_node, int iens) {
  if (enkf_node->initialize != NULL) {
    enkf_node_ensure_memory(enkf_node);
    enkf_node->initialize(enkf_node->data , iens);
    enkf_node->__report_step = 0;
    enkf_node->__state       = analyzed;
    enkf_node->__modified    = true;
  }
}


void enkf_node_free_data(enkf_node_type * enkf_node) {
  FUNC_ASSERT(enkf_node->free_data);
  enkf_node->free_data(enkf_node->data);
  enkf_node->__memory_allocated = false;
  enkf_node->__report_step      = -1;
  enkf_node->__state            = undefined;
  enkf_node->__modified         = true;
}



void enkf_node_clear(enkf_node_type *enkf_node) {
  FUNC_ASSERT(enkf_node->clear);
  enkf_node->clear(enkf_node->data);
}


void enkf_node_printf(const enkf_node_type *enkf_node) {
  printf("%s \n",enkf_node->node_key);
}

/*
  char * enkf_node_alloc_ensfile(const enkf_node_type *enkf_node , const char * path) {
  FUNC_ASSERT(enkf_node , "alloc_ensfile");
  return enkf_node->alloc_ensfile(enkf_node->data , path);
}
*/

void enkf_node_free(enkf_node_type *enkf_node) {
  if (enkf_node->freef != NULL)
    enkf_node->freef(enkf_node->data);
  free(enkf_node->node_key);
  serial_state_free(enkf_node->serial_state);
  free(enkf_node);
}


void enkf_node_free__(void *void_node) {
  enkf_node_free((enkf_node_type *) void_node);
}

const char *enkf_node_get_key(const enkf_node_type * enkf_node) {
  return enkf_node->node_key;
}


#undef FUNC_ASSERT



/*****************************************************************/


/* Manual inheritance - .... */
static enkf_node_type * enkf_node_alloc_empty(const enkf_config_node_type *config) {
  const char *node_key     = enkf_config_node_get_key_ref(config);
  enkf_impl_type impl_type = enkf_config_node_get_impl_type(config);
  enkf_node_type * node    = util_malloc(sizeof * node , __func__);
  node->config             = config;
  node->node_key           = util_alloc_string_copy(node_key);
  node->data               = NULL;
  node->__memory_allocated = false;
  node->__modified         = true;
  node->__report_step      = -1;
  node->__state            = undefined;

  /*
     Start by initializing all function pointers
     to NULL.
  */
  node->realloc_data   = NULL;
  node->alloc          = NULL;
  node->ecl_write      = NULL;
  node->ecl_load       = NULL;
  node->fread_f        = NULL;
  node->fwrite_f       = NULL;
  node->copyc          = NULL;
  node->initialize     = NULL;
  node->serialize      = NULL;
  node->deserialize    = NULL;
  node->freef          = NULL;
  node->free_data      = NULL;
  node->fprintf_results= NULL;
  node->iget           = NULL;
  node->get_index      = NULL;

  switch (impl_type) {
  case(GEN_KW):
    node->realloc_data 	  = gen_kw_realloc_data__;
    node->alloc        	  = gen_kw_alloc__;
    node->ecl_write    	  = gen_kw_ecl_write__;
    node->fread_f      	  = gen_kw_fread__;
    node->fwrite_f     	  = gen_kw_fwrite__;
    node->copyc        	  = gen_kw_copyc__;
    node->initialize   	  = gen_kw_initialize__;
    node->serialize    	  = gen_kw_serialize__;
    node->deserialize  	  = gen_kw_deserialize__;
    node->freef        	  = gen_kw_free__;
    node->free_data    	  = gen_kw_free_data__;
    node->fprintf_results = gen_kw_ensemble_fprintf_results__;
    break;
  case(MULTZ):
    node->realloc_data = multz_realloc_data__;
    node->alloc        = multz_alloc__;
    node->ecl_write    = multz_ecl_write__;
    node->fread_f      = multz_fread__;
    node->fwrite_f     = multz_fwrite__;
    node->copyc        = multz_copyc__;
    node->initialize   = multz_initialize__;
    node->serialize    = multz_serialize__;
    node->deserialize  = multz_deserialize__;
    node->freef        = multz_free__;
    node->free_data    = multz_free_data__;
    break;
  case(RELPERM):
    node->alloc       = relperm_alloc__;
    node->ecl_write   = relperm_ecl_write__;
    node->fread_f     = relperm_fread__;
    node->fwrite_f    = relperm_fwrite__;
    node->copyc       = relperm_copyc__;
    node->initialize  = relperm_initialize__;
    node->serialize   = relperm_serialize__;
    node->deserialize = relperm_deserialize__;
    node->freef       = relperm_free__;
    node->free_data   = relperm_free_data__;
    break;
  case(MULTFLT):
    node->realloc_data 	  = multflt_realloc_data__;
    node->alloc        	  = multflt_alloc__;
    node->ecl_write    	  = multflt_ecl_write__;
    node->fread_f      	  = multflt_fread__;
    node->fwrite_f     	  = multflt_fwrite__;
    node->copyc        	  = multflt_copyc__;
    node->initialize   	  = multflt_initialize__;
    node->serialize    	  = multflt_serialize__;
    node->deserialize  	  = multflt_deserialize__;
    node->freef        	  = multflt_free__;
    node->free_data    	  = multflt_free_data__;
    node->fprintf_results = multflt_ensemble_fprintf_results__;
    break;
  case(WELL):
    node->ecl_load        = well_ecl_load__;
    node->realloc_data 	  = well_realloc_data__;
    node->alloc        	  = well_alloc__;
    node->fread_f      	  = well_fread__;
    node->fwrite_f     	  = well_fwrite__;
    node->copyc        	  = well_copyc__;
    node->serialize    	  = well_serialize__;
    node->deserialize  	  = well_deserialize__;
    node->freef        	  = well_free__;
    node->free_data    	  = well_free_data__;
    node->fprintf_results = well_ensemble_fprintf_results__;
    break;
  case(SUMMARY):
    node->ecl_load     = summary_ecl_load__;
    node->realloc_data = summary_realloc_data__;
    node->alloc        = summary_alloc__;
    node->fread_f      = summary_fread__;
    node->fwrite_f     = summary_fwrite__;
    node->copyc        = summary_copyc__;
    node->serialize    = summary_serialize__;
    node->deserialize  = summary_deserialize__;
    node->freef        = summary_free__;
    node->free_data    = summary_free_data__;
    break;
  case(HAVANA_FAULT):
    node->realloc_data 	  = havana_fault_realloc_data__;
    node->alloc        	  = havana_fault_alloc__;
    node->ecl_write    	  = havana_fault_ecl_write__;
    node->fread_f      	  = havana_fault_fread__;
    node->fwrite_f     	  = havana_fault_fwrite__;
    node->copyc        	  = havana_fault_copyc__;
    node->serialize    	  = havana_fault_serialize__;
    node->deserialize  	  = havana_fault_deserialize__;
    node->freef        	  = havana_fault_free__;
    node->free_data    	  = havana_fault_free_data__;
    node->initialize   	  = havana_fault_initialize__;
    node->fprintf_results = havana_fault_ensemble_fprintf_results__;
    break;
  case(FIELD):
    node->realloc_data = field_realloc_data__;
    node->alloc        = field_alloc__;
    node->ecl_write    = field_ecl_write__; 
    node->ecl_load     = field_ecl_load__;  
    node->fread_f      = field_fread__;
    node->fwrite_f     = field_fwrite__;
    node->copyc        = field_copyc__;
    node->initialize   = field_initialize__;
    node->serialize    = field_serialize__;
    node->deserialize  = field_deserialize__;
    node->freef        = field_free__;
    node->free_data    = field_free_data__;
    node->iget         = field_iget__;
    break;
  case(EQUIL):
    node->alloc       = equil_alloc__;
    node->ecl_write   = equil_ecl_write__;
    node->fread_f     = equil_fread__;
    node->fwrite_f    = equil_fwrite__;
    node->copyc       = equil_copyc__;
    node->initialize  = equil_initialize__;
    node->serialize   = equil_serialize__;
    node->deserialize = equil_deserialize__;
    node->freef       = equil_free__;
    node->free_data   = equil_free_data__;
    break;
  case(STATIC):
    node->realloc_data = ecl_static_kw_realloc_data__;
    node->ecl_write    = ecl_static_kw_ecl_write__; 
    node->alloc        = ecl_static_kw_alloc__;
    node->fread_f      = ecl_static_kw_fread__;
    node->fwrite_f     = ecl_static_kw_fwrite__;
    node->copyc        = ecl_static_kw_copyc__;
    node->freef        = ecl_static_kw_free__;
    node->free_data    = ecl_static_kw_free_data__;
    break;
  case(GEN_DATA):
    node->realloc_data = gen_param_realloc_data__;
    node->alloc        = gen_param_alloc__;
    node->fread_f      = gen_param_fread__;
    node->fwrite_f     = gen_param_fwrite__;
    node->initialize   = gen_param_initialize__;
    node->copyc        = gen_param_copyc__;
    node->freef        = gen_param_free__;
    node->free_data    = gen_param_free_data__;
    node->ecl_write    = gen_param_ecl_write__;
    node->serialize    = gen_param_serialize__;
    node->deserialize  = gen_param_deserialize__;
    break;
  default:
    util_abort("%s: implementation type: %d unknown - all hell is loose - aborting \n",__func__ , impl_type);
  }
  node->serial_state = serial_state_alloc();
  return node;
}



#define CASE_SET(type , func) case(type): has_func = (func != NULL); break;
bool enkf_node_has_func(const enkf_node_type * node , node_function_type function_type) {
  bool has_func = false;
  switch (function_type) {
    CASE_SET(alloc_func        		    , node->alloc);
    CASE_SET(ecl_write_func    		    , node->ecl_write);
    CASE_SET(ecl_load_func                  , node->ecl_load);
    CASE_SET(fread_func        		    , node->fread_f);
    CASE_SET(fwrite_func       		    , node->fwrite_f);
    CASE_SET(copyc_func        		    , node->copyc);
    CASE_SET(initialize_func   		    , node->initialize);
    CASE_SET(serialize_func    		    , node->serialize);
    CASE_SET(deserialize_func  		    , node->deserialize);
    CASE_SET(free_func         		    , node->freef);
    CASE_SET(free_data_func    		    , node->free_data);
    CASE_SET(ensemble_fprintf_results_func  , node->fprintf_results);
  default:
    fprintf(stderr,"%s: node_function_identifier: %d not recognized - aborting \n",__func__ , function_type);
  }
  return has_func;
}
#undef CASE_SET


static enkf_node_type * enkf_node_alloc__(const enkf_config_node_type * config) {
  enkf_node_type * node    = enkf_node_alloc_empty(config);
  enkf_node_alloc_domain_object(node);
  return node;
}


enkf_node_type * enkf_node_alloc(const enkf_config_node_type * config) {
  if (config == NULL)
    util_abort("%s: internal error - must use enkf_node_alloc_static() to allocate static nodes.\n",__func__);

  return enkf_node_alloc__(config);
}




