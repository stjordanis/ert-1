/*
   Copyright (C) 2011  Statoil ASA, Norway. 
    
   The file 'geo_surface.h' is part of ERT - Ensemble based Reservoir Tool. 
    
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


#ifndef __GEO_SURFACE_H__
#define __GEO_2DGEO_SURFACE_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct geo_surface_struct geo_surface_type;


void geo_surface_free( geo_surface_type * geo_surface );





#ifdef __cplusplus
}
#endif
#endif