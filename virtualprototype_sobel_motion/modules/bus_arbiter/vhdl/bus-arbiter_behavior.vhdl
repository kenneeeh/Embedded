--------------------------------------------------------------------------------
-- $RCSfile: bus-arbiter_behavior_xilinx.vhdl,v $
--
-- DESC    : OpenRisk 1300 single and multi-processor emulation platform on
--           the UMPP Xilinx FPA based hardware
--
-- EPFL    : LAP
--
-- AUTHORS : T.J.H. Kluter and C. Favi
--
-- CVS     : $Revision: 1.7 $
--           $Date: 2009/06/24 07:25:48 $
--           $Author: kluter $
--           $Source: /home/lapcvs/projects/or1300/modules/bus_arbiter/vhdl/bus-arbiter_behavior_xilinx.vhdl,v $
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
--  $Log: bus-arbiter_behavior_xilinx.vhdl,v $
--  Revision 1.7  2009/06/24 07:25:48  kluter
--  Added snoopable burst for profiling
--
--  Revision 1.6  2009/03/27 01:16:03  kluter
--  Fixed some bugs
--
--  Revision 1.5  2009/02/15 16:22:14  kluter
--  Added memory to processor distance emulation to sdram controller. Only
--  CPU 1 can control this value, the other CPUs can only read the value.
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

ARCHITECTURE noPlatformSpecific OF bus_arbiter IS

   COMPONENT queueMemory IS
   PORT ( writeClock   : IN  std_logic;
          writeEnable  : IN  std_logic;
          writeAddress : IN  std_logic_vector( 4 DOWNTO 0 );
          readAddress  : IN  std_logic_vector( 4 DOWNTO 0 );
          writeData    : IN  std_logic_vector( 31 DOWNTO 0 );
          dataReadPort : OUT std_logic_vector( 31 DOWNTO 0 ) );
   END COMPONENT;
   
   TYPE ARBITER_TYPE IS (IDLE , GRANT , WAIT_BEGIN , SERVICING , BUS_ERROR , END_TRANSACTION ,REMOVE);
   
   CONSTANT c_zero_vector            : std_logic_vector( 30 DOWNTO 0 ) := (OTHERS => '0');
   
   SIGNAL s_queued_mask_reg          : std_logic_vector(30 DOWNTO 0);
   SIGNAL s_queued_mask_next         : std_logic_vector(30 DOWNTO 0);
   SIGNAL s_masked_requests          : std_logic_vector(30 DOWNTO 0);
   SIGNAL s_to_be_queued_mask        : std_logic_vector(31 DOWNTO 0);
   SIGNAL s_insert_into_queue        : std_logic;
   SIGNAL s_queue_insert_pointer_reg : std_logic_vector( 4 DOWNTO 0);
   SIGNAL s_queue_remove_pointer_reg : std_logic_vector( 4 DOWNTO 0);
   SIGNAL s_schedule_mask            : std_logic_vector(31 DOWNTO 0);
   SIGNAL s_service_mask             : std_logic_vector(30 DOWNTO 0);
   SIGNAL s_take_request             : std_logic;
   SIGNAL s_request_taken_reg        : std_logic_vector(30 DOWNTO 0);
   SIGNAL s_queue_is_empty           : std_logic;
   SIGNAL s_arbiter_state_reg        : ARBITER_TYPE;
   SIGNAL s_bus_time_out_count_reg   : std_logic_vector(11 DOWNTO 0);
   SIGNAL s_masked_request_taken     : std_logic_vector(30 DOWNTO 0);
   SIGNAL s_bus_error_reg            : std_logic;
   SIGNAL s_end_transaction_reg      : std_logic;
   SIGNAL s_retry_count_reg          : std_logic_vector( 2 DOWNTO 0);
   SIGNAL s_is_snoopable_burst       : std_logic;
   
BEGIN

