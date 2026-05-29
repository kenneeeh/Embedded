LIBRARY ieee;
USE ieee.std_logic_1164.all;
USE ieee.numeric_std.all;

ENTITY queueMemory IS
   PORT ( writeClock   : IN  std_logic;
          writeEnable  : IN  std_logic;
          writeAddress : IN  std_logic_vector( 4 DOWNTO 0 );
          readAddress  : IN  std_logic_vector( 4 DOWNTO 0 );
          writeData    : IN  std_logic_vector( 31 DOWNTO 0 );
          dataReadPort : OUT std_logic_vector( 31 DOWNTO 0 ) );
END queueMemory;
