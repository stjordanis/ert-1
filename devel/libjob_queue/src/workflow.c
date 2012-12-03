/*
   Copyright (C) 2012  Statoil ASA, Norway. 
    
   The file 'workflow.c' is part of ERT - Ensemble based Reservoir Tool. 
    
   ERT is free software: you can redistribute it and/or modify 
   it under the terms of the GNU General Public License as published by 
   the Free Software Foundation, either version 3 of the License, or 
   (at your option) any later version. 
    
   ERT is distributed in the hope that it will be useful, but WITHOUT ANY 
   WARRANTY; without even the implied warranty of MERCHANTABILITY or 
   FITNESS FOR A PARTICULAR PURPOSE.   
    
   See the GNU General Public License at <http://www.gnu.org/licenses/gpl.html> 
   for more details. 
*/

#include <stdbool.h>
#include <stdlib.h>
#include <dlfcn.h>

#include <int_vector.h>
#include <util.h>
#include <type_macros.h>
#include <stringlist.h>
#include <arg_pack.h>
#include <vector.h>

#include <config.h>

#include <workflow.h>
#include <workflow_job.h>
#include <workflow_joblist.h>

#define CMD_TYPE_ID                66153
#define WORKFLOW_TYPE_ID         6762081
#define WORKFLOW_COMMENT_STRING     "--"
#define WORKFLOW_INCLUDE       "INCLUDE"



typedef struct cmd_struct {
  UTIL_TYPE_ID_DECLARATION;
  const workflow_job_type * workflow_job;
  stringlist_type         * arglist;
} cmd_type;





struct workflow_struct {
  UTIL_TYPE_ID_DECLARATION;
  time_t                 compile_time;
  bool                   compiled;
  char                  * src_file; 
  vector_type           * cmd_list;
  workflow_joblist_type * joblist;
};

/*****************************************************************/


static cmd_type * cmd_alloc( const workflow_job_type * workflow_job , const stringlist_type * arglist) {
  cmd_type * cmd = util_malloc( sizeof * cmd );
  UTIL_TYPE_ID_INIT(cmd , CMD_TYPE_ID );
  cmd->workflow_job = workflow_job;
  cmd->arglist = stringlist_alloc_deep_copy( arglist );
  return cmd;
}

static UTIL_SAFE_CAST_FUNCTION( cmd , CMD_TYPE_ID );

static void cmd_free( cmd_type * cmd ){ 
  stringlist_free( cmd->arglist );
  free( cmd );
}

static void cmd_free__( void * arg ) {
  cmd_type * cmd = cmd_safe_cast( arg );
  cmd_free( cmd );
}

/*****************************************************************/

static void workflow_add_cmd( workflow_type * workflow , cmd_type * cmd ) {
  vector_append_owned_ref( workflow->cmd_list , cmd , cmd_free__ );
}


static bool workflow_try_compile( workflow_type * script ) {
  if (util_file_exists( script->src_file )) {
    time_t src_mtime = util_file_mtime( script->src_file );
    if (script->compiled) {
      if (util_difftime_seconds( src_mtime , script->compile_time ) > 0 ) 
        return true;
      else {
        // Script has been compiled succesfully, but then changed afterwards. 
        // We tro to recompile; if that fails we are left with 'nothing'.
      }
    }
    
    {
      // Try to compile
      config_type * config_compiler = workflow_joblist_get_compiler( script->joblist );
      script->compiled = false;
      vector_clear( script->cmd_list );
      config_clear( config_compiler );
      {
        if (config_parse( config_compiler , script->src_file , WORKFLOW_COMMENT_STRING , WORKFLOW_INCLUDE , NULL , CONFIG_UNRECOGNIZED_WARN , true )) {
          int cmd_line;
          for (cmd_line = 0; cmd_line < config_get_content_size(config_compiler); cmd_line++) {
            const config_content_node_type * node = config_iget_content_node( config_compiler , cmd_line );
            const char * jobname = config_content_node_get_kw( node );
            const workflow_job_type * job = workflow_joblist_get_job( script->joblist , jobname );
            cmd_type * cmd = cmd_alloc( job , config_content_node_get_stringlist( node ));
            
            workflow_add_cmd( script , cmd );
          }
          return true;
        } else
          return false;
      }
    }
  } else
    return script->compiled;  // It is legal to remove the script after 
                              // successfull compilation.
}


bool workflow_run( const workflow_type * workflow , void * self ) {
  return false;
}



workflow_type * workflow_alloc( const char * src_file , workflow_joblist_type * joblist) {
  workflow_type * script = util_malloc( sizeof * script );
  UTIL_TYPE_ID_INIT( script , WORKFLOW_TYPE_ID );

  script->src_file        = util_alloc_string_copy( src_file );
  script->joblist         = joblist;
  script->cmd_list        = vector_alloc_new();
  script->compiled        = false;
  
  workflow_try_compile( script );

  return script;
}
