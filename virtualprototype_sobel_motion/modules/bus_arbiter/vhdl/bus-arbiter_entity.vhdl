--------------------------------------------------------------------------------
-- $RCSfile: bus-arbiter_entity.vhdl,v $
--
-- DESC    : OpenRisk 1300 single and multi-processor emulation platform on
--           the UMPP Xilinx FPA based hardware
--
-- EPFL    : LAP
--
-- AUTHORS : T.J.H. Kluter and C. Favi
--
-- CVS     : $Revision: 1.6 $
--           $Date: 2009/06/24 07:25:48 $
--           $Author: kluter $
--           $Source: /home/lapcvs/projects/or1300/modules/bus_arbiter/vhdl/bus-arbiter_entity.vhdl,v $
--
--------------------------------------------------------------------------------
--
-- Copyright (C) 2007/2008 Theo Kluter <ties.kluter@epfl.ch> EPFL-ISIM-LAP
-- Copyright (C) 2007/2008 Claudio Favi <claudio.favi@epfl.ch> EPFL-ISIM-GR-CH
--
--  This file is subject to the terms and conditions of the GNU General Public
--  License.
--
--------------------------------------------------------------------------------
--
--  HISTORY :
--
--  $Log: bus-arbiter_entity.vhdl,v $
--  Revision 1.6  2009/06/24 07:25:48  kluter
--  Added snoopable burst for profiling
--
--  Revision 1.5  2009/03/27 01:16:03  kluter
--  Fixed some bugs
--
--  Revision 1.4  2008/05/12 10:56:21  kluter
--  Added space for 16 more masters on the bus
--
--  Revision 1.3  2008/03/20 09:32:01  kluter
--  Transformed tri-stated bus to none-tristated one
--
--  Revision 1.2  2008/02/22 15:49:43  kluter
--  Added CVS header
--
--
--------------------------------------------------------------------------------

LIBRARY ieee;
USE ieee.std_logic_1164.all;
USE ieee.std_logic_arith.all;

ENTITY bus_arbiter IS
   PORT ( clock                  : IN  std_logic;
          reset                  : IN  std_logic;
          
          bus_requests           : IN  std_logic_vector(30 DOWNTO 0 );
          bus_grants             : OUT std_logic_vector(30 DOWNTO 0 );
          
          bus_error_out          : OUT std_logic;
          begin_transaction_in   : IN  std_logic;
          end_transaction_in     : IN  std_logic;
          address_data_in        : IN  std_logic_vector(31 DOWNTO 30);
          burst_size_in          : IN  std_logic_vector( 7 DOWNTO 0);
          end_transaction_out    : OUT std_logic;
          data_valid_in          : IN  std_logic;
          bus_idle               : OUT std_logic;
          snoopable_burst        : OUT std_logic );
END bus_arbiter;
