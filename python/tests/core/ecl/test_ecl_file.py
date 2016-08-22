#!/usr/bin/env python
#  Copyright (C) 2011  Statoil ASA, Norway. 
#   
#  The file 'sum_test.py' is part of ERT - Ensemble based Reservoir Tool. 
#   
#  ERT is free software: you can redistribute it and/or modify 
#  it under the terms of the GNU General Public License as published by 
#  the Free Software Foundation, either version 3 of the License, or 
#  (at your option) any later version. 
#   
#  ERT is distributed in the hope that it will be useful, but WITHOUT ANY 
#  WARRANTY; without even the implied warranty of MERCHANTABILITY or 
#  FITNESS FOR A PARTICULAR PURPOSE.   
#   
#  See the GNU General Public License at <http://www.gnu.org/licenses/gpl.html> 
#  for more details.
import datetime
import os.path
from unittest import skipIf

from ert.ecl import EclFile, FortIO, EclKW , openFortIO , openEclFile
from ert.ecl import EclFileFlagEnum, EclTypeEnum, EclFileEnum

from ert.test import ExtendedTestCase , TestAreaContext


    

class EclFileTest(ExtendedTestCase):

    def assertFileType(self , filename , expected):
        file_type , step , fmt_file = EclFile.getFileType(filename)
        self.assertEqual( file_type , expected[0] )
        self.assertEqual( fmt_file , expected[1] )
        self.assertEqual( step , expected[2] )

        
    def test_file_type(self):
        self.assertFileType( "ECLIPSE.UNRST" , (EclFileEnum.ECL_UNIFIED_RESTART_FILE , False , None))
        self.assertFileType( "ECLIPSE.X0030" , (EclFileEnum.ECL_RESTART_FILE , False , 30 ))
        self.assertFileType( "ECLIPSE.DATA"  , (EclFileEnum.ECL_DATA_FILE , None , None ))
        self.assertFileType( "ECLIPSE.FINIT" , (EclFileEnum.ECL_INIT_FILE , True , None ))
        self.assertFileType( "ECLIPSE.A0010" , (EclFileEnum.ECL_SUMMARY_FILE , True , 10 ))
        self.assertFileType( "ECLIPSE.EGRID" , (EclFileEnum.ECL_EGRID_FILE , False  , None ))

        
    def test_IOError(self):
        with self.assertRaises(IOError):
            EclFile("No/Does/not/exist")


    def test_context( self ):
        with TestAreaContext("python/ecl_file/context"):
            kw1 = EclKW.create( "KW1" , 100 , EclTypeEnum.ECL_INT_TYPE)
            kw2 = EclKW.create( "KW2" , 100 , EclTypeEnum.ECL_INT_TYPE)
            with openFortIO("TEST" , mode = FortIO.WRITE_MODE) as f:
                kw1.fwrite( f )
                kw2.fwrite( f )

            with openEclFile("TEST") as ecl_file:
                self.assertEqual( len(ecl_file) , 2 )
                self.assertTrue( ecl_file.has_kw("KW1"))
                self.assertTrue( ecl_file.has_kw("KW2"))

        
                
                
        
            