-- Assign outputs
   bus_error_out         <= s_bus_error_reg;
   end_transaction_out   <= s_end_transaction_reg;
   make_bus_grants : PROCESS( clock , reset , s_masked_request_taken , s_arbiter_state_reg )
   BEGIN
      IF (reset = '1') THEN bus_grants <= (OTHERS => '0');
      ELSIF (clock'event AND (clock = '1')) THEN
         IF (s_arbiter_state_reg = GRANT) THEN bus_grants <= s_masked_request_taken;
                                          ELSE bus_grants <= (OTHERS => '0');
         END IF;
      END IF;
   END PROCESS make_bus_grants;
   
   make_bus_idle : PROCESS( clock , reset , s_arbiter_state_reg , s_queue_is_empty )
   BEGIN
      IF (reset = '1') THEN bus_idle <= '1';
      ELSIF (clock'event AND (clock = '1')) THEN
         IF (s_arbiter_state_reg = IDLE AND
             s_queue_is_empty = '1') THEN bus_idle <= '1';
                                     ELSE bus_idle <= '0';
         END IF;
      END IF;
   END PROCESS make_bus_idle;
   
   make_snoopable_burst : PROCESS( clock )
   BEGIN
      IF (clock'event AND (clock = '1')) THEN
         snoopable_burst <= s_is_snoopable_burst;
      END IF;
   END PROCESS make_snoopable_burst;

-- Assign control signals
   s_is_snoopable_burst   <= '1' WHEN begin_transaction_in = '1' AND
                                      address_data_in = "00" AND
                                      burst_size_in = "11111111" ELSE '0';
   s_masked_requests      <= bus_requests AND NOT(s_queued_mask_reg);
   s_queued_mask_next     <= (s_queued_mask_reg AND NOT(s_request_taken_reg)) OR s_to_be_queued_mask( 30 DOWNTO 0) 
                                WHEN s_arbiter_state_reg = REMOVE ELSE
                             s_queued_mask_reg OR s_to_be_queued_mask( 30 DOWNTO 0 );
   s_queue_is_empty       <= '1' WHEN s_queue_insert_pointer_reg = s_queue_remove_pointer_reg ELSE '0';
   s_service_mask         <= s_schedule_mask(30 DOWNTO 0) AND bus_requests;
   s_take_request         <= '1' WHEN s_queue_is_empty = '0' AND
                                      s_service_mask /= c_zero_vector ELSE '0';
   s_masked_request_taken <= bus_requests AND s_request_taken_reg;
   
-- Assign processes
   make_bus_error_reg : PROCESS( clock , reset , s_arbiter_state_reg )
   BEGIN
      IF (reset = '1') THEN s_bus_error_reg             <= '0';
                            s_end_transaction_reg       <= '0';
      ELSIF (clock'event AND (clock = '1')) THEN
         IF (s_arbiter_state_reg = BUS_ERROR AND
             end_transaction_in /= '1') THEN s_bus_error_reg <= '1';
                                        ELSE s_bus_error_reg <= '0';
         END IF;
         IF (s_arbiter_state_reg = END_TRANSACTION) THEN s_end_transaction_reg <= '1';
                                                    ELSE s_end_transaction_reg <= '0';
         END IF;
      END IF;
   END PROCESS make_bus_error_reg;

   make_arbiter_state_reg : PROCESS( clock , reset , s_arbiter_state_reg , s_queue_is_empty , s_take_request , begin_transaction_in , s_bus_time_out_count_reg ,
                                     end_transaction_in ,s_masked_request_taken , s_retry_count_reg)
      VARIABLE v_next_state : ARBITER_TYPE;
   BEGIN
      CASE (s_arbiter_state_reg) IS
         WHEN IDLE            => IF (s_queue_is_empty = '1') THEN v_next_state := IDLE;
                                 ELSIF(s_take_request = '1') THEN v_next_state := GRANT;
                                                             ELSE v_next_state := REMOVE;
                                 END IF;
         WHEN GRANT           => v_next_state := WAIT_BEGIN;
         WHEN WAIT_BEGIN      => IF (begin_transaction_in = '1') THEN v_next_state := SERVICING;
                                 ELSIF (end_transaction_in = '1') THEN v_next_state := REMOVE;
                                 ELSIF (s_bus_time_out_count_reg(11) = '1') THEN
                                    IF (s_masked_request_taken = c_zero_vector) THEN v_next_state := REMOVE;
                                    ELSIF (s_retry_count_reg(2) = '1') THEN v_next_state := BUS_ERROR;
                                                                       ELSE v_next_state := GRANT;
                                    END IF;
                                                                           ELSE v_next_state := WAIT_BEGIN;
                                 END IF;
         WHEN SERVICING       => IF (end_transaction_in = '1') THEN 
                                    IF (s_masked_request_taken = c_zero_vector) THEN v_next_state := REMOVE;
                                                                                ELSE v_next_state := GRANT; -- For atomic transactions
                                    END IF;
                                 ELSIF (s_bus_time_out_count_reg(11) = '1') THEN v_next_state := BUS_ERROR;
                                                                           ELSE v_next_state := SERVICING;
                                 END IF;
         WHEN BUS_ERROR       => IF (end_transaction_in = '1') THEN v_next_state := REMOVE;
                                 ELSIF (s_bus_time_out_count_reg(11) = '1') THEN v_next_state := END_TRANSACTION;
                                                                           ELSE v_next_state := BUS_ERROR;
                                 END IF;
         WHEN END_TRANSACTION => v_next_state := REMOVE;
         WHEN OTHERS          => v_next_state := IDLE;
      END CASE;
      IF (reset = '1') THEN s_arbiter_state_reg <= IDLE;
      ELSIF (clock'event AND (clock = '1')) THEN s_arbiter_state_reg <= v_next_state;
      END IF;
   END PROCESS make_arbiter_state_reg;

   make_request_taken_reg : PROCESS( clock , reset , s_arbiter_state_reg , s_schedule_mask )
   BEGIN
      IF (reset = '1') THEN s_request_taken_reg <= (OTHERS => '0');
      ELSIF (clock'event AND (clock = '1')) THEN
         IF (s_arbiter_state_reg = IDLE) THEN s_request_taken_reg <= s_schedule_mask( 30 DOWNTO 0);
         END IF;
      END IF;
   END PROCESS make_request_taken_reg;

   make_queue_insert_pointer_reg : PROCESS( clock , reset , s_insert_into_queue )
   BEGIN
      IF (reset = '1') THEN s_queue_insert_pointer_reg <= (OTHERS => '0');
      ELSIF (clock'event AND (clock = '1')) THEN
         IF (s_insert_into_queue = '1') THEN 
            s_queue_insert_pointer_reg <= unsigned(s_queue_insert_pointer_reg) + 1;
         END IF;
      END IF;
   END PROCESS make_queue_insert_pointer_reg;
   
   make_queue_remove_pointer_reg : PROCESS( clock , reset , s_queue_is_empty , s_arbiter_state_reg )
   BEGIN
      IF (reset = '1') THEN s_queue_remove_pointer_reg <= (OTHERS => '0');
      ELSIF (clock'event AND (clock = '1')) THEN
         IF (s_arbiter_state_reg = IDLE AND
             s_queue_is_empty = '0') THEN
            s_queue_remove_pointer_reg <= unsigned(s_queue_remove_pointer_reg) + 1;
         END IF;
      END IF;
   END PROCESS make_queue_remove_pointer_reg;
   
   make_queued_mask_reg : PROCESS( clock , reset , s_insert_into_queue , s_queued_mask_next , s_arbiter_state_reg )
   BEGIN
      IF (reset = '1') THEN s_queued_mask_reg <= (OTHERS => '0');
      ELSIF (clock'event AND (clock = '1')) THEN
         IF (s_insert_into_queue = '1' OR
             s_arbiter_state_reg = REMOVE) THEN
            s_queued_mask_reg <= s_queued_mask_next;
         END IF;
      END IF;
   END PROCESS make_queued_mask_reg;
   
   make_bus_time_out_count_reg : PROCESS( reset , clock , begin_transaction_in , end_transaction_in , data_valid_in ,
                                          s_arbiter_state_reg )
   BEGIN
      IF (clock'event AND (clock = '1')) THEN
         IF (begin_transaction_in = '1' OR
             end_transaction_in = '1' OR
             data_valid_in = '1' OR
             s_arbiter_state_reg = IDLE OR
             s_arbiter_state_reg = GRANT OR
             (s_arbiter_state_reg = SERVICING AND
              s_bus_time_out_count_reg(11) = '1') OR
             reset = '1') THEN s_bus_time_out_count_reg <= (OTHERS => '0');
         ELSIF (s_bus_time_out_count_reg(11) = '0') THEN
            s_bus_time_out_count_reg <= unsigned(s_bus_time_out_count_reg) + 1;
         END IF;
      END IF;
   END PROCESS make_bus_time_out_count_reg;

   make_to_be_queued_mask : PROCESS( s_masked_requests )
      VARIABLE v_mask_1 : std_logic_vector( 3 DOWNTO 0 );
      VARIABLE v_mask_2 : std_logic_vector( 3 DOWNTO 0 );
      VARIABLE v_mask_3 : std_logic_vector( 3 DOWNTO 0 );
      VARIABLE v_mask_4 : std_logic_vector( 3 DOWNTO 0 );
      VARIABLE v_mask_5 : std_logic_vector( 3 DOWNTO 0 );
      VARIABLE v_mask_6 : std_logic_vector( 3 DOWNTO 0 );
      VARIABLE v_mask_7 : std_logic_vector( 3 DOWNTO 0 );
      VARIABLE v_mask_8 : std_logic_vector( 2 DOWNTO 0 );
      VARIABLE v_or_1   : std_logic;
      VARIABLE v_or_2   : std_logic;
      VARIABLE v_or_3   : std_logic;
      VARIABLE v_or_4   : std_logic;
      VARIABLE v_or_5   : std_logic;
      VARIABLE v_or_6   : std_logic;
      VARIABLE v_or_7   : std_logic;
      VARIABLE v_or_8   : std_logic;
   BEGIN
      v_mask_1(0) := s_masked_requests(0);
      v_mask_1(1) := s_masked_requests(1) AND NOT(s_masked_requests(0));
      v_mask_1(2) := s_masked_requests(2) AND NOT(s_masked_requests(1)) AND 
                                              NOT(s_masked_requests(0));
      v_mask_1(3) := s_masked_requests(3) AND NOT(s_masked_requests(2)) AND 
                                              NOT(s_masked_requests(1)) AND
                                              NOT(s_masked_requests(0));
      v_or_1      := s_masked_requests(0) OR s_masked_requests(1) OR
                     s_masked_requests(2) OR s_masked_requests(3);
      v_mask_2(0) := s_masked_requests(4);
      v_mask_2(1) := s_masked_requests(5) AND NOT(s_masked_requests(4));
      v_mask_2(2) := s_masked_requests(6) AND NOT(s_masked_requests(5)) AND 
                                              NOT(s_masked_requests(4));
      v_mask_2(3) := s_masked_requests(7) AND NOT(s_masked_requests(6)) AND 
                                              NOT(s_masked_requests(5)) AND
                                              NOT(s_masked_requests(4));
      v_or_2      := s_masked_requests(4) OR s_masked_requests(5) OR
                     s_masked_requests(6) OR s_masked_requests(7);
      v_mask_3(0) := s_masked_requests( 8);
      v_mask_3(1) := s_masked_requests( 9) AND NOT(s_masked_requests( 8));
      v_mask_3(2) := s_masked_requests(10) AND NOT(s_masked_requests( 9)) AND 
                                               NOT(s_masked_requests( 8));
      v_mask_3(3) := s_masked_requests(11) AND NOT(s_masked_requests(10)) AND 
                                               NOT(s_masked_requests( 9)) AND
                                               NOT(s_masked_requests( 8));
      v_or_3      := s_masked_requests( 8) OR s_masked_requests( 9) OR
                     s_masked_requests(10) OR s_masked_requests(11);
      v_mask_4(0) := s_masked_requests(12);
      v_mask_4(1) := s_masked_requests(13) AND NOT(s_masked_requests(12));
      v_mask_4(2) := s_masked_requests(14) AND NOT(s_masked_requests(13)) AND 
                                               NOT(s_masked_requests(12));
      v_mask_4(3) := s_masked_requests(15) AND NOT(s_masked_requests(14)) AND 
                                               NOT(s_masked_requests(13)) AND
                                               NOT(s_masked_requests(12));
      v_or_4      := s_masked_requests(12) OR s_masked_requests(13) OR
                     s_masked_requests(14) OR s_masked_requests(15);
      v_mask_5(0) := s_masked_requests(16);
      v_mask_5(1) := s_masked_requests(17) AND NOT(s_masked_requests(16));
      v_mask_5(2) := s_masked_requests(17) AND NOT(s_masked_requests(17)) AND 
                                               NOT(s_masked_requests(16));
      v_mask_5(3) := s_masked_requests(19) AND NOT(s_masked_requests(18)) AND 
                                               NOT(s_masked_requests(17)) AND
                                               NOT(s_masked_requests(16));
      v_or_5      := s_masked_requests(16) OR s_masked_requests(17) OR
                     s_masked_requests(18) OR s_masked_requests(19);
      v_mask_6(0) := s_masked_requests(20);
      v_mask_6(1) := s_masked_requests(21) AND NOT(s_masked_requests(20));
      v_mask_6(2) := s_masked_requests(22) AND NOT(s_masked_requests(21)) AND 
                                               NOT(s_masked_requests(20));
      v_mask_6(3) := s_masked_requests(23) AND NOT(s_masked_requests(22)) AND 
                                               NOT(s_masked_requests(21)) AND
                                               NOT(s_masked_requests(20));
      v_or_6      := s_masked_requests(20) OR s_masked_requests(21) OR
                     s_masked_requests(22) OR s_masked_requests(23);
      v_mask_7(0) := s_masked_requests(24);
      v_mask_7(1) := s_masked_requests(25) AND NOT(s_masked_requests(24));
      v_mask_7(2) := s_masked_requests(26) AND NOT(s_masked_requests(25)) AND 
                                               NOT(s_masked_requests(24));
      v_mask_7(3) := s_masked_requests(27) AND NOT(s_masked_requests(26)) AND 
                                               NOT(s_masked_requests(25)) AND
                                               NOT(s_masked_requests(24));
      v_or_7      := s_masked_requests(24) OR s_masked_requests(25) OR
                     s_masked_requests(26) OR s_masked_requests(27);
      v_mask_8(0) := s_masked_requests(28);
      v_mask_8(1) := s_masked_requests(29) AND NOT(s_masked_requests(28));
      v_mask_8(2) := s_masked_requests(30) AND NOT(s_masked_requests(29)) AND 
                                               NOT(s_masked_requests(28));
      v_or_8      := s_masked_requests(28) OR s_masked_requests(29) OR
                     s_masked_requests(30);
      s_to_be_queued_mask( 3 DOWNTO 0 ) <= v_mask_1;
      IF (v_or_1 = '0') THEN s_to_be_queued_mask( 7 DOWNTO 4 ) <= v_mask_2;
                        ELSE s_to_be_queued_mask( 7 DOWNTO 4 ) <= X"0";
      END IF;
      IF (v_or_1 = '0' AND
          v_or_2 = '0') THEN s_to_be_queued_mask(11 DOWNTO 8 ) <= v_mask_3;
                        ELSE s_to_be_queued_mask(11 DOWNTO 8 ) <= X"0";
      END IF;
      IF (v_or_1 = '0' AND
          v_or_2 = '0' AND
          v_or_3 = '0') THEN s_to_be_queued_mask(15 DOWNTO 12) <= v_mask_4;
                        ELSE s_to_be_queued_mask(15 DOWNTO 12) <= X"0";
      END IF;
      IF (v_or_1 = '0' AND
          v_or_2 = '0' AND
          v_or_3 = '0' AND
          v_or_4 = '0') THEN s_to_be_queued_mask(19 DOWNTO 16) <= v_mask_5;
                        ELSE s_to_be_queued_mask(19 DOWNTO 16) <= X"0";
      END IF;
      IF (v_or_1 = '0' AND
          v_or_2 = '0' AND
          v_or_3 = '0' AND
          v_or_4 = '0' AND
          v_or_5 = '0') THEN s_to_be_queued_mask(23 DOWNTO 20) <= v_mask_6;
                        ELSE s_to_be_queued_mask(23 DOWNTO 20) <= X"0";
      END IF;
      IF (v_or_1 = '0' AND
          v_or_2 = '0' AND
          v_or_3 = '0' AND
          v_or_4 = '0' AND
          v_or_5 = '0' AND
          v_or_6 = '0') THEN s_to_be_queued_mask(27 DOWNTO 24) <= v_mask_7;
                        ELSE s_to_be_queued_mask(27 DOWNTO 24) <= X"0";
      END IF;
      IF (v_or_1 = '0' AND
          v_or_2 = '0' AND
          v_or_3 = '0' AND
          v_or_4 = '0' AND
          v_or_5 = '0' AND
          v_or_6 = '0' AND
          v_or_8 = '0') THEN s_to_be_queued_mask(30 DOWNTO 28) <= v_mask_8;
                        ELSE s_to_be_queued_mask(30 DOWNTO 28) <= "000";
      END IF;
      s_to_be_queued_mask(31) <= '0';
      s_insert_into_queue <= v_or_1 OR v_or_2 OR v_or_3 OR v_or_4 OR
                             v_or_5 OR v_or_6 OR v_or_7 OR v_or_8;
   END PROCESS make_to_be_queued_mask;
   
   make_retry_count_reg : PROCESS( clock , reset , s_arbiter_state_reg )
   BEGIN
      IF (clock'event AND (clock = '1')) THEN
         IF (s_arbiter_state_reg = IDLE OR
             reset = '1') THEN s_retry_count_reg <= (OTHERS => '0');
         ELSIF (s_arbiter_state_reg = GRANT) THEN
            s_retry_count_reg <= unsigned(s_retry_count_reg) + 1;
         END IF;
      END IF;
   END PROCESS make_retry_count_reg;
   
   -- map components
   queueMem : queueMemory
   PORT MAP ( writeClock   => clock,
              writeEnable  => s_insert_into_queue,
              writeAddress => s_queue_insert_pointer_reg,
              readAddress  => s_queue_remove_pointer_reg,
              writeData    => s_to_be_queued_mask,
              dataReadPort => s_schedule_mask );
END noPlatformSpecific;
